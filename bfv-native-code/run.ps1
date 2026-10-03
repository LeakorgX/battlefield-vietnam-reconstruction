param([string]$GameDirectory=$env:BFV_GAME_DIR)
$ErrorActionPreference='Stop'
if (-not $GameDirectory) { throw 'Provide -GameDirectory or set BFV_GAME_DIR.' }
$gameWorkspace=(Resolve-Path -LiteralPath $GameDirectory).Path
$editableGame=Join-Path $gameWorkspace 'BfVietnam-editable.exe'
if (-not (Test-Path -LiteralPath $editableGame)) { & (Join-Path $PSScriptRoot 'build.ps1') -Target client -GameDirectory $gameWorkspace }
Push-Location $gameWorkspace
try { & $editableGame +game BFVietnam +restart 1 }
finally { Pop-Location }
