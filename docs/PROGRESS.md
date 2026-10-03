# Whole-game completion

![Whole-game completion](../reports/progress.svg)

**0%: 0 of 7 full-project acceptance milestones complete.**

100% means the original unmodified Battlefield Vietnam client and dedicated-server
engine have been reconstructed into readable, editable source, compile without
copying original executable code, and pass documented original-behavior checks.
User-provided original game assets remain a separate input.

| Milestone | Current state |
| --- | --- |
| Original unmodified reference | In progress; pristine asset corpus and full-game traces unverified |
| Interfaces and object layouts | In progress; selected interfaces recovered |
| Complete AI | In progress; selected routines reconstructed |
| Complete gameplay and physics | No complete reconstructed subsystem validated |
| Complete multiplayer | Original native networking still required |
| Engine services and standalone build | In progress; replacement payload build still requires original EXEs |
| Original-game parity | In progress; focused checks pass, full-game equivalence unverified |

The percentage is `completed milestones / 7 × 100`. Each milestone counts only
after meeting its full acceptance criteria in [ROADMAP.md](ROADMAP.md). This is a
coarse completion tracker for the whole project, not a precise estimate of the
amount of code recovered or time remaining. Zero complete milestones does not
mean zero work: the README and verification reports describe the partial results.

We cannot currently measure an exact whole-game decompilation percentage.
Function counts do not account for complexity, missing boundaries, external
dependencies or behavioral correctness. Exporting C-like decompiler output is
also insufficient to produce editable, independently buildable source.

Update `reports/project-milestones.json` with evidence when an acceptance criterion
is met, then run `python analysis/tools/update_progress.py`. The generated JSON,
SVG badge and suggested GitHub About text use the same whole-project definition.
`reports/recovery-registry.json` separately records reconstructed native entries;
that inventory is not the primary completion percentage.
