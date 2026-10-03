param([string]$Bind = '0.0.0.0:23000', [int]$Bots = 8, [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if (-not $SkipBuild) {
        & cargo build --locked -p bfv-server
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    }
    & (Join-Path $PSScriptRoot 'target/debug/bfv-server.exe') --bind $Bind --bots $Bots
} finally { Pop-Location }

