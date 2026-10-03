param([string]$GameDirectory=$env:BFV_GAME_DIR)
$ErrorActionPreference='Stop'
if (-not $GameDirectory) { throw 'Provide -GameDirectory or set BFV_GAME_DIR.' }
$gameWorkspace=(Resolve-Path -LiteralPath $GameDirectory).Path
$editableServer=Join-Path $gameWorkspace 'bfvietnam_w32ded-editable.exe'
if (-not (Test-Path -LiteralPath $editableServer)) { & (Join-Path $PSScriptRoot 'build.ps1') -Target server -GameDirectory $gameWorkspace }
Push-Location $gameWorkspace
try { & $editableServer +game BFVietnam +restart 1 +hostServer 1 }
finally { Pop-Location }
