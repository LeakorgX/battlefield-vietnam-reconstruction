# Builds build/linkmode/linkmode.exe (the link-mode C rewriter) with 64-bit MinGW.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $root 'build\linkmode'
New-Item -ItemType Directory -Force $out | Out-Null
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"  # cc1plus needs the MinGW DLLs
& C:\msys64\mingw64\bin\g++.exe -std=c++17 -O2 -Wall (Join-Path $PSScriptRoot 'linkmode.cpp') -o "$out\linkmode.exe" -static
if ($LASTEXITCODE) { throw 'linkmode build failed' }
& C:\msys64\mingw64\bin\g++.exe -std=c++17 -O2 -Wall (Join-Path $PSScriptRoot 'relocheck.cpp') -o "$out\relocheck.exe" -static
if ($LASTEXITCODE) { throw 'relocheck build failed' }
