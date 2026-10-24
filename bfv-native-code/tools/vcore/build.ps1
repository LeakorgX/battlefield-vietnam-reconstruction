# Builds build/vcore/vcore.exe (the C++ verifier core, verify_lifted.py --engine cpp)
# with 64-bit MinGW, against the unicorn and capstone the Python packages ship.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $root 'build\vcore'
New-Item -ItemType Directory -Force $out | Out-Null
$site = uv run --with capstone --with unicorn python -c "import unicorn, capstone, os; print(os.path.dirname(os.path.dirname(unicorn.__file__)))"
Copy-Item "$site\unicorn\lib\unicorn.dll", "$site\capstone\lib\capstone.dll" $out
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"  # cc1plus needs the MinGW DLLs
& C:\msys64\mingw64\bin\g++.exe -std=c++17 -O2 -Wall "-I$site\unicorn\include" "-I$site\capstone\include" `
    (Join-Path $PSScriptRoot 'vcore.cpp') -o "$out\vcore.exe" "$out\unicorn.dll" "$out\capstone.dll" -static
if ($LASTEXITCODE) { throw 'vcore build failed' }
