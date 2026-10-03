param(
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [Parameter(Mandatory=$true)][string]$GhidraHome,
    [Parameter(Mandatory=$true)][string]$ReferenceDirectory,
    [ValidateSet('client','server','both')][string]$Target='both'
)
$ErrorActionPreference='Stop'
$sourceRoot=[System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$referenceRoot=[System.IO.Path]::GetFullPath($ReferenceDirectory)
if ($referenceRoot.Equals($sourceRoot,[StringComparison]::OrdinalIgnoreCase) -or
    $referenceRoot.StartsWith($sourceRoot+[System.IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'ReferenceDirectory must be outside the source repository.'
}
$launcher=Join-Path $GhidraHome 'support\analyzeHeadless.bat'
if (-not (Test-Path -LiteralPath $launcher)) { throw 'Ghidra headless launcher not found.' }
$projectDir=Join-Path $referenceRoot 'ghidra-projects'
New-Item -ItemType Directory -Path $projectDir -Force | Out-Null
$targets=@(
    @{Name='client';File='BfVietnam.exe';Project='BFVBinary'},
    @{Name='server';File='bfvietnam_w32ded.exe';Project='BFVServerBinary'}
)
foreach ($item in $targets) {
    if ($Target -ne 'both' -and $Target -ne $item.Name) { continue }
    $exe=Join-Path $GameDirectory $item.File
    if (-not (Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }
    $output=Join-Path $referenceRoot $item.Name
    New-Item -ItemType Directory -Path $output -Force | Out-Null
    & uv run --with capstone --with pefile python (Join-Path $PSScriptRoot 'tools\binary_logic.py') --exe $exe --output $output
    if ($LASTEXITCODE -ne 0) { throw 'Binary seed extraction failed.' }
    $arguments=@($projectDir,$item.Project)
    if (Test-Path -LiteralPath (Join-Path $projectDir ($item.Project+'.gpr'))) {
        $arguments+=@('-process',$item.File,'-noanalysis')
    } else {
        $arguments+=@('-import',$exe)
    }
    $arguments+=@('-scriptPath',(Join-Path $PSScriptRoot 'tools\ghidra'),
        '-postScript','AuditNativeCoverage.java',(Join-Path $output 'coverage-before'),
        '-postScript','DiscoverNativeCallTargets.java',(Join-Path $output 'call-discovery'),
        '-postScript','DiscoverNativePointerTargets.java',(Join-Path $output 'pointer-discovery'),
        '-postScript','DiscoverOrphanPrologues.java',(Join-Path $output 'orphan-discovery'),
        '-postScript','AuditNativeCoverage.java',(Join-Path $output 'coverage-after'),
        '-postScript','ExportNativeLogic.java',(Join-Path $output 'decompiled'),(Join-Path $output 'function-seeds.tsv'),'all','60',
        '-log',(Join-Path $output 'analysis.log'))
    & $launcher @arguments
    if ($LASTEXITCODE -ne 0) { throw "Ghidra failed for $($item.Name)." }
    $summary=Join-Path $output 'decompiled\export-summary.txt'
    if (-not (Test-Path -LiteralPath $summary)) { throw 'Export did not produce its completion summary.' }
    Get-Content -LiteralPath $summary
}
