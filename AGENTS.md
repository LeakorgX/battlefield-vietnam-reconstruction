# Agent instructions

Read [docs/HANDOFF.md](docs/HANDOFF.md) first. It records the current implementation,
hash-specific evidence, unfinished work, commands and local reference locations.
Treat the current filesystem and process state as authoritative when resuming.

## User requirements

- Reconstruct the original unmodified Battlefield Vietnam client and dedicated
  server into readable, editable source with original behavior, including AI,
  physics, gameplay and multiplayer. The native replacement build is intermediate.
- **Current instruction: resume existing work, commit and publish to GitHub.**
  This supersedes the earlier local-only restriction. Verify the installed build,
  inspect remote history and exclude original binaries/private analysis before publishing.
- No browser for GitHub. If publishing is later authorized, use an available
  connector or authenticated CLI; inspect remote state before updating it.
- Keep the README concise and focused on reconstruction; no Bevy experiment history.
- Whole-project progress uses `reports/project-milestones.json`: 0 of 7 complete
  at handoff. Do not substitute function counts or decompiler exports for a
  whole-game percentage. Never infer complete behavior from narrow tests.

## Implementation and evidence

- Preserve uncommitted work. Do not reset the repository or overwrite original EXEs.
- Use instruction evidence to establish ABI, field layout, evaluation order,
  callback rereads, aliasing, floating-point behavior and native dependencies.
- Retain native behavior by default; do not invent guards or clamp values without
  evidence. Clearly separate partial inline stages from complete functions.
- Build only against the supported hashes. Keep proprietary reference bodies,
  original binaries and assets outside this repository.
- Compare original and replacement instructions. Record test limitations and hashes.
  A report from an older EXE is not evidence for the current one.
- `bfv-native-code/tools/export_reports.py` requires current full and live results for both targets;
  do not bypass its stale-report assertions. See the handoff for remaining gates.
- Reuse a confirmed running verifier. Re-poll after an observation timeout; restart
  only after it is terminal/missing and current process state confirms it stopped.
- Do not terminate unrelated game processes. The live verifier manages its own
  temporary children. No new subagents unless explicitly requested by the user.

## Environment

Commands run from the repository in PowerShell. If tool shell defaults fail, use
`C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe` with `login:false`.
Use `uv run --with pefile --with unicorn python ...` for instruction comparisons.
32-bit MinGW is at `C:\msys64\mingw32\bin`; plain Python is `C:\Python314\python.exe`.
