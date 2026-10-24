# Battlefield Vietnam Reconstruction

Battlefield Vietnam (2004), client and dedicated server, rebuilt as C so the game
logic can be read, modded and recompiled.

You need your own copy of the game. This repo has **no game assets or original EXEs**.

## Progress

| Stage | What it means | Client | Server |
| --- | --- | ---: | ---: |
| **L1: lifted C** | Every function translated to C mechanically and checked against the original. Works, but looks like assembly. | **99.0%** | **98.6%** |
| **L2: readable C** | Same functions as compact C: conditions as `if`, dead flag code removed, nested `if`/`else`/`while` with `break`/`continue` ([READABLE-C](docs/READABLE-C.md)) | 33,697 of 33,731 verified | 20,458 of 20,473 verified |
| **L2b: named C** | Values, stack slots, addresses and known fields named (values survive calls; a call returns into the C body); calls written with their arguments, typed where the callee ABI is known (`r = THISCALL(...)`); `this->field` for known class layouts | 187,281 variables, all other register values named; 18,977 address names; 57,255 named stack slots; 1,095 fields; 93,652 calls with arguments (41,686 typed); 100 class members | 104,613 variables; 11,192 address names; 32,812 named stack slots; 665 fields; 51,917 calls with arguments (24,238 typed); 66 class members |
| **L3: readable C/C++** | Named classes, fields and functions | 8,121 functions | (shared count) |

- The game boots and runs with the lifted C in place: the server ran 180 s and the client 90 s with no faults.
- Whole-game goal (fully readable source with no original code): **0 of 7 milestones**. See [PROGRESS](docs/PROGRESS.md).

![Code converted to readable C](reports/c-progress.svg)

## How it works

1. `lift_c.py` translates each function of the EXE into C.
2. `verify_lifted.py` runs the original and the C side by side in an emulator and compares every memory access and call.
3. `build.py` compiles the verified C and patches it into a copy of the EXE. The original files are never touched.

## Quick start

Requirements: Windows, [uv](https://docs.astral.sh/uv/), 32-bit MinGW (`C:\msys64\mingw32\bin`), and the game version listed in [SETUP](docs/SETUP.md).

```powershell
$env:BFV_GAME_DIR = 'C:\path\to\Battlefield Vietnam'
.\bfv-native-code\build.ps1     # build the editable EXEs
.\bfv-native-code\run.ps1       # play (run-server.ps1 for the server)
```

To mod, write a hook in plain C in `bfv-native-code/mods/`. It runs before, after or instead of any game function that runs as C, can do float math, and can call game code. Then rebuild. The game code itself is in `bfv-native-code/readable/<target>/`, sorted into the game's own folders (e.g. `bf/ai/`, `ref2/world/BFSoldier.c`). `tools/find.py` searches it by string, class or address and prints a hook skeleton (`--hook`). `CONSOLE.tsv` maps all ~2,250 console/script commands (like `ObjectTemplate.repairFactor`) to their code. See **[MODDING.md](docs/MODDING.md)**.

To play a mod, build it with `--mods-only`: the game stays original code and only the hooked functions run as C (`mods/shared/example_repair_factor.c` is a small example):

```powershell
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/build.py --tree readable --mods-only --mods example_repair_factor --target client --output "$env:BFV_GAME_DIR\BfVietnam-mod.exe"
```

## Folders

| Path | Contents |
| --- | --- |
| `bfv-native-code/lifted` | Lifted C (L1), in the game's original folder/file names |
| `bfv-native-code/readable` | Readable C (L2), same folders and functions as `lifted` |
| `bfv-native-code/src` | Readable C (L3) |
| `bfv-native-code/mods` | Mod API (`include/bfv_mod.h`), mod runtime, hook tests and an example mod |
| `bfv-native-code/notes` | Hand notes and shared names merged into the readable C |
| `bfv-native-code/tools` | Lifter, verifier, build and recompiler |
| `docs` | [Lifted C](docs/LIFTED-C.md), [Readable C](docs/READABLE-C.md), [Static recomp](docs/STATIC-RECOMP.md), [Handoff](docs/HANDOFF.md) |

See [NOTICE.md](NOTICE.md) for legal details.
