# Reconstruction handoff

Updated **2026-10-05, Asia/Baghdad**. Start here when resuming in another agent.
The user authorizes **committing and publishing to GitHub**, then continuing the
full reconstruction. Preserve source and reports; exclude original binaries,
assets, raw bodies and private analysis. Use an authenticated CLI or connector.

## Current checkpoint: installed second-pass category scorer

The current installed EXEs passed **46,160 client + 44,576 server = 90,736**
full comparisons and **8 live vector exception checks per target**. The guarded
exporter verified both EXE hashes and refreshed compact reports. There is no
remaining verification process to wait on for this checkpoint.

| Target | Installed SHA-256 | Payload bytes | Entry patches | Vtable patches |
| --- | --- | ---: | ---: | ---: |
| Client | `872573710aef446c1ad5828462c8f7f88e74380d75831fe64e087b61b7ef9cc1` | 37,133 | 697 | 5 |
| Server | `eeea0961906021b3b618a5a81b389b1b938d3aa9b404d0011a417b3b29fb3a81` | 34,317 | 552 | 5 |

`artillery_second_category_score.c` owns the 420-byte, 110-instruction interval
at client `009a0dba..009a0f5e` / server `0074b59a..0074b73e`. It reconstructs
category accumulation, generation-handle resolution, score scaling, best-record
selection and the loop back to the reconstructed factor. Both structural audits
passed. The full suite includes **262 scorer comparisons per target**, covering
callback mutations, output memory, exceptional floats, precision/rounding modes,
retained x87 registers and continuation state.

Object methods remain controlled in these focused comparisons; EDX/CPU flags,
the complete evaluator and live artillery matches are not verified by that suite.
The live checks cover vector insertion exception handling, not artillery play.
The gate and split prefix/factor stages are also installed and verified.
Complete-function inventory remains **1,233**; these partial inline stages do not
add complete function entries. Whole-game acceptance remains **0/7**.

Remote history was fetched before publication: local HEAD and origin/main both
matched `9ba1f1b`. Verify the published commit with Git before resuming. Next
recover the iterator/cleanup continuation at client **`009a0f5e`** / server
**`0074b73e`**, then finish target-state/firing and the remaining whole-game work.

The following checkpoint descriptions are retained as **historical evidence**.
Their older hashes, sessions and authorization statements are superseded by the
current section above and the current resume checklist below.

## Latest verified checkpoint: split second-pass category factor

The installed split category prefix/factor passed **45,898 client + 44,314
server = 90,212 full comparisons** and 16 matching-hash live vector executions.
Both full client/server sessions completed with exit code 0; the guarded exporter
completed, so `reports/{client,server}` match the current manifests. Whole-game
milestones remain **0/7** and the complete-function inventory remains **1,233**;
these inline replacements do not add a complete function entry.

`artillery_second_category_gate.c` reconstructs the 40-byte interval at client
`009a0c7e..009a0ca6` and server `0074b45e..0074b486`. It preserves the
candidate category-bit test, captured descriptor/argument, fastcall eligibility
call, EDI continuation value and full EAX helper result. Structural audit reports
ten instructions, an eight-byte guarded entry and no exterior/interior overwrite
references. `verify_second_category.py` supplied **262** comparisons per target,
including raw flags/results, callback mutations, register/continuation state and
x87 precision/rounding variants. Eligibility remains a controlled dependency in
that focused suite; retained category scanning and target lifecycle remain native.

The prior 276-byte candidate crossed a loop re-entry, so it is now safely split:
`artillery_second_category_prefix.c` covers 74 bytes at client
`009a0ca6..009a0cf0` / server `0074b486..0074b4d0`; the re-enterable
`artillery_second_category_factor.c` covers 202 bytes at client
`009a0cf0..009a0dba` / server `0074b4d0..0074b59a`. The factor boundary is the
native loop re-entry from client `009a0e5f` / server `0074b63f`. The code
preserves target-service capture, category-class accumulation, callback-dependent
list reads, iterator search, EDI/EBP/EBX continuation state and the live x87
factor. `verify_second_category_factor.py` passed **264** comparisons per target.
The structural audit confirms both guarded boundaries; the factor's only external
exit is its intended native continuation.

Current installed hashes are `97edae268f828aa090c85223fffe7255364a9a1a1ac5c93d18cdbeab3f71f8a9`
(client) and `f06cff89850907283c6495e3c51776127595ccf4152a367c9216a404817242b5`
(server). Payload sizes are 36,365 / 33,549 bytes, with 696 / 551 guarded entry
patches and five vtable patches per target. Continue after the factor at client
`009a0dba` / server `0074b59a`, then recover target-state, firing, networking
and the remaining engine work. No commit or push is authorized now. Older
sections below are historical.

### Read-only boundary inspection: second-pass category scorer

The next native interval is client `009a0dba..009a0f5e` and server
`0074b59a..0074b73e`, 420 bytes and 110 instructions in each target. The
read-only Ghidra inspection is recorded in
`reports/{client,server}/artillery-second-category-score-inspection.tsv`.
Both targets have the same shape: the factor's internal continuation enters the
interval at its first instruction; the interval has one loop exit back to the
already reconstructed factor (`009a0e5f -> 009a0cf0` / `0074b63f -> 0074b4d0`),
then three exits to the native iterator continuation (`009a0f5e` /
`0074b73e`). This makes a combined scorer bridge structurally possible, but it
must preserve the factor re-entry and the live x87 factor. No scorer source has
been installed; the prior trial was removed after its differential test exposed
an x87/field-layout mismatch. Rebuild from the hashes above before continuing.

## Active continuation: installed second-pass movement

The verified second-weapons checkpoint was published to GitHub main as
`42827f75858de793248c17ecdd4c37e3ea274366`; remote main was checked and matched.
It passed 85,376 full comparisons and 16 live checks and is preserved privately.
Public compact installed reports still describe that checkpoint.

The subsequent movement gate and score are now installed locally. Both original
hashes were checked before publication. Movement gate focused checks passed 522
per target; score focused checks passed 439 per target; session **65041** exited 0. The privately
compiled composite passed 439 score cases per target, including eight with the
compiled gate, and structural audits passed for both intervals. Current-hash live session **61113** exited 0, passing 8 executions per target.
Full sessions **42034 client / 37476 server** are running; Python PIDs
**43736 / 40648** were confirmed live. Reuse these sessions; do not relabel old
full reports with the new hashes.

Current installed hashes:
- client `722af5af9be654c0360d6bf640b641618984d6800f370456183e982603a87eb2`
- server `882a5f0c347a7f6a76f904fe58f50fd68c56ec1b998ffcefc82b55c1f9db6a68`

Payloads are 35,149 / 32,333 bytes, with 691 / 546 guarded entry patches and five
vtable patches per target. Inventory stays 1,233 complete functions; these are
partial inline stages, and whole-game milestones stay 0/7. Private one-time
`install-second-movement.py` HAS been executed; do not rerun it. Build, full
verifier and guarded compact exporter now integrate both movement stages.

Next: re-poll full sessions42034/client and37476/server, then
export/preserve/publish only after matching results. Continue second-pass driver
predicate at 009a0bc9/0074b3a9 afterward. Earlier checkpoint history follows; its older resume commands describe historical
builds. The active section above is authoritative for the latest installed state.

## Active continuation: second-pass weapons

The whole-project goal is active and incomplete. Main was fetched and verified
as `f34ae00e5bddf2ea16d23e8692d1ad26396e43a0`; local HEAD matched it before
publication of the current work. Original EXE hashes remain unchanged.

The second-aim checkpoint passed **42,760 client + 41,176 server = 83,936**
full comparisons and 16 matching-hash live vector executions. Sessions 13052 and
78626 exited 0. Compact reports are exported and EXEs/full/live evidence are
preserved under `bfv-reference-local/checkpoints/2026-10-04-second-aim`.
The complete normalization helper is registered after installed full evidence
passed. Inventory is **1,233** (689 client + 544 server); whole-game milestones
remain **0/7**. Earlier second-filter checkpoint passed 81,422 and is preserved.

Second-pass position/distance/weapon stages are now **installed**. Both targets
passed 164 + 262 + 294 staged and installed focused comparisons and all three
structural audits. Installed focused session 60471 and live session 65469 exited
0; current-hash live checks passed 8 executions per target. Full sessions **51149 client / 75419 server** exited 0, with
**43,480 + 41,896 = 85,376** comparisons. Compact reports were exported through
the guarded exporter and the matching EXEs/full/live evidence are preserved at
`bfv-reference-local/checkpoints/2026-10-04-second-weapons`. These sessions are
terminal; there is no current full verifier to reuse. Publication is pending.
No unrelated original game processes were terminated.

Current installed EXE hashes:
- client `594bd509e82f141b6835340537d5a6ee085b005723065309cce9cce0718bca82`
- server `b3edbbf838361e97c2115ec656031ac8af861c80285714a0b44a16e4c43f8773`

Payload sizes 34,061 / 31,245 bytes, guarded entry patches 689 / 544, five vtable
patches each. Entry-patch counts and complete inventory are separate metrics.

Installed source stages and evidence:
- Filter: 229 bytes at 009a04f0/0074acd0, 355 cases/target; callback-dependent
  candidate, component, iterator and history-receiver reads and x87 comparisons.
- Weight: 88 bytes at 009a05d5/0074adb5, 364 cases/target. Slot1a4 writes timestamp
  frame10c; low byte controls weight. Float rounding precedes actual clamp math.
- Normalization: complete 128-byte helper at 004a2a10/00438d30, 687 cases/target.
  Squared length rounds before tolerance checks. Tiny vectors zero with EAX
  ffffb1df; near-unit vectors stay unchanged. ECX returns squared-length bits
  because native scratch overwrites its saved ECX slot. Registered after full.
- Aim: 127 bytes at 009a062d/0074ae0d, 206 cases/target. Actual source position,
  difference and normalization helpers execute with controlled final predicate.
- Position: 74 bytes at 009a06ac/0074ae8c, 164 cases/target. Actual event-interface
  helper, captured candidate/EDI, slot2c writes position or sequential fallback
  copies from slot18, preserving overlap and callback frame changes.
- Distance: 152 bytes at 009a06f6/0074aed6, 262 cases/target. Native rounded z/x/y
  and intervening word stores, actual length/maximum(length,0.5)/division helpers.
- Weapons: 298 bytes at 009a078e/0074af6e, 294 cases/target. Rating uses signed
  availability, with -1 converted to 65536; no first-pass history/distance term.
  Weapon table capture precedes rounded class store, method read follows it.
  Strict best score/index selection, dynamic captured-vector reads and accepted
  padding preserve EBP/vector and EBX/scan count. Tests include frame aliases,
  counts through ten, callbacks, exceptional parameters and occupied x87 stacks.

All inline stages remain partial evaluator work and add no complete inventory
entries. Object methods are controlled where stated; sustained match, full target
selection and firing behavior remain unverified. Unmasked faults/overflowing x87
caller stacks are excluded from helper claims. See ARTILLERY.md for detail.

Private staged artifacts are under second-filter, second-weight, vector-normalize,
second-aim, second-position, second-distance and second-weapons, each with
staged/{client,server}. Use private stage-inline.py with the matching name.
It links actual helper sources or resolves calls to the hash-matched installed
source helper; raw native evidence remains outside the repository. Private
install-second-aim.py and install-second-weapons.py have already been executed;
do NOT rerun their one-time mutations. All current groups are integrated into
verify.py and guarded compact export.

**Next:** publish verified source/reports, then integrate the staged movement gate
and score. The private one-time `install-second-movement.py` integration script
is prepared but has NOT been executed. Rebuild both targets and run installed
focused/full/live checks before exporting new installed evidence.
Movement gate
009a08b8/0074b098 through 009a0981/0074b161 (201 bytes). It captures frame24 in
EDI, initializes velocity frame64/68/6c, optionally calls slot14 and captures
all three words before stores. Source flag bit1 at word(frame1f8)+4 and target
flag bit1 at word(frame18)+4 gate pattern slot28. Low-byte false multiplies
frame20 by movement scale and compares with frame28: ordered-less writes
frame34=0 and routes 009a0bc2/0074b3a2. Otherwise magnitude evaluates
x*x + z*z + y*y (different first-pass order); compare mask4100 nonzero resumes
0981/b161, other outcome zeroes score. Low-byte true uses alternate scale:
strict ordered greater routes unit score 009a0bba/0074b39a, other outcomes
continue 0981/b161. Preserve captured EDI and callback rereads. Later heavy
movement/score, vector cleanup, target-state, firing and most engine work remain
native. Use complete private current-artillery listings, not older truncated ones.

## Staged continuation: second-pass movement

`artillery_second_movement_gate.c` and `verify_second_movement.py` now reconstruct
the 201-byte movement gate, **staged only**. Both targets passed 522 comparisons
and read-only `AuditArtillerySecondMovement.java` audits (six-byte entry boundary,
three continuations, no interior references). Fixtures cover callback replacement
of frame18's target pointer, captured movement/EDI, zero initialization, overlapping
vector copies, flags, low-byte predicates, exceptional inputs and occupied x87
states at depths 0/2/5 in all twelve control modes. This is not installed: entry,
full verifier and compact export integration remain pending. Private artifacts
are second-movement/staged/{client,server}; rerun stage-inline.py second-movement.
The private stage driver now also overrides a fixture module's own load_machine
when present, so this suite compares real native versus privately compiled code.

The staged score was reconstructed separately from first-pass movement score.
An auditable interval is 009a0981..009a0bba / 0074b161..0074b39a (569 bytes),
ending before native unit-score entry. It needs three continuations: unit bba/b39a,
reciprocal/reload bc2/b3a2, and heavy-score bc9/b3a9. Do not include externally
entered unit/reload blocks in a no-interior-entry interval. Initial TEST EDI and
JZ unit occupy eight bytes; expected guard 85ff0f8431020000. Verify from originals.

Semantics from the complete private listing:
- Null incoming movement routes unit. Distance frame28 versus parameter frame20
  mask4100 nonzero uses freshly read frame24's slot14 and reciprocal length,
  storing frame34, then native reload bc2.
- Heavy path captures driver frame1fc in EDI. The event-2 scalar callback can
  change the frame driver pointer, but the following slot14 call still uses
  captured EDI; unlike the first-pass source, do not reread the driver pointer.
- Driverless path zeroes scratch frame110/114/118 and scalar framec0. After driver
  callbacks, capture movement freshly from frame24 in EBX BEFORE driver copies
  to frame88/8c/90. Preserve EDI/driver and EBX/movement on heavy continuation.
- Scalar delta against framec0 uses actual component_event2_scalar. Mask4100
  nonzero stores quarter at frame34 and jumps bc9/b3a9.
- Cross scratch is frame150/154/158 (0,1,0), crossed with basis frame78/7c/80.
  Capture result to frameb0/b4/b8, then slot14 on captured EBX returns movement
  vector. Actual source cross/scalar helpers already exist in movement_geometry.c.
- Four dot products retain native operand/addition order: A=driverX*crossX +
  crossZ*driverZ + crossY*driverY; B=crossY*moveY + crossX*moveX + crossZ*moveZ;
  C=basisY*moveY + basisX*moveX + basisZ*moveZ;
  D=driverX*basisX + driverZ*basisZ + driverY*basisY.
- FSUBP then rounded/clamped frame24; FSUBRP then rounded/clamped frame3c.
  GNU x87 syntax used by existing source is fsubrp for native FSUBP, and fsubp
  for native FSUBRP. Verify differential cases rather than assuming mnemonics.
  The returned movement vector may alias projection stores; keep subsequent
  length read timing. Final quarter/(length*scale + 2*(frame3c+frame24) + 1)
  rounds to frame34 and jumps heavy continuation bc9, bypassing native EDI reload.

Use source helpers and explicit live register packets. Compare callback captures,
frame aliases, both projection clamps, NaNs, x87 states and all three routes.
Later query, region/category, target selection and engine subsystems remain native.


## Staged continuation: second-pass movement score

`artillery_second_movement_score.c` reconstructs the 569-byte interval at
009a0981..009a0bba / 0074b161..0074b39a. It is staged only, alongside the
previous movement gate; neither new movement stage is installed yet. Both
read-only structural audits passed: 123 instructions, an eight-byte guarded
entry and no external or overwritten interior references. Both targets passed 439
staged comparisons, including eight cases executing compiled gate and score together.
Native unit-score,
driver-reload and heavy-score continuations remain separate.

The source captures the driver across its event callback, captures movement
from frame24 after driver callbacks, retains EDI/EBX on heavy continuation,
and executes actual event-scalar, cross-product and length helpers. It retains
second-pass dot-product order, rounded projection stores at frame24/frame3c,
ordered-negative clamps and the subsequent aliased vector read. Staged fixtures
compare full frame/arena memory, callback order, routes, live registers and x87
state. Object vector/event methods are controlled; complete evaluation and firing
remain unverified. Private source/payload hash evidence is in
`bfv-reference-local/second-movement-score/staged/{client,server}`.

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
   verified category-scorer checkpoint before installing further source stages.
2. Full/live checks passed and compact reports were exported for the current
   hashes. Commit and publication are authorized; inspect remote history first. Keep
   original inputs and private analysis outside the repository.
3. Continue after the second-pass scorer at `009a0f5e` / `0074b73e`.
   Later target-state updates, firing, history, networking and engine work remain
   unreconstructed.
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

> Read AGENTS.md and docs/HANDOFF.md. Commit and GitHub publication are authorized;
> verify current remote history. The installed category scorer passed 46,160 client
> + 44,576 server full comparisons and 16 live vector checks. Its focused suite has
> 262 cases per target and structural audits passed. Inventory is 1,233 and
> whole-game acceptance remains 0/7. Continue at client 009a0f5e / server
> 0074b73e. Preserve original inputs and raw native listings outside the repo.

## Following second-pass driver predicate: instruction evidence

Next interval is 009a0bc9..009a0c38 / 0074b3a9..0074b418 (111 bytes).
It does not implement the first pass's wider spatial query. It scales parameter
frame20 and compares distance frame28; AH mask05 parity-even skips to acceptance.
The other path rejects if captured EDI/driver is null, then captures frame18's
target in EBP and calls slot18 twice on that captured target (rereading its table
before the second call). First point remains captured across the second callback.
Both float inputs load before their stores: first point z and second point x.
After those callbacks but before stores, capture receiver framef8 in EBX and its
table in EBP. Store second x to frame138 and first z to frame13c, using x87
conversion. Actual component_event2_word executes on captured EDI, followed by
captured-table slot84(receiver, event word, frame138 point). Do not reread driver,
receiver or table after the helper callback. Low-byte true rejects at second_reject
009a0f5e/0074b73e; false accepts at c38/b418. EDI remains captured; EBP/EBX outputs
are path-dependent. Native region/category stages follow acceptance. Guard begins
four-byte FLD frame20 plus six-byte FMUL query-scale; verify the ten bytes and
incoming interior references from originals before detouring. The new staged source and fixtures implement this interval; installed/full
evidence remains pending.
