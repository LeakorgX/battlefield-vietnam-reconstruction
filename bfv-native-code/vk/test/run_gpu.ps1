param([ValidateSet('x86','x64')][string]$Architecture = 'x86', [switch]$NoValidation,
    [switch]$ProductionPlatform, [string]$ValidationLayerPath, [switch]$Fullscreen, [switch]$Bench)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$out = Join-Path $root "build/vulkan-test-$Architecture"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$toolchain = if ($Architecture -eq 'x86') { 'C:/msys64/mingw32/bin' } else { 'C:/msys64/mingw64/bin' }
$gcc = "$toolchain/gcc.exe"
$env:PATH = "$toolchain;$env:PATH"
$machine = if ($Architecture -eq 'x86') { '-m32' } else { '-m64' }
$src = Join-Path $root 'vk'
$platform = if ($ProductionPlatform) { "$src/platform.c" } else { "$src/test/harness_platform.c" }
$inputs = @("$src/test/test_gpu.c", $platform, "$src/d3d9_device.c",
    "$src/d3d9_main.c", "$src/d3d9_resource.c", "$src/d3d9_shader.c", "$src/vk_core.c",
    "$src/dxso.c", "$src/spirv.c", "$src/ff.c")
[string[]]$platformFlags = if ($ProductionPlatform) { @('-DBFV_VKP_TEST_SHIM') } else { @('-ldxguid', '-luuid') }
& $gcc $machine -std=gnu11 -O1 -g -msse2 -mfpmath=sse -Wall -Wextra -Werror -Wno-unused-parameter -ffunction-sections -fdata-sections `
    @inputs @platformFlags '-Wl,--gc-sections' -o "$out/test_gpu.exe" -lgdi32 -luser32
if ($LASTEXITCODE) { throw 'GPU harness compilation failed' }
$previous = [Environment]::GetEnvironmentVariable('vulkan.validation', 'Process')
$oldLayerPath = $env:VK_LAYER_PATH
$oldDisabled = $env:VK_LOADER_LAYERS_DISABLE
$oldRTSS = $env:DISABLE_RTSS_LAYER
$oldReshade = $env:DISABLE_VK_LAYER_reshade_1
$registry = $null
$manifest = $null
$oldRegistryValue = $null
$ownsRegistryValue = $false
try {
    if (!$NoValidation) {
        if (!$ValidationLayerPath) {
            $bits = if ($Architecture -eq 'x86') { '32' } else { '64' }
            $ValidationLayerPath = Join-Path $root "../../bfv-reference-local/vulkan-validation/layers$bits"
        }
        $manifest = (Resolve-Path (Join-Path $ValidationLayerPath 'VkLayer_khronos_validation.json')).Path
        $env:VK_LAYER_PATH = Split-Path $manifest
        $env:VK_LOADER_LAYERS_DISABLE = '~implicit~'
        $env:DISABLE_RTSS_LAYER = '1'
        $env:DISABLE_VK_LAYER_reshade_1 = '1'
        # Elevated Vulkan loaders ignore VK_LAYER_PATH. Register only for this run,
        # retain any previous value and restore it even when the executable fails.
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = [Security.Principal.WindowsPrincipal]::new($identity)
        if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            $view = if ($Architecture -eq 'x86') { [Microsoft.Win32.RegistryView]::Registry32 } else { [Microsoft.Win32.RegistryView]::Registry64 }
            $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, $view)
            $registry = $base.CreateSubKey('SOFTWARE\Khronos\Vulkan\ExplicitLayers')
            $wantedLayer = (Get-Content -Raw -LiteralPath $manifest | ConvertFrom-Json).layer
            $wantedLibrary = [IO.Path]::GetFullPath((Join-Path (Split-Path $manifest) $wantedLayer.library_path))
            $reusedLayer = $false
            foreach ($candidate in $registry.GetValueNames()) {
                if ($registry.GetValue($candidate) -ne 0 -or !(Test-Path -LiteralPath $candidate)) { continue }
                try {
                    $candidateLayer = (Get-Content -Raw -LiteralPath $candidate | ConvertFrom-Json).layer
                    if ($candidateLayer.name -ne $wantedLayer.name) { continue }
                    $candidateLibrary = [IO.Path]::GetFullPath((Join-Path (Split-Path $candidate) $candidateLayer.library_path))
                    if ($candidateLibrary -ieq $wantedLibrary) { $reusedLayer = $true; break }
                } catch { continue }
            }
            if (!$reusedLayer) {
                $oldRegistryValue = $registry.GetValue($manifest, $null)
                $registry.SetValue($manifest, 0, [Microsoft.Win32.RegistryValueKind]::DWord)
                $ownsRegistryValue = $true
            }
            $base.Dispose()
        }
    }
    [Environment]::SetEnvironmentVariable('vulkan.validation', $(if ($NoValidation) { '0' } else { '1' }), 'Process')
    $env:BFV_VK_FULLSCREEN = if ($Fullscreen) { '1' } else { $null }
    $env:BFV_VK_BENCH = if ($Bench) { '1' } else { $null }
    # Windows PowerShell 5.1 turns native stderr lines into error records, which
    # 'Stop' makes fatal; keep them as plain log text and check the exit code.
    $ErrorActionPreference = 'Continue'
    # -ProductionPlatform writes vulkan.log to the working directory.
    Push-Location $out
    & "$out/test_gpu.exe" 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath "$out/gpu.log"
    $exitCode = $LASTEXITCODE
    Pop-Location
    $ErrorActionPreference = 'Stop'
    if ($exitCode) { throw "GPU harness failed: $exitCode" }
} finally {
    if ($registry) {
        if ($ownsRegistryValue) {
            if ($null -eq $oldRegistryValue) { $registry.DeleteValue($manifest, $false) }
            else { $registry.SetValue($manifest, $oldRegistryValue, [Microsoft.Win32.RegistryValueKind]::DWord) }
        }
        $registry.Dispose()
    }
    $env:VK_LAYER_PATH = $oldLayerPath
    $env:VK_LOADER_LAYERS_DISABLE = $oldDisabled
    $env:DISABLE_RTSS_LAYER = $oldRTSS
    $env:DISABLE_VK_LAYER_reshade_1 = $oldReshade
    $env:BFV_VK_FULLSCREEN = $null
    $env:BFV_VK_BENCH = $null
    [Environment]::SetEnvironmentVariable('vulkan.validation', $previous, 'Process')
}
