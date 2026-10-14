# Agent instructions

Read [docs/HANDOFF.md](docs/HANDOFF.md) and [docs/STATIC-RECOMP.md](docs/STATIC-RECOMP.md)
first. Treat the current filesystem and process state as authoritative.

## User requirements

- Goal: the original unmodified Battlefield Vietnam client and dedicated server
  as readable, editable, independently buildable source with original behavior,
  including AI, physics, gameplay and multiplayer.
- Approach: static recompilation. The executables are rebuilt from emitted
  assembly plus reconstructed C, and every build is gated on byte identity
  (or differences only at intended edits).
- Commit and push only when the owner asks. Preserve all verified source,
  reports and uncommitted work.
- Keep the README concise and focused on the static recompilation. No Bevy
  prototype history.
- Whole-project progress uses `reports/project-milestones.json` (0 of 7). Do not
  substitute instruction, function or byte counts for a whole-game percentage.

## Evidence rules

- Never commit original binaries, assets, generated `.s`/`.o` files, raw
  disassembly or decompiler bodies. The L1 lifted C in `bfv-native-code/lifted/`
  is committed by the owner's decision (2026-10-14). `build/` and `bfv-reference-local/` stay local.
- Use instruction evidence for ABI, layouts, evaluation order, callback rereads,
  aliasing, x87 behavior and native dependencies. Retain native behavior by
  default; no invented guards or clamps.
- Build only against the supported hashes. A report from an older EXE is not
  evidence for the current one.
- `export_reports.py` requires current full and live results for both targets;
  do not bypass its stale-report assertions.
- Reuse a confirmed running verifier; restart only after it is terminal or missing.
- Do not terminate unrelated game processes. No subagents unless the user asks.

## Environment

PowerShell from the repository root. If the shell default fails, use
`C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe` with `login:false`.
Use `uv run --with pefile --with capstone --with unicorn python ...`.
32-bit MinGW is at `C:\msys64\mingw32\bin`; plain Python is `C:\Python314\python.exe`.
