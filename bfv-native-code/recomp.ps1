param([ValidateSet('client','server','both')][string]$Target='both',
      [string]$GameDirectory=$env:BFV_GAME_DIR,
      [string]$CompilerBin='C:\msys64\mingw32\bin',
      [switch]$AssembleOnly,
      [switch]$Install)
# Static recompilation. Without -AssembleOnly, regenerates build\<target>-recomp\*.s
# from the image (overwriting hand edits). With -AssembleOnly, assembles the existing
# .s files. -Install copies the result over the editable EXE in the game directory.
$ErrorActionPreference='Stop'
$uvCommand=Get-Command uv.exe -ErrorAction SilentlyContinue
if (-not $uvCommand) { throw 'Install uv and place it on PATH.' }
if (-not $GameDirectory) { throw 'Provide -GameDirectory or set BFV_GAME_DIR to your game installation.' }
$env:MINGW32_BIN=$CompilerBin
$arguments=@('run','--with','pefile','--with','capstone','python',
             (Join-Path $PSScriptRoot 'tools\static_recomp.py'),
             '--target',$Target,'--game-dir',$GameDirectory)
if ($AssembleOnly) { $arguments+='--assemble-only' }
if ($Install) { $arguments+='--install' }
& $uvCommand.Source @arguments
if ($LASTEXITCODE -ne 0) { throw "Static recompilation failed ($LASTEXITCODE)" }
