param([ValidateSet('client','server','both')][string]$Target='both',
      [string]$GameDirectory=$env:BFV_GAME_DIR,
      [string]$CompilerBin='C:\msys64\mingw32\bin')
$ErrorActionPreference='Stop'
$modProjectDir=$PSScriptRoot
$env:PATH=$CompilerBin+';'+$env:PATH
$uvCommand=Get-Command uv.exe -ErrorAction SilentlyContinue
$uvTool=if ($uvCommand) { $uvCommand.Source } else { Join-Path $env:LOCALAPPDATA 'hermes\bin\uv.exe' }
if (-not (Test-Path -LiteralPath $uvTool)) { throw 'Install uv and place it on PATH.' }
if (-not $GameDirectory) { throw 'Provide -GameDirectory or set BFV_GAME_DIR to your game installation.' }
& $uvTool run --with pefile python (Join-Path $modProjectDir 'tools\build.py') --target $Target --game-dir $GameDirectory --compiler-bin $CompilerBin
if ($LASTEXITCODE -ne 0) { throw "Native build failed ($LASTEXITCODE)" }
