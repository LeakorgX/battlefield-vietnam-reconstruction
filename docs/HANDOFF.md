# Handoff

Updated **2026-10-13, Asia/Baghdad**. Start here when resuming. The full
chronological log up to this point is in [archive/HANDOFF-LOG.md](archive/HANDOFF-LOG.md).

**User direction: finish locally; do not commit or push to GitHub.** Preserve
uncommitted work. Keep original binaries, assets, generated assembly and private
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
  instruction-aligned functions. Installed build (v13 lift, VERIFIER 12): client 33,267
  and server 20,259 lifted functions run as C. "Runs as C": client 38,157/38,657
  (96.78%), server 23,755/23,986 (97.03%) functions. Live exception probes pass;
  the dedicated server runs 180 s and the client 90 s under `crashdbg` with no faults.
  Known-good EXEs: `X:\temp\bfvscan\lift\good-v6` (earlier: `good-v5`, `good-v4`).
- Whole-game acceptance: **0 of 7** milestones (`reports/project-milestones.json`).

Installed editable EXE hashes (match `build/<target>/manifest.json`):

```text
client 5489f14ed7eef8ef1bc043c3432b8759df08ff78172955379adaed5fc634d8bb
server 4c27f68d2f5078ccee838b93a9695b7e0c43d043e6763e23cfc715c66e5fa297
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
