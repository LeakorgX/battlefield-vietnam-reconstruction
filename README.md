# Battlefield Vietnam static recompilation

A static recompilation of the original Battlefield Vietnam client
(`BfVietnam.exe`) and dedicated server (`bfvietnam_w32ded.exe`). Each executable
is rebuilt from assembler source plus hand-reconstructed C. The output is
byte-identical to the verified build, so any part of the game can be edited and
rebuilt.

![Code converted to C](reports/c-progress.svg)

The bar is the share of the original code (bytes of every function Ghidra
indexed) that now comes from verified C. Run
`python analysis/tools/update_progress.py` to refresh it.

![Whole-game completion](reports/progress.svg)

**Whole-game completion: 0% (0 of 7 acceptance milestones).** 100% means the
entire original client and server are readable, editable source that builds
without copying code from the original EXEs. See [PROGRESS](docs/PROGRESS.md).

## How it works

```
original EXE ──► static_recomp.py ──► build/<target>-recomp/text.s, data.s, rdata.s, ...
                                                 │ (editable assembly)
src/*.c ──► build.py ──► payload.bin ────────────┤ (reconstructed C, linked in)
                                                 ▼
                                    GNU as ──► BfVietnam-editable.exe
                                               bfvietnam_w32ded-editable.exe
```

- **Assembly layer.** The whole `.text` section is emitted as GNU as
  Intel-syntax source with symbolic control flow: `sub_XXXXXXXX` function
  labels, `loc_XXXXXXXX` branch targets, and calls into reconstructed C by name.
  The output reassembles byte-exact.
- **C layer.** 8,121 routines (client plus server) are reconstructed as readable C in
  [`bfv-native-code/src`](bfv-native-code/src), including 4,407 C++ static-object
  initializers, 954 constant-return stubs, 889 field/global accessors, 691
  scalar deleting destructors, 544 `dynamic_cast` wrappers, 357 virtual clones
  and 175 field getters. The rest cover the AI plan interpreter, bailout,
  collision, events and large parts of the artillery evaluator.
  Each is verified side by side against the original under emulation. The
  originals jump into this code through guarded detours.
- **L1 lifted C.** A mechanical translator turns the remaining functions into
  C (not yet readable). Each lifted function is checked against the original
  under emulation, and only verified ones are switched on. Today about 73% of
  client and 80% of server functions run as C. See
  [docs/LIFTED-C.md](docs/LIFTED-C.md).
- **Gate.** Every build must be byte-identical to the verified editable EXE
  (or differ only where you edited it).

| Target | Instructions | Symbolic branches | C detours | Raw `.byte` lines |
| --- | ---: | ---: | ---: | ---: |
| Client | 2,240,682 | 359,407 | 4,691 | 986 |
| Server | 1,275,735 | 194,095 | 3,466 | 448 |

Details and limits: [docs/STATIC-RECOMP.md](docs/STATIC-RECOMP.md).

## Build and run

Requires Windows, PowerShell, [`uv`](https://docs.astral.sh/uv/), 32-bit MinGW
GCC/binutils (`C:\msys64\mingw32\bin`) and your own Battlefield Vietnam
installation matching the hashes in [SETUP](docs/SETUP.md). Close the game
before rebuilding.

```powershell
cd battlefield-vietnam-reconstruction
$env:BFV_GAME_DIR = 'X:\Battlefield Vietnam - Copy'

.\bfv-native-code\build.ps1                  # compile reconstructed C, write editable EXEs
.\bfv-native-code\recomp.ps1                 # regenerate the assembly, check byte identity
.\bfv-native-code\verify.ps1                 # original-vs-C instruction comparisons
.\bfv-native-code\run.ps1                    # client   (run-server.ps1 for the server)
```

### Editing

- **C:** edit files in `bfv-native-code/src`, then run `build.ps1` and `recomp.ps1`.
- **Assembly:** edit `bfv-native-code/build/<target>-recomp/text.s`, then run
  `recomp.ps1 -AssembleOnly -Install`. Edits must keep each section the same
  size, because addresses in data are still absolute. Running `recomp.ps1`
  without `-AssembleOnly` regenerates the assembly and **overwrites** hand edits.

The original EXEs are never modified.

## Repository

| Path | Contents |
| --- | --- |
| [bfv-native-code](bfv-native-code/) | Static recompiler, reconstructed C, build/run/verify scripts |
| [analysis](analysis/) | Ghidra export, audit and progress tools |
| [reports](reports/) | Function indexes, call graphs, verification summaries |
| [docs/STATIC-RECOMP.md](docs/STATIC-RECOMP.md) | Recompiler design, edit workflow, limits |
| [docs/SETUP.md](docs/SETUP.md) | Toolchain, supported hashes, analysis setup |
| [docs/VERIFICATION.md](docs/VERIFICATION.md) | Test coverage and its limits |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Remaining work toward a standalone source build |
| [docs/modules](docs/modules/) | Notes on each reconstructed subsystem |
| [docs/HANDOFF.md](docs/HANDOFF.md) | Current state for whoever continues the work |

## Status and limits

- The assembly is a faithful listing of the original machine code, not
  decompiled C. Only the routines in `src/` are readable C.
- Data references and jump tables are still numeric addresses, so code cannot
  yet grow or shrink. Symbolizing them is the next milestone.
- The build still uses the original EXE's headers and data bytes as input.
- Physics, gameplay, networking and engine services are not reconstructed as C.

Original binaries, assets and generated assembly or decompiler output are never
committed. See [NOTICE.md](NOTICE.md).
