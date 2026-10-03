param([string]$Connect = '', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if (-not $SkipBuild) {
        & cargo build --locked -p bfv-server -p bfv-client
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    }
    $clientPath = Join-Path $PSScriptRoot 'target/debug/bfv-client.exe'
    if ($Connect) { & $clientPath --connect $Connect }
    else { & $clientPath --host }
} finally { Pop-Location }

