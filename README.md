# Battlefield Vietnam engine reconstruction

Recovering the original game's client and dedicated-server logic into readable,
editable C/C++ source, with original gameplay and multiplayer compatibility as
the goal.

## Progress

- Reconstructed the native AI plan interpreter in C.
- Reconstructed bailout logic and its numerical helpers in C.
- Reconstructed AI collision handling, geometry helpers, events and vector insertion in C.
- Reconstructed artillery driver rating selection and cached-target validation in C.
- Added editable plan eligibility, bailout-rating and vehicle-rating rules.
- Built client and server EXE copies that execute the compiled replacement code.
- Passed 23,714 comparisons and ABI checks across the two inspected binaries.
- Validated vector cleanup/rethrow in live Windows client and server processes.
- Exported 62,640 C-like function outputs across client/server; full coverage is still unverified.
- Added binary-analysis tools, function indexes, class/vtable maps and call graphs.

**This is a partial reconstruction.** The current build still needs the original
EXEs and retains most of their engine code. Full AI behavior, artillery target evaluation and firing,
physics, gameplay systems and networking have not yet been reconstructed.

## Build and run

Requires Windows, PowerShell, `uv`, 32-bit MinGW GCC/binutils, and your own matching
Battlefield Vietnam installation. Close generated game processes before rebuilding.

```powershell
git clone https://github.com/LeakorgX/battlefield-vietnam-reconstruction.git
cd battlefield-vietnam-reconstruction
$env:BFV_GAME_DIR = 'D:\Games\Battlefield Vietnam'
.\bfv-native-code\build.ps1
.\bfv-native-code\verify.ps1
.\bfv-native-code\run.ps1
```

For the dedicated server, use `run-server.ps1`.
Edit the C files in `bfv-native-code/src`, then
rebuild. Outputs are `BfVietnam-editable.exe` and `bfvietnam_w32ded-editable.exe`
in your installation. The original EXEs are unchanged.

## Repository

| Folder | Contents |
| --- | --- |
| [bfv-native-code](bfv-native-code/) | Reconstructed C code, build/launch scripts and verification tools |
| [analysis](analysis/) | Binary-analysis and Ghidra export/inspection tools |
| [reports](reports/) | Client/server analysis indexes and verification summaries |
| [docs/SETUP.md](docs/SETUP.md) | Toolchain setup, supported binary hashes and analysis instructions |
| [docs/ANALYSIS.md](docs/ANALYSIS.md) | Current export results and remaining coverage gaps |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Remaining work toward a full source build |
| [docs/VERIFICATION.md](docs/VERIFICATION.md) | Test coverage and its limits |

## Contributing

Clone the repository, create a branch, edit the source, and submit a pull request.
For newly reconstructed functions, document the binary version, calling convention
and object layout, then compare the compiled implementation against the original.
Intentional gameplay changes need their own expected-behavior tests.

Original game binaries, assets and generated decompiler bodies are excluded.
See [NOTICE.md](NOTICE.md) for ownership and license scope.
