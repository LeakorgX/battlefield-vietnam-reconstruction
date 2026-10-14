# Handoff

Updated **2026-10-14, Asia/Baghdad**. Start here when resuming. The full
chronological log up to this point is in [archive/HANDOFF-LOG.md](archive/HANDOFF-LOG.md).

**User direction: commit locally when asked; the owner pushes.** The lifted C is
committed (owner's decision, 2026-10-14). Current focus: make modding easier
without breaking verified work (see [MODDING.md](MODDING.md)). Keep original binaries, assets, generated assembly and private
analysis out of the repository.

## Current state

The project is now a **static recompilation**. See [STATIC-RECOMP.md](STATIC-RECOMP.md).

- `bfv-native-code/tools/static_recomp.py` (run via `recomp.ps1`) emits the whole
  `.text` of each executable as symbolic GNU as source, emits the data sections
  as byte streams, links the reconstructed-C payload, and checks byte identity
  with the installed editable EXEs.
- Verified on Windows 2026-10-13 (`recomp.ps1`, regenerate and `-AssembleOnly`):
  both targets byte-identical, 0 diffs.
  Client 2,240,682 instructions, 359,407 symbolic branches, 4,691 detours, 986
  `.byte` lines. Server 1,275,735 / 194,095 / 3,466 / 448.
- `--assemble-only` / `-AssembleOnly` builds from hand-edited `.s` without
  regenerating; `--install` / `-Install` writes the EXE. Size-changing edits are
  rejected because references are still absolute.
- Reconstructed C: 8,121 routines (inventory in `reports/recovery-registry.json`),
  1.46% of client and 2.08% of server indexed code bytes (`reports/c-progress.svg`,
  refreshed by `analysis/tools/update_progress.py`). New on 2026-10-13: 889
  field/global accessors (42,672 focused comparisons) and 3,947 C++ static-object
  initializers (15,788 focused comparisons); then 691 scalar deleting destructors
  (4,146), 460 initializers with constructor arguments (1,840), 544 `dynamic_cast`
  wrappers (3,264) and 357 virtual clones (2,142), see `docs/modules/`.
  The latest full regression (entry-bridge checkpoint, 2026-10-10) passed
  48,056 client + 46,472 server comparisons, plus 8 live executions per target;
  rerun `verify.ps1` to confirm it against the new payload.
- **L1 lifted C** (user goal: working C for modding first, readability later; see
  [LIFTED-C.md](LIFTED-C.md)). `tools/lift_c.py` translates every indexed function
  into mechanical C, and `tools/verify_lifted.py` compares each one against the
  original under emulation. `build.py` detours only verified, unchanged and
  instruction-aligned functions. Installed build (v14 lift, VERIFIER 13): client 33,519
  and server 20,381 lifted functions run as C. "Runs as C": client 38,409/38,657
  (98.32%), server 23,877/23,986 (98.74%) functions. Live exception probes pass;
  the dedicated server runs 180 s and the client 90 s under `crashdbg` with no faults.
  The lifted C is sorted into the developers' folders and files by
  `tools/sort_lifted.py` (client 75%, server 82% placed by evidence, the rest in
  `unsorted/`).
  Known-good EXEs: `X:\temp\bfvscan\lift\good-v7` (earlier: `good-v6`, `good-v5`).
- Whole-game acceptance: **0 of 7** milestones (`reports/project-milestones.json`).

Installed editable EXE hashes (match `build/<target>/manifest.json`):

```text
client 7be136229d07cf1b9f300db0847b56f0da814f6995cbacd7536536e5e89e0f7c
server afd6d821906819919881f9ff7de5475ee965e264b342bb7742fe0135f9cff779
```

## Pending

0. L1 remaining gaps (see [LIFTED-C.md](LIFTED-C.md)): unverified functions
   (no random state completes natively, even with the 400k budget), failures
   that read stale return addresses from dead stack through random pointers,
   entries blocked by the detour audit (interior branch targets, misaligned
   index entries), `stack_untracked` CRT SEH-prolog functions, and code in
   the client's `.ctfsnd` section outside `.text`.
1. Symbolize absolute references (image-pointing immediates and displacements,
   jump tables, vtables, pointers in data); emit labelled data sections; then
   allow size-changing edits and relinking.
2. Continue C reconstruction where it unlocks milestones. The next artillery
   step is the candidate-search interior (client `0099f3d5..0099f6c3`), then the
   394-byte weapons interval `0099f8d7..0099fa61`. Follow the evidence rules in
   [AGENTS.md](../AGENTS.md).

## Commands

Run from the repository root in PowerShell, with `$env:BFV_GAME_DIR` set.

```powershell
.\bfv-native-code\build.ps1                       # C payload + editable EXEs
.\bfv-native-code\recomp.ps1                      # regenerate assembly, check identity
.\bfv-native-code\recomp.ps1 -AssembleOnly        # build from edited .s
.\bfv-native-code\verify.ps1 -Target client       # full regression (also server)
uv run --with pefile --with capstone python bfv-native-code/tools/verify_live_exceptions.py --game-dir $env:BFV_GAME_DIR --target both
C:\Python314\python.exe bfv-native-code/tools/export_reports.py
C:\Python314\python.exe analysis/tools/update_progress.py
```

Ghidra (read-only) usage is in `analysis/tools/ghidra/README.md`. Private
checkpoints live in `..\bfv-reference-local\checkpoints`.
