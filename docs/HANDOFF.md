# Reconstruction handoff

Updated **2026-10-04, Asia/Baghdad**. Start here when resuming in another agent.
The user's latest direction is **resume existing work, commit and publish to GitHub**.
This supersedes the earlier local-only restriction. Preserve source and reports;
exclude original binaries, assets, raw bodies and private analysis from publication.

## Current resume checkpoint

The second-setup checkpoint passed **41,148 client + 39,564 server = 80,712
full comparisons** and 16 matching-hash live vector executions. Compact reports
were exported; generated EXEs, manifests and reports are preserved privately in
`bfv-reference-local/checkpoints/2026-10-04-second-setup/{client,server}`.
Sessions 25051/client and 13088/server completed with exit code 0. Eligibility
and traversal are registered; complete inventory is **1,231**. Whole-game acceptance remains
0/7. Earlier category/region/query/movement checkpoints remain preserved.

The following category stage is now **installed**. `list_search.c/.h` translates
the complete 40-byte iterator search at client `009bf7c0` / server `00775bf0`.
`artillery_category_score.c` translates the 383-byte inline stage at `009a007a`
/ `0074a85a`, ending at the native alternate flag path `009a01f9` / `0074a9d9`.
Its two new patches per target passed read-only `AuditCategoryScore.java` guards,
with metadata in `reports/*/category-score-audit.tsv`.

Private staged checks passed **611 comparisons per target (136 search + 475
category)**; source/payload hashes and artifacts are in
`bfv-reference-local/category-score/staged/{client,server}`. Current EXE focused
checks passed on the installed EXEs. Full/live status is recorded below and in
`reports/handoff-state.json`; compact reports were exported after both checks.

The stage executes actual interface lookup, node search and float selectors.
Virtual object methods remain controlled. Four list calls can return different
containers; EDI retains the second sentinel while later frame slots capture the
third/fourth results. Native FST rounds the score into frame+0x14 without popping
ST0; FCOMP compares that retained extended score to the float32 best at +0x48.
Equal rounded bits still select in four 53/64-bit rounding-mode fixtures. Preserve
that behavior and callback-dependent pointer/descriptor rereads.

Region focused checks passed 1,067 geometry + 592 modifier comparisons per target.
The symmetric-bound helper rejects ordered less for its initial bounds, keeps
reflected x extended and rounds reflected y. All bound checks accept unordered;
radial mode uses strict ordered distance less than radius. Projection writes x
before reading z. These are validated source details, not generic geometry rules.

The retained query insertion at `00972940` / `0072d640` differs from the earlier
insertion by four omitted WAIT instructions. Its native listings stay private.
The next alternate flag path calls the 399-byte pool/object eligibility helper
at client `0097ede0`; its listing is private under `artillery-query/bodies`.
`artillery_target_eligibility.c/.h` is installed and calls the already registered
`bfv_artillery_component_test` in `artillery.c`; there is no duplicate event-3
predicate. Both targets passed 176 focused comparisons and the nine-byte entry
passed structural audits. Its full/live checks passed and the checkpoint is
preserved. The predicate was already registered; no extra predicate entry was
added to the inventory.

The following `artillery_alternate_gate.c` translates the 107-byte inline stage
at `009a01f9` / `0074a9d9`, ending at `009a0264` / `0074aa44`. It is **installed**
and passed 164 comparisons per target plus both read-only
`AuditArtilleryAlternateGate.java` audits. Private payload/source hashes and case
data are in `bfv-reference-local/alternate-gate/staged/{client,server}`; the script
is `alternate-gate-staged.py`. The native owner-method result goes into frame
`+0x70`, since the apparent `+0x74` store executes after PUSH 0.

`artillery_nested_score.c` is also installed: the 360-byte loop at `009a0264`
/ `0074aa44` ends at `009a03cc` / `0074abac`. It passed 242 staged and installed
comparisons per target and `AuditArtilleryNestedScore.java` in both originals.
It accumulates component ratings into frame+0x5c and target scores into +0x14,
using actual list search and the reconstructed traversal wrapper with retained pool lookup.
Four list results can differ; search uses the current handle at frame+0x54.
The first category/table capture precedes the rounded sum store, including an
explicit descriptor-alias fixture. Native virtual methods remain controlled.
Final scaling/selection is installed below; later evaluator phases remain native. Private staged
artifacts are in `bfv-reference-local/nested-score/staged/{client,server}`.

The gate/loop and following finish/traversal full runs passed and their checkpoints
are preserved. All four current focused groups, **80,078 full comparisons** and
16 matching-hash live checks passed. That checkpoint is preserved. The iterator
and second-pass setup are now installed; full sessions 25051/client and
13088/server completed with exit code 0. Their checkpoint is preserved.

The following final alternate scaling/selection at `009a03cc` / `0074abac`
is **installed** in `artillery_alternate_finish.c`. Its 170-byte block
passed 329 staged and installed comparisons per target and both `AuditArtilleryAlternateFinish.java`
audits. The signed setting is compared against the accumulated rating; unordered
skips the additional scale. Best-target selection reloads the rounded float score,
so equal rounded scores never select in all twelve control modes. Private artifacts
are in `bfv-reference-local/alternate-finish/staged/{client,server}`.

The retained 41-byte traversal helper at `00963a20` / `0071b110` is separately
**installed** in `target_traversal.c/.h`. It passed 154 staged and installed comparisons
per target and both `AuditTargetTraversal.java` audits. Its second stack argument
is read after the first callback, including when scratch aliases that argument.
Private artifacts are in `bfv-reference-local/target-traversal/staged/{client,server}`.
The loop now calls this source helper directly. The loop fixture tracks the
second argument at helper-entry ESP+8, preserving its mutation check without
assuming identical native/source internal stack frames. Its source inventory
entry is registered after installed full evidence passed. The four-byte
setting getter at `00956100` / `00713230` is read directly by the installed finish
source; its other native callers remain unpatched. It is not a new inventory entry.

`artillery_next_candidate.c` is **installed**. The 29-byte iterator
at `009a0476` / `0074ac56` passed 157 comparisons per target and both
`AuditArtilleryNextCandidate.java` audits. It captures the next node and bot table
before storing frame+0x40, calls list slot+0x84, then compares the captured node
against the freshly returned boundary. Tests include a vtable alias at frame+0x40
and callbacks changing the boundary/frame node. Private artifacts are under
`bfv-reference-local/next-candidate/staged/{client,server}`; use private
`stage-inline.py next-candidate` to stage it again. Continuations are first-pass
filter and second-pass setup at `009a0493` / `0074ac73`.

`artillery_second_setup.c` is **installed**. The following 93-byte
block passed 160 comparisons per target and both `AuditArtillerySecondSetup.java`
audits. Pattern slot+0x28's low byte can skip the pass. Otherwise it initializes
the three vector fields, calls query slot+0x24 with raw argument bits 44548000,
and routes by the resulting begin/end pointers. Native reads the manager table
after initialization. EAX carries the allocation pointer into the empty cleanup;
the source packet preserves it and tests explicitly compare it. Query methods
remain controlled; cleanup, exceptions and second-pass filtering remain native.
Private artifacts are under `bfv-reference-local/second-setup/staged/{client,server}`;
use private `stage-inline.py second-setup`. Filtering starts at 009a04f0/0074acd0.

Fresh private full listings under `bfv-reference-local/current-artillery/{client,server}/bodies`
contain all 8,269 bytes. Read-only `InspectArtilleryEvaluator.java` followed by
`InspectNativeFunctions.java` restored the 31 release-continuation bytes in memory
before export; changes were discarded from both projects. Older listings omit
these bytes, including ADD ESP,4 after the query-buffer free. Use the fresh listings
for subsequent work; raw bodies/decompiler exports remain private.

## Objective and current state

Recover the original unmodified Battlefield Vietnam client and dedicated server
into readable, editable source that builds without copying original executable
code, preserving original gameplay and multiplayer. Most engine code still runs
from the original binaries. Full decompilation/reconstruction is **not complete**.
Do not return to the abandoned Bevy remake or publish raw decompiler output as
completed source.

Whole-game acceptance remains **0% (0/7 milestones complete)**. This measures
completed whole-game milestones, not effort or source-function coverage. See
[PROGRESS.md](PROGRESS.md) and [ROADMAP.md](ROADMAP.md). The separate source inventory
contains 1,231 recovered entries across the binaries, excluding partial
evaluator stages. Eligibility adds one function per target; the event-3 predicate
was already registered before this work. Gate/loop stages add no complete entries.
The traversal helper adds one complete entry per target. Iterator/query setup is
installed and partial; it adds no complete inventory entries.
The requested GitHub About percentage has not been confirmed updated: previously
available connector tools could write Git data but not repository metadata.

## Repository and reference locations

| Item | Location |
| --- | --- |
| Game/workspace | `X:\Battlefield Vietnam - Copy` |
| Repository | `X:\Battlefield Vietnam - Copy\battlefield-vietnam-reconstruction` |
| GitHub | <https://github.com/LeakorgX/battlefield-vietnam-reconstruction> |
| Base source commit before publication | `5bd83fa` — Reconstruct first-pass artillery weapon scoring and selection |
| History reconciliation commit | `7311997` — preserves equivalent local/GitHub histories |
| Last verified remote main | `019145a6119489bc125b36268c27fe5b7493b276` — same file contents as local commit |
| Private analysis | `X:\Battlefield Vietnam - Copy\bfv-reference-local` |
| Complete client assembly | `bfv-reference-local\current-artillery\client\bodies\0099f2a0.asm` |
| Complete server assembly | `bfv-reference-local\current-artillery\server\bodies\00749a80.asm` |
| Corrected decompiler reference | `bfv-reference-local\artillery-recovery\client\0099f2a0.c` and `server\00749a80.c` |
| Private tree listings | `bfv-reference-local\artillery-tree` |
| Ghidra | `X:\ghidra_12.1.4_PUBLIC` |
| Ghidra projects | `bfv-reference-local\ghidra-projects`: `BFVBinary` / `BfVietnam.exe`, `BFVServerBinary` / `bfvietnam_w32ded.exe` |
| Java | `C:\Program Files\Eclipse Adoptium\jdk-21.0.7.6-hotspot` |
| 32-bit compiler | `C:\msys64\mingw32\bin` |

Prior local and remote histories were unrelated because publication used GitHub's
Git-data connector. Fetch confirmed the base trees differ only in line endings.
Merge 7311997 preserves both histories and the local tree, allowing a normal
fast-forward publication without force-pushing. A CLI push dry-run succeeded.

Supported original SHA256 values:

```text
client 79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5
server 86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d
```

These are fixed-base 32-bit PE images at `0x400000`. Keep original binaries/assets
and raw decompiler/assembly exports private and outside the source repository.
The installation has previously contained modified archives/custom maps; pristine
asset-level parity has not been verified. Binary comparisons use the hashes above.

## Latest uncommitted implementation

Earlier uncommitted files, retained with the new stages above:

- `bfv-native-code/src/aim_geometry.c` and `.h`: affine point transform, vector
  difference, aiming-view world position and component position/event wrapper.
- `bfv-native-code/src/artillery_aim_gate.c`: first-pass candidate weight and
  aiming gate following weapon selection.
- `bfv-native-code/src/artillery_movement_gate.c`: following movement-vector,
  flag and distance gate, preserving the captured receiver in EDI.
- `bfv-native-code/tools/verify_artillery_movement.py`: 521 comparisons per binary.
- `bfv-native-code/src/movement_geometry.c` and `.h`: in-place cross product and
  component event-2 scalar wrapper.
- `bfv-native-code/src/artillery_movement_score.c`: following movement score.
- `bfv-native-code/tools/verify_movement_score.py`: 645 comparisons per binary.
- `analysis/tools/ghidra/AuditMovementScore.java`: helper/scoring boundaries and
  retained score=1 alias.
- `analysis/tools/ghidra/AuditAimGeometry.java` and `AuditArtilleryMovement.java`:
  read-only structural audits, with compact metadata under `reports/*`.
- `bfv-native-code/tools/verify_aim_geometry.py`: 839 differential comparisons per
  binary, with math, aliasing, callback mutation, ABI and x87 checks.

Modified files additionally include artillery/verification documentation,
Ghidra tool documentation, source registry and generated inventory progress.

- `bfv-native-code/tools/build.py`: target addresses and guarded entry patches
  for the helpers and inline stages above; current totals are in the hash section.
- `bfv-native-code/tools/verify.py`: integrates all installed focused groups into full runs.
- `bfv-native-code/tools/export_reports.py`: exports their compact current-hash reports.
- `bfv-native-code/README.md`: short source/test inventory entry.

The handoff request additionally creates root/repository `AGENTS.md`, this guide,
and `reports/handoff-state.json`, and adds a short README link to this guide.
Use `git status --short` to confirm the exact current set before editing.

### Recovered interfaces

| Function/block | Client | Server | Size / ABI |
| --- | --- | --- | --- |
| `bfv_transform_point` | `0049b340` | `005b25f0` | 90 bytes; ECX matrix, point/output stack args, RET 8, EAX output |
| `bfv_vector_difference` | `0049b490` | `0048d160` | 37 bytes; ECX origin, output/point stack args, RET 8, EAX output |
| `bfv_aim_world_position` | `009be440` | `0078df90` | 113 bytes; ECX view, output stack arg, RET 4, EAX world matrix |
| `bfv_component_position` | `00994a80` | `0073f1e0` | 31 bytes; ECX component, output stack arg, RET 4, EAX world matrix |
| `bfv_artillery_aim_gate_bridge` | `0099fa61` | `0074a241` | 208-byte inline stage, not a whole function |
| Gate accept continuation | `0099fb31` | `0074a311` | Retained evaluator |
| Gate reject continuation | `009a0476` | `0074ac56` | Retained evaluator |

Helpers use x86 `thiscall`. Small x87 assembly primitives preserve original
evaluation order; they are embedded in readable C. The world-position helper
stores each coordinate immediately, unlike the point transform which computes
all three before stores. This matters when buffers overlap.

The gate uses the existing PUSHAD packet (EDI, ESI, EBP, original ESP, EBX, EDX,
ECX, EAX). EBP is the target object; original ESP is the evaluator frame.
If record byte `+0x14` is nonzero, it writes frame `+0x74` as
`1 / (1 + (frame[+0x58] - record[+0x18]) * 0.05)` with native rounding and no clamp.
Otherwise the weight is 1; a nonzero driver word at frame `+0x1fc` skips aiming.
The remaining path retrieves positions, transforms the record point, subtracts
the selected weapon's origin and calls the existing reconstructed aiming helper.
Do not invent names/units for record fields whose semantics remain unresolved.

Preserve the apparently unusual offsets: the selected weapon is frame `+0xec`,
the direction difference output is `+0x1c4`, and copied direction is `+0x1a4`.
Native stack offsets shift while arguments are pushed. The gate retains the
component captured at frame `+0xf4` across callbacks, but rereads other pointers
as the original does. Tests exercise these details.

## Current verification and hashes

The installed iterator/second-pass setup EXEs match their current manifests:

```text
client c24ed0b4dcc139f73003beffb29e81f763dedf886806217093bf2bc825760457
server b8bbecfbcbff474c8bac8044512b37ad9175c18a107aa1d5e41e881f755d4cb7
```

Payloads are 31,885 / 29,037 bytes, with 682 / 537 guarded entry patches and
five vtable patches per target. Current focused gate/loop/finish/traversal checks
passed 164 + 242 + 329 + 154 per binary and live checks passed 16 executions.
Current focused iterator/setup checks passed 157 + 160 per target and matching-hash
live checks passed 16 executions. Full sessions 25051/client and 13088/server
completed with exit code 0: 41,148 + 39,564 = 80,712. Exported compact reports
match the current hashes; EXEs and full evidence are preserved privately.

## Resume checklist

1. Current full runs completed; no verifier needs restarting. Preserve the
   verified second-setup checkpoint before installing further source stages.
2. Full/live checks passed and compact reports were exported. The current request
   authorizes committing and pushing source/reports. Keep original inputs/private
   analysis outside the repository; inspect Git history for publication status.
3. Continue second-pass filtering at `009a04f0` / `0074acd0`. Later history, second
   pass, target-state updates and aiming/firing remain unreconstructed.
4. Full physics, gameplay, networking and the independently source-owned engine
   remain incomplete. Inline stages do not count as complete evaluators.

## Commands

Run from the repository in PowerShell. `uv` supplies `pefile`/`unicorn`; plain
`C:\Python314\python.exe` does not have them installed. If the shell tool default
fails, explicitly select `C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe`
with `login:false`.

```powershell
$env:BFV_GAME_DIR = 'X:\Battlefield Vietnam - Copy'
git status --short

# Rebuild only when source changes require it. Close generated game processes first.
uv run --with pefile python bfv-native-code/tools/build.py --game-dir $env:BFV_GAME_DIR --target both

# Position-helper checks; current full suites include these comparisons.
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_aim_geometry.py --game-dir $env:BFV_GAME_DIR --target both

# Current score/helper focused checks passed (645 per binary).
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_movement_score.py --game-dir $env:BFV_GAME_DIR --target both

# Movement-gate suite is also included in current full runs.
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_artillery_movement.py --game-dir $env:BFV_GAME_DIR --target both

# Installed alternate gate and nested-loop checks passed: 164 + 242 per target.
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_alternate_gate.py --game-dir $env:BFV_GAME_DIR --target both
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_nested_score.py --game-dir $env:BFV_GAME_DIR --target both

# Full suite: current build passed; rerun after further installed source changes.
uv run --with pefile --with unicorn python bfv-native-code/tools/verify.py --game-dir $env:BFV_GAME_DIR --target client
uv run --with pefile --with unicorn python bfv-native-code/tools/verify.py --game-dir $env:BFV_GAME_DIR --target server

# Own temporary processes only; requires private server settings already checked by the tool.
uv run --with pefile --with capstone python bfv-native-code/tools/verify_live_exceptions.py --game-dir $env:BFV_GAME_DIR --target both

# Requires current full and live reports for both targets.
C:\Python314\python.exe bfv-native-code/tools/export_reports.py
C:\Python314\python.exe analysis/tools/update_progress.py
git diff --check
```

Ghidra read-only example (replace script/output with the new audit):

```powershell
$env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.7.6-hotspot'
& 'X:\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat' `
  'X:\Battlefield Vietnam - Copy\bfv-reference-local\ghidra-projects' BFVBinary `
  -process BfVietnam.exe -noanalysis -readOnly `
  -scriptPath "$PWD/analysis/tools/ghidra" -postScript YOUR_AUDIT.java OUTPUT_TSV
```

For the server use `BFVServerBinary` and `bfvietnam_w32ded.exe`. Do not save changes
to the reference projects unless needed and explicitly justified. Do not publish
raw native bodies. Only reuse currently callable tools; former connector names or
in-memory tool stores may not survive transfer to another agent.

## Prompt to give the next agent

> Read AGENTS.md and docs/HANDOFF.md. Resume and publish verified changes to GitHub.
> Second-setup checkpoint passed 80,712 comparisons and 16 live checks and is private.
> Inventory is 1,231; eligibility reuses the existing component predicate.
> Gate/loop/final scaling/traversal are installed, with 164 + 242 + 329 + 154
> focused checks per target, structural audits and 16 live checks passed.
> Full sessions 25051/client and 13088/server completed with exit code 0.
> Iterator/setup are installed and passed 157 + 160 focused checks per target,
> audits, full regression and 16 live checks. Reports are exported and the
> checkpoint is preserved. Inspect Git history for publication status.
> Preserve live EAX for empty-query cleanup. Use complete repaired private listings
> in current-artillery/{client,server}/bodies. Continue filter at 009a04f0/0074acd0.
