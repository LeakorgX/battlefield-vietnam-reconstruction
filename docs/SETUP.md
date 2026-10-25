# Setup and local regeneration

## Native build and static recompilation

Windows is required for launching and inspecting the native game. Supply your own
installation with the inspected client/server hashes listed below.

Required tools:

- Python via `uv` on PATH. The scripts request `pefile`, `capstone` and `unicorn`
  through uv.
- 32-bit MinGW GCC/binutils. The tested directory is
  `C:/msys64/mingw32/bin`, targeting `i686-w64-mingw32`.
- PowerShell.
- For the Vulkan renderer (`--mods vulkan`): `mingw-w64-i686-vulkan-headers`.
  Its GPU tests also use the x64 compiler, `mingw-w64-x86_64-spirv-tools` and
  validation layers; see [VULKAN.md](VULKAN.md), Testing.

The project does not install these tools or change registry/firewall settings.
Provide a different toolchain directory with `build.ps1 -CompilerBin ...` if
necessary. A 64-bit compiler is not a substitute for the recovered x86 ABI.

```powershell
$env:BFV_GAME_DIR='D:\Games\Battlefield Vietnam'
.\bfv-native-code\build.ps1
.\bfv-native-code\recomp.ps1
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
| BfVietnam.exe | `32aec85d35049b8f4ecbd4dd9fa7f580a62c4a20cb42ab70e3656744fc15c0ea` |
| bfvietnam_w32ded.exe | `86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d` |

These identify the inspected binaries. The existing runtime tests used an
installation with modified assets and a custom map; pristine vanilla equivalence
has not yet been validated.

The client is a newer patched build of the reference client
`79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5`, from which
the C, the catalogs and `reports/client/all-functions.tsv` were made. It keeps
the reference layout and adds code: the patch sections `.rofcap`, `.altfire`,
`.thrfire`, `.vzoom`, `.seats10` and `.fcam`, a larger `.ctfsnd`, an entry stub
in the raw slack after `.text`, and in-place changes to 44 indexed functions.
`bfv-native-code/rebase.json` lists every differing byte run and what the build
keeps native because of it (see below). Reports and audits made from the
reference image (Ghidra exports, `live-exceptions.json`) remain evidence for
that image only.

### Rebasing onto a newer build

A later build with the same section layout is supported without re-lifting:

```powershell
uv run --with pefile python .\bfv-native-code\tools\rebase.py --target client `
  --reference <reference BfVietnam.exe> --exe <new BfVietnam.exe>
```

writes `rebase.json`. Then replace the reference hash with the new one in
`TARGETS` (`tools/build.py`) and in each catalog's `original_sha256` (the nine
`bfv-native-code/*.json` catalogs and `lifted|readable/<target>/catalog.json`),
rerun `verify_lifted.py` for both trees, build, `verify.ps1` and `recomp.ps1`.
The build keeps native every lifted entry whose ranges touch a changed run, and
every hand-written or catalogued entry and vtable patch inside an indexed
function with changed bytes. `verify_lifted.py` reports those lifted entries as
`native`, and `verify.py` skips (or runs without those cases) the checks of the
hand-written entries kept native. `build.py` and `verify_lifted.py` recompute
the lists from `changed`, so a stale `rebase.json` stops them. Patch code
outside `.text` counts in the detour audit: its branches and address fields
block lifted detours like those of `.text`, and a jump into the guard of a
hand-written entry stops the build.

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
