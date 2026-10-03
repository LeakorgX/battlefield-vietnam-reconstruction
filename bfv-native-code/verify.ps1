param([string]$GameDirectory=$env:BFV_GAME_DIR)
$ErrorActionPreference='Stop'
$uvCommand=Get-Command uv.exe -ErrorAction SilentlyContinue
$uvTool=if ($uvCommand) { $uvCommand.Source } else { Join-Path $env:LOCALAPPDATA 'hermes\bin\uv.exe' }
if (-not (Test-Path -LiteralPath $uvTool)) { throw 'Install uv and place it on PATH.' }
if (-not $GameDirectory) { throw 'Provide -GameDirectory or set BFV_GAME_DIR.' }
& $uvTool run --with pefile --with unicorn python (Join-Path $PSScriptRoot 'tools\verify.py') --game-dir $GameDirectory
if ($LASTEXITCODE -ne 0) { throw "Native comparison failed ($LASTEXITCODE)" }
