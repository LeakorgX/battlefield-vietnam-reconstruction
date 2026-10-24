# Builds build/textcheck/textcheck.exe (the instruction-level gate for edited
# .text) with 64-bit MinGW, against the capstone the Python package ships.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $root 'build\textcheck'
New-Item -ItemType Directory -Force $out | Out-Null
$site = uv run --with capstone python -c "import capstone, os; print(os.path.dirname(os.path.dirname(capstone.__file__)))"
Copy-Item "$site\capstone\lib\capstone.dll" $out
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"  # cc1plus needs the MinGW DLLs
& C:\msys64\mingw64\bin\g++.exe -std=c++17 -O2 -Wall "-I$site\capstone\include" `
    (Join-Path $PSScriptRoot 'textcheck.cpp') -o "$out\textcheck.exe" "$out\capstone.dll" -static
if ($LASTEXITCODE) { throw 'textcheck build failed' }
