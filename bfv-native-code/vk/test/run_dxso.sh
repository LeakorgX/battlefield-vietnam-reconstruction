#!/bin/sh
# Build the translator test, translate the corpus, validate every module.
set -e
cd "$(dirname "$0")"
PATH=/c/msys64/mingw32/bin:$PATH
gcc -m32 -std=c11 -O1 -g -Wall -Wextra -o test_dxso.exe test_dxso.c harness_platform.c ../dxso.c ../spirv.c ../ff.c
rm -rf out
./test_dxso.exe
bad=0
for f in out/*.spv; do
    if ! /c/msys64/mingw64/bin/spirv-val --target-env vulkan1.3 "$f" > out/val.txt 2>&1; then
        echo "INVALID $f"; head -5 out/val.txt; bad=$((bad + 1))
    fi
done
echo "spirv-val: $bad invalid"
[ "$bad" = 0 ]
