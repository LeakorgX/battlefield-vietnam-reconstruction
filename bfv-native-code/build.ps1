param([ValidateSet('client','server','both')][string]$Target='both',
      [string]$GameDirectory=$env:BFV_GAME_DIR,
      [string]$CompilerBin='C:\msys64\mingw32\bin',
      # readable/ (L2, the tree to edit) or lifted/ (L1); mods/<target>/*.c need readable (lifted leaves them out)
      [ValidateSet('readable','lifted')][string]$Tree='readable',
      # write the executable here instead of the game folder (one target only)
      [string]$Output='')
$ErrorActionPreference='Stop'
$modProjectDir=$PSScriptRoot
$env:PATH=$CompilerBin+';'+$env:PATH
$uvCommand=Get-Command uv.exe -ErrorAction SilentlyContinue
$uvTool=if ($uvCommand) { $uvCommand.Source } else { Join-Path $env:LOCALAPPDATA 'hermes\bin\uv.exe' }
if (-not (Test-Path -LiteralPath $uvTool)) { throw 'Install uv and place it on PATH.' }
if (-not $GameDirectory) { throw 'Provide -GameDirectory or set BFV_GAME_DIR to your game installation.' }
$extra=@(); if ($Output) { $extra=@('--output',$Output) }
& $uvTool run --with pefile --with capstone python (Join-Path $modProjectDir 'tools\build.py') --target $Target --game-dir $GameDirectory --compiler-bin $CompilerBin --tree $Tree @extra
if ($LASTEXITCODE -ne 0) { throw "Native build failed ($LASTEXITCODE)" }
