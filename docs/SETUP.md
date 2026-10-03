# Setup and local regeneration

## Current native build

Windows is required for launching and inspecting the native game. Supply your own
installation with the inspected client/server hashes listed below.

Required tools:

- Python via `uv` on PATH. The scripts request `pefile` and `unicorn` through uv.
- 32-bit MinGW GCC/binutils. The tested directory is
  `C:/msys64/mingw32/bin`, targeting `i686-w64-mingw32`.
- PowerShell.

The project does not install these tools or change registry/firewall settings.
Provide a different toolchain directory with `build.ps1 -CompilerBin ...` if
necessary. A 64-bit compiler is not a substitute for the recovered x86 ABI.

```powershell
$env:BFV_GAME_DIR='D:\Games\Battlefield Vietnam'
.\bfv-native-code\build.ps1
.\bfv-native-code\verify.ps1
.\bfv-native-code\run.ps1
```

Or pass `-GameDirectory` to each script. Build outputs go into that installation;
objects/payloads/manifests/verification JSON go under `bfv-native-code/build` and
are ignored by Git. Close generated game processes before rebuilding.

Only the two inspected executable hashes are supported. On a different release,
the build must stop; port and verify the address map/ABI before adding support.
No code silently patches an unknown executable version.

## Local native analysis

Ghidra 12.1.4 with JDK 21 was used. Keep Ghidra projects, raw C exports and assembly
listings outside the repository. The Java scripts accept explicit output paths.

Example RTTI/function-seed regeneration:

```powershell
$game='D:\Games\Battlefield Vietnam'
$reference='D:\BFV-local-research'
uv run --with capstone --with pefile python .\analysis\tools\binary_logic.py `
  --exe "$game\BfVietnam.exe" --output "$reference\client"
uv run --with capstone --with pefile python .\analysis\tools\binary_logic.py `
  --exe "$game\bfvietnam_w32ded.exe" --output "$reference\server"
```

Import each executable into a separate Ghidra project and run analysis. The
headless post-script `ExportNativeLogic.java OUTPUT_DIR FUNCTION_SEEDS_TSV all`
exports detected internal functions and direct call relationships. Omitting `all`
restricts it to AI/gameplay seeds plus direct callees. Existing exports are reused.

`InspectNativeFunctions.java OUTPUT_DIR ADDRESS...` preserves instruction evidence
and inferred signatures for review. `RetryNativeDecompile.java` makes one targeted
retry without read-only constant-pointer assumptions. A failed retry remains a
failure; it is not treated as recovered source.

See `analysis/tools/ghidra/README.md` for the workflow. Run Ghidra's headless
launcher from your installed Ghidra directory, passing your project directory,
executable path, script path and reference output explicitly. The original engine
code is not modified by those read-only analysis/export tools.

## Reference oracles and reports

`bfv-native-code/tools/native_oracle.py` supplies the controlled fixtures used by
the compiled-code comparison. `analysis/tools/native_oracle.py` is the earlier
original-instruction-only oracle. Their scopes differ; do not conflate the counts.

`analysis/tools/native_report.py --root ...` creates a local navigation index
linking recovered classes to generated function exports. Its local absolute links
are intentionally not copied into public documentation.

## Supported executables

| Executable | SHA256 |
| --- | --- |
| BfVietnam.exe | `79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5` |
| bfvietnam_w32ded.exe | `86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d` |

These identify the inspected binaries. The existing runtime tests used an
installation with modified assets and a custom map; pristine vanilla equivalence
has not yet been validated.

## Complete known-function export pipeline

With `uv` on PATH and JDK 21 configured for Ghidra:

```powershell
.\analysis\Run-Analysis.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam' `
  -GhidraHome 'D:\Tools\ghidra_12.1.4_PUBLIC' `
  -ReferenceDirectory 'D:\BFV-local-research'
```

The pipeline saves inferred function candidates in local Ghidra projects. It
reuses existing exports; remove or relocate a local export before explicitly
requesting a fresh decompilation of that function. It does not reconstruct a
standalone engine or validate candidate signatures. See ANALYSIS.md for the
current counts, failed entries and coverage gaps.
