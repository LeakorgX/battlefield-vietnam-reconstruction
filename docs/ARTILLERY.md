# Artillery AI reconstruction

`bfv-native-code/src/artillery.c` reconstructs the rating stage of
`BBFireArtilleryDriver`. The client method at `009a13a0` and server method at
`0074bb80` are replaced in the generated EXEs. The underlying target evaluator
now has source-owned cached-target validation. Much of candidate search still executes
original machine code; this is not yet complete artillery AI.

## Recovered behavior

1. Ask the bot for its driver handle and entry ID. Resolve the driver through the
   generation-checked object pool, obtain its entry interface, and resolve the gun.
2. Send event 3 to the gun's component receiver. Use the **returned component**
   for the following predicate, which need not be the receiver itself.
3. Obtain that component's owner, call owner slot `0x14`, convert the returned
   object through the native component-view helper, and test view slot `0x18`.
4. If its low byte is nonzero, call the artillery evaluator with the bot,
   recompute word, scale, gun and driver component. Store the rounded float rating,
   then store the evaluator's eligibility result for the bot.
5. Otherwise clear the eligibility byte and call the fallback evaluator with the
   same bot, recompute word and scale. Store its rounded float rating.
6. Fetch the bot index again and return the cached rating at that index.

Repeated bot-index calls and pointer reads are retained. Callbacks may change the
index or rating/flag table pointers. The code preserves the original ordering,
including the different rating/flag write order in the two branches.

## ABI and fields

The method is x86 `thiscall`: ECX holds the behavior; three stack arguments are
bot pointer, 32-bit recompute word, and float32 scale. It returns through x87 ST0
and consumes 12 argument bytes. The initial decompiler signature omitted the last
two arguments and misidentified stack values as an unrelated return address;
instruction inspection established the actual interface.

| Behavior offset | Recovered role |
| --- | --- |
| `0x1c` | Per-bot float32 rating table |
| `0x24` | Per-bot eligibility byte table |
| `0x30` | Fallback evaluator; rating slot at byte offset `0x24` |
| `0x34` | Artillery evaluator; eligibility slot at byte offset `0x10` |

Bot virtual slots are at byte offsets `0xcc` (driver handle), `0xd4` (entry ID)
and `0xdc` (bot index). Pool records are eight bytes: object pointer at zero and
generation at six. A handle's low word is a one-based index and high word is the
generation. Invalid handles resolve to zero; the original method subsequently
requires valid components. No new null guards have been invented.

| Dependency | Client | Server | Status |
| --- | --- | --- | --- |
| Rating stage | `009a13a0` | `0074bb80` | Reconstructed and patched |
| Component predicate | `009d8100` | `0078c670` | Reconstructed within the rating stage |
| Component-view conversion | `0049a980` | `00437e80` | Native |
| Artillery target evaluator | `0099f2a0` | `00749a80` | Cached-target path reconstructed; candidate search native |
| Artillery rating vtable slot | `00bf6378` | `00874778` | Guarded replacement |

The component-view helper dynamically casts an `IObject` to
`IPlayerControlObject`; if that fails, it follows the parent pointer at `0x50`
and retries until a match or null. That helper and the native RTTI runtime are
still dependencies of the source-owned predicate.

## Verification and limits

The focused verifier runs 1,536 paired original/recompiled cases per target.
It compares exact float32 result and table bytes, eligibility bytes, external call
order/arguments, receiver identity, the initial value of the entry-interface temporary slot, and
stack cleanup. It covers low-byte predicate
and eligibility values, recompute words, scale bits including signed zero and NaN,
signed-zero and rounding-boundary evaluator results, changing bot indices, and
rating/flag table relocation during callbacks.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_artillery.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

The complete `verify.ps1` includes these checks. The original component predicate
executes during comparisons; its conversion service and the target/fallback
evaluators are controlled fixtures. The comparisons prove driver routing and ABI,
not target-selection quality, ballistic aiming, firing timing, complete gameplay
or sustained multiplayer compatibility. Those systems still need recovered source
and tests that execute their decisions. No artillery live-match claim is made.

## Cached-target evaluator

`bfv_artillery_evaluate` now handles the evaluator's non-search path. Only the
low recompute byte selects a candidate search: `256` takes the cached path,
while `257` selects the native search. Its full interface has five stack arguments
(bot, recompute word, scale, gun, driver component), consumes 20 argument bytes,
and returns an unrounded x87 value. The older decompiler prototype omitted scale
and shifted later argument roles; it must not be used as a header.

The cached path first calls the behavior pattern's eligibility slot. Rejection
clears the bot flag and rating and returns zero. Otherwise it resolves the bot's
cached target handle through the generation-checked pool. A live target clears
the flag, submits the cached rating through bot slot `0x160`, and reads the rating
again for its return. A missing or stale target sets the flag, sends `ffffffff`
through bot slot `0x78`, calls the pattern reset slot, and clears the rating.

Callbacks may change bot indices, table pointers, and even the bot's vtable. The
original captures one vtable before the index callback and subsequently calls
its `0x160` slot; the reconstructed code preserves that ordering rather than
fetching the bot's replacement table. Float32 ratings are loaded into extended
precision, and the native search's 80-bit result is forwarded without a float or
double intermediate.

The added verifier runs 700 evaluator cases per target and eight driver/evaluator
integration cases. It covers valid/stale/empty/null pool entries, generation
mismatches, low-byte predicates and recompute values, NaNs/infinity/signed zero,
callback mutations, register preservation, stack cleanup, an empty x87 stack,
and selected cases across all rounding directions and precision settings.
Integration cases execute the real original/reconstructed cached evaluator from
the driver; external bot/pattern/component services remain controlled fixtures.
Candidate search is checked for argument and exact 80-bit result forwarding only.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_artillery_cache.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

## Target history

`src/target_history.c` reconstructs the timestamp lookup used by both candidate
passes. It searches the existing red/black map with unsigned handle comparisons.
An exact match returns the stored float pointer. A missing key allocates four
bytes, initializes the float to -10000, and passes the handle/pointer pair to
insertion, now reconstructed in `src/history_tree.c`. The helper returns its allocation even if insertion reports a duplicate;
substituting the existing value or silently freeing the allocation would change
original behavior.

The map is embedded at behavior offset `0x38`, with its header pointer at `0x3c`.
Node links are left/parent/right at offsets `0/4/8`, key at `12`, float pointer at
`16`, and sentinel flag at `21`. The native helper addresses are `0099f040` for
client and `00749820` for server. Insertion entries at `0099ef80` and
`00749760` now redirect to compiled C. The map must already be initialized by native lifecycle
code; this reconstruction does not add new null guards.

The builder now supports the required nonvirtual replacement: a guarded five-byte
jump at the original helper entry. It checks the complete original binary hash and
entry bytes, and records the exact jump and target in `entry_patches` in the build
manifest. Original instructions are skipped; compiled C consumes the same four
argument bytes and returns a pointer through EAX. Both native candidate passes
therefore use this source-owned helper, while their broader filtering/scoring
control flow remains native. Live validation checks that this jump is loaded.

432 focused comparisons per target execute the original tree traversal and the
new helper through that jump. Inputs include empty, balanced and skewed trees,
unsigned handle boundaries, duplicate keys, existing timestamps, and callback
changes to the header/timestamp and insertion-result flag. Allocation and insertion
are controlled services in these lookup-only tests; the separate tree tests below
execute actual insertion and balancing. Allocation failure, concurrent map access
and complete candidate-search decisions are not established by these tests.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_target_history.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

## History insertion and balancing

`src/history_tree.c` reconstructs six complete functions per binary. Insertion
orders handles as unsigned 32-bit values. It preserves existing values when a key
is duplicated, reports whether a node was inserted, and updates the map's count,
root, minimum and maximum. Red/black rotations and recoloring preserve balanced
search paths. Node construction copies the handle/value pair and preserves padding.

| Function | Client | Server | Stack argument bytes |
| --- | --- | --- | --- |
| Unique insertion | `0099ef80` | `00749760` | 8 |
| Node insertion and balancing | `0099ed10` | `007494f0` | 16 |
| Predecessor iterator | `0068ee10` | `0073f240` | 0 |
| Left rotation | `00996f10` | `005c7460` | 4 |
| Right rotation | `0066b6c0` | `0055c080` | 4 |
| Node constructor | `00517c10` | `007686e0` | 20 |

All use x86 `thiscall`. The map's header pointer is at `+4` and count at `+8`.
Node color is at `+20` (zero red, one black); the sentinel flag is at `+21`.
The constructor returns its receiver; insertion functions return their output
pointer. Iterator and rotation functions have no source-level return value.
These are shared native helper entries, so other callers also reach their C
replacements. Static audits verify complete indexed bodies, instruction-aligned
patches and no recorded incoming references into overwritten entry instructions;
they cannot prove absence of every computed jump.

The focused verifier passed **4,678 original/source comparisons per binary**.
It covers all permutations of five boundary keys, ascending/descending sequences,
random trees, repeated duplicates, every node's predecessor and the sentinel,
constructor aliasing, and timestamp lookup with real insertion. Each insertion
compares arena memory and allocations and checks ordering, parent links, count,
extremes, red-child constraints and equal black heights. ABI and x87 state are
checked on normal returns. The retained protected node allocator executes with
controlled raw heap allocation.

Five capacity-error cases per target verify the unsigned limit `0x1ffffffe`,
native string/exception call arguments and the throw object. Those services are
controlled in this test; real capacity-error unwinding is not established. Node
allocation protection, string/exception runtime, map initialization/deletion and
concurrent access remain native dependencies or unverified behavior.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_history_tree.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

The default verifier includes these comparisons. Address audits are recorded in
`reports/{client,server}/history-tree-audit.tsv`; hash-specific results are in
`history-tree-verification.json` in the same folders.

## First-pass weapon selection

`src/artillery_weapons.c` replaces the 394-byte inline loop at client
`0099f8d7..0099fa60` and server `0074a0b7..0074a240`. This is another stage of the
partially reconstructed evaluator, not an additional complete native function.
It scans the returned weapon vector, scores each non-null weapon and keeps the
first weapon with the greatest positive score; equal scores retain the earlier
selection. Rejected candidates do not receive the accepted-path score padding.

The recovered arithmetic is:

```text
available = returned count, with 0xffffffff replaced by 65536
score = class_rating / (1 + ((record_a - record_b) * 20 + 10) / available)
```

Other nonpositive signed counts produce zero. The signed integer class rating is
rounded to float32 before the calculation. The two record arrays are at offsets
`0x34` and `0x54`, indexed by weapon; their semantic meaning is still unresolved.
The implementation preserves extended intermediates and the original float32
stores with small x87 primitives. Modifying the formula is now possible in source.

Weapon parameter `+0x28` is compared with the candidate distance: a distance below
that threshold clears the score. Unordered distance comparisons retain the score;
unordered score comparisons never replace the best score. Parameter `+0x2c` is
tracked as a maximum across non-null weapons, including those with zero scores.
Its broader meaning is not claimed here. Accepted candidates have unused score
slots zeroed up to eight; the original scan itself has no added eight-weapon cap.

Inventory retrieval uses owner slot `0x1c`, the rating-table index comes from
target-component slot `0x3c`, and the availability word from weapon slot `0x24`.
Those object methods remain dependencies. Container pointers/counts are reread
after callbacks as in the native loop. The bridge reproduces its live frame and
EBX updates, preserves ESI/EBP, and resumes at the native accept/reject continuation.

The focused verifier passed **303 comparisons per target**, including empty/null
entries, equal scores, one to eight weapons, count sentinels, signed integer
boundaries, distance equality, NaN/infinity/signed zero, deterministic random
inputs, callback mutations and all supported x87 precision/rounding modes with
zero, two or six retained caller values. It compares the entire fixture arena
and evaluator frame, callback ordering/arguments, live registers and x87 state.
The three object methods are controlled services; arithmetic and loop decisions
execute actual original/reconstructed instructions. This does not establish the
remaining target evaluation, firing behavior or live-match equivalence.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_artillery_weapons.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

`AuditArtilleryWeapons.java` verifies both 98-instruction blocks, their owning
functions, instruction boundaries, outgoing branches and absence of recorded
external references into block interiors. The builder guards the original hash
and six overwritten entry bytes. Static references cannot rule out every computed
jump. Audit metadata is in `reports/*/artillery-weapons-audit.tsv`; the standard
verification command includes the new comparisons.

## Position helpers and post-weapon aiming gate

`aim_geometry.c` reconstructs four complete helpers per binary:

| Helper | Client | Server | Bytes / stack cleanup |
| --- | --- | --- | --- |
| `bfv_transform_point` | `0049b340` | `005b25f0` | 90 / 8 |
| `bfv_vector_difference` | `0049b490` | `0048d160` | 37 / 8 |
| `bfv_aim_world_position` | `009be440` | `0078df90` | 113 / 4 |
| `bfv_component_position` | `00994a80` | `0073f1e0` | 31 / 4 |

All use x86 `thiscall`. The transform and difference return the output pointer;
the two position wrappers write a vector and return the world-matrix pointer.
The transform reads all inputs before output stores, computing z, y, then x.
The world-position helper stores x immediately before evaluating y, then z.
Those orders and the distinct product/addition sequences are preserved through
small x87 primitives in the readable C implementation. Overlapping buffers can
therefore affect the helpers differently.

`artillery_aim_gate.c` replaces the 208-byte inline stage at client `0099fa61`
/ server `0074a241`. A nonzero record byte at `+0x14` sets frame `+0x74` to
`1 / (1 + (frame[+0x58] - record[+0x18]) * 0.05)` and accepts the gate.
The original does not clamp this result; field units remain unresolved.
Otherwise the weight is 1 and a nonzero driver at frame `+0x1fc` skips aiming.
The remaining path obtains component and target positions, transforms the record
point, subtracts the selected weapon's origin, and calls the recovered direction
predicate. Its accept/reject continuations are `0099fb31` / `009a0476` and
`0074a311` / `0074ac56`. The selected weapon is frame `+0xec`, the difference
output is `+0x1c4`, and the copied direction is `+0x1a4`. The component at `+0xf4`
is captured across callbacks; other pointers are reread at the native stages.

The focused suite contains 839 comparisons per target covering full fixture
memory, math/aliasing, callback ordering and mutations, ABI, and x87 state across
precision/rounding modes. Object methods and selected direction services remain
controlled dependencies. It does not establish complete trajectory or firing
behavior. Full-build evidence must match the hashes in the verification reports.

`AuditAimGeometry.java` passed on both supported originals: complete helper
intervals, exact overwritten instruction bytes, RET cleanup, owning evaluator,
51-instruction gate, and its accept/reject exits. It found no recorded references
into either the overwritten bytes or the block/function interiors from outside.
Audit metadata is in `reports/*/aim-geometry-audit.tsv`. Computed references and
live-match behavior are beyond this static audit.

## Initial movement and distance gates

`artillery_movement_gate.c` replaces the following 208-byte, 55-instruction
inline stage, client `0099fb31` / server `0074a311`. It captures frame `+0x30`
in live EDI, clears the vector at `+0x78`, and, if the receiver exists, copies
its slot-`0x14` vector after reading all three words. The receiver remains
captured even if a callback changes frame `+0x30`.

The special distance tests run only when source frame `+0x1f8` flags have bit 1
clear and target EBP flags have bit 1 set. The owner at frame `+0x50`, slot
`0x28`, supplies a low-byte predicate. With a zero low byte, the code compares
`frame[+0x1c] * 0.5` against distance `+0x28`, and, when required, compares
`sqrt(z*z + y*y + x*x)` with 15.0. One outcome writes score `+0x20` as zero.
With a nonzero low byte, it compares the parameter times the original float32
0.9 constant against distance; one outcome resumes the native score=1 store.
Unordered comparisons follow the original AH/parity masks, not ordinary C
relational operators. These fields' physical units remain unestablished.

| Continuation | Client | Server |
| --- | --- | --- |
| Later movement scoring | `0099fc01` | `0074a3e1` |
| Score already zero; remaining gates | `0099fe3b` | `0074a61b` |
| Native score=1 store | `0099fe33` | `0074a613` |

`AuditArtilleryMovement.java` passed on both originals, checking ownership,
the complete six-byte entry patch, all three continuations, instruction alignment
and recorded interior references. Metadata is in
`reports/*/artillery-movement-audit.tsv`. The differential suite has 521 cases
per target: vector overlap, callbacks mutating frame/object fields, predicate
low-byte boundaries, flag combinations, distance/vector boundaries, nonfinite
values, all supported x87 precision/rounding modes and retained caller values.
It compares full frame/arena memory, callback order, live registers and x87 state.
Movement-vector and predicate methods are controlled. Staged and current-EXE
focused comparisons passed, as did 16 current-hash live vector executions. The
movement-gate checkpoint subsequently passed its full suites and was exported.
The following scoring build passed full regressions on its own hashes.

## Following movement score

`artillery_movement_score.c` reconstructs the 570-byte, 123-instruction stage
at client `0099fc01` / server `0074a3e1`, ending at `0099fe3b` / `0074a61b`.
It consumes the movement receiver captured in EDI. A null receiver writes
score 1. When distance `+0x28` compares smaller/equal/unordered against parameter
`+0x1c`, the score is `1 / (1 + vector_length * 0.5)` using the receiver's
slot-`0x14` vector and the recovered vector-length helper.

The other path retrieves an event-2 scalar for driver `+0x1fc`, rounds it to
float32 at `+0xc0`, then rereads the driver pointer for its slot-`0x14` vector.
With no driver, the native temporary vector at `+0x11c` and scalar are cleared.
The vector is copied to `+0x88`. The captured movement receiver's event-2 scalar
minus `+0xc0` is compared with 5.0; smaller/equal/unordered results score 0.25.
The event/scalar units remain unestablished.

For the remaining path, the source computes `(0,1,0) cross frame[+0x64]` at
`+0x180`, copies it to `+0xb0`, then retrieves the captured receiver's vector.
Four dot products retain the original product/addition order on the x87 stack.
Their two differences are stored to float32 at `+0x38` and `+0x54`; the original
ordered-negative branches clear them to zero. Unordered results keep their
stored bits. The final score uses 0.25 divided by one plus half the vector length
plus twice the sum of those rounded projections. A returned vector may overlap
projection fields, so it is reread by the length helper after the stores/clamps.

`movement_geometry.c` also reconstructs two complete helpers per binary:

| Helper | Client | Server | Bytes / ABI |
| --- | --- | --- | --- |
| `bfv_vector_cross_assign` | `006eb030` | `00611430` | 86; ECX left, right stack arg, RET 4, EAX left |
| `bfv_component_event2_scalar` | `00970550` | `0072e7d0` | 23; ECX component, RET, ST0 scalar |

The in-place cross product snapshots the left vector, then stores x/y/z
immediately while rereading the right vector. Full/partial overlap therefore
affects later coordinates. The scalar wrapper captures component `+4` owner,
owner `+0x20` receiver, invokes receiver slot `0xa0` with event ID 2, then loads
returned event `+0x14` pointer's float at `+8`. Those loads and ABI are established;
the full event service remains a native dependency.

`AuditMovementScore.java` passed both originals, including complete helper bodies,
stack cleanup, patched instruction alignment, owning evaluator and exits.
The earlier movement gate has one recorded incoming branch to the score=1 store
at `0099fe33` / `0074a613`. That eight-byte native store remains an explicitly
retained alternate entry; the audit requires this exact instruction/reference.
There are no other recorded external interior references or references into the
overwritten entry bytes. Metadata is in `reports/*/movement-score-audit.tsv`.

Staged and installed-EXE payloads passed 645 comparisons per binary: 212 helper
and 433 scoring cases, including eight cases executing the previous gate and
this score together. Full fixture memory, callbacks, aliases, live registers and
x87 state are compared across exceptional inputs and precision/rounding modes.
Object methods are controlled; these results do not establish live-match parity.
This stage is now installed in both generated EXEs and passed 16 current-hash
live vector executions. Its full suites passed 36,765 client and 35,181 server checks. Hash-specific compact reports were exported. These live checks do not execute the movement
score in an actual match.

## Following distance, driver and query gate

`artillery_query_gate.c` reconstructs the 502-byte, 128-instruction stage at
client `0099fe3b` / server `0074a61b`, ending at `009a0031` / `0074a811`.
It compares parameter `frame+0x1c` times the native scale (about 0.89) against
distance `+0x28`, preserving x87 ordering and unordered branches. An ordered
smaller product takes the driver path: absent driver rejects; otherwise two
position callbacks supply x/z, event 2 supplies a raw word and slot 0x84 returns
a low-byte predicate. It captures the method table before coordinate stores and
rereads the receiver after the event callback.

The other path accepts immediately when the driver is present. With no driver,
it retrieves query data through the bot's component/interface methods, prepares
an origin and a vector of source/target handles, transforms the record point and
optionally adds predicted-minus-actual movement. The native x/y differences retain
extended precision; z is rounded before addition. Origin x is copied as raw bits,
while y/z pass through FLD/FSTP, including signaling-NaN conversion.

Slot 0x2c is a zero-argument identity getter. The original pushes two query
arguments before calling it; they belong to the following slot-0x50 query, whose
ABI has **ten stack arguments**. Its method table is captured before the identity
callback, while the receiver is reread afterward. Query results use AL alone.
Both query outcomes destroy the ignored-handle vector exactly once. Neither the
query-service semantics nor names/units of unknown fields are inferred here.

| Complete helper | Client | Server | Original bytes / cleanup |
| --- | --- | --- | --- |
| `bfv_component_event2_word` | `0092db30` | `006eb4b0` | 23 / RET |
| `bfv_component_query_data` | `00970440` | `00727d50` | 38 / RET or indirect tail jump |
| `bfv_query_vector_append` | `00975ab0` | `0072d870` | 77 / RET 4 |
| `bfv_query_vector_destroy` | `00490610` | `006ff4f0` | 42 / RET |

The event wrapper returns raw `event->data+4` bits. Query data uses object slot
0x34, interface conversion slot 0x0c and converted slot 0x3c; a null initial object
returns the native global fallback pointer, with no invented later null guard.
Append uses signed shifts followed by an unsigned size/capacity comparison and
retains the protected native insertion dependency on growth. Destroy frees the
captured begin pointer, then zeros begin/end/capacity even if the free callback
mutates them.

`AuditArtilleryQuery.java` passed both originals. It checks full intervals,
guarded entry bytes, ownership, branch exits, cleanup and recorded references.
The destructor has three executable ADD ESP,4 bytes omitted from Ghidra's indexed
body because its free routine is incorrectly marked no-return. The audit repairs
that listing/body in memory and discards project changes. Structural metadata is
in `reports/*/artillery-query-audit.tsv`; native bodies stay private.

Staged and installed EXEs passed **731 comparisons per binary**: 269 helper and
462 gate cases. These compare complete fixture memory, callbacks, aliases, ABI,
EDI and x87 control/status/retained values across finite/nonfinite boundaries and
precision/rounding modes. The point transform executes actual original/source
math; virtual services, protected growth and raw free are controlled. Current
hashes also passed 16 live vector executions, which do not execute this query in
an actual match. Full current-build regressions passed 37,496 client and 35,912 server checks;
matching compact reports were exported and a private checkpoint preserved.

The retained query insertion at `00972940` / `0072d640` matches the earlier vector
insertion's arithmetic/calls after address normalization, but omits four WAIT
instructions. That is a lead for shared source with distinct exception timing,
not permission to reuse the prior implementation unchanged. Reference listings
are private under `bfv-reference-local/artillery-query/bodies`.

## Following region helpers and score modifier

`region_geometry.c` implements raw x/z projection, symmetric-bound testing and
2D/3D region predicates. Projection stores x before loading z, so overlapping
buffers can change the second source word. Radial mode compares the native
four-register squared-distance/square-root sequence strictly against radius.
Symmetric bounds reject ordered less at the first two boundaries, retain the
reflected x boundary in extended precision, round reflected y to float32, then
compare x/y boundaries with the native unordered decisions. The 2D region uses
fields 0x114/0x118 as its first bound and 0x84/0x88 as the reflection center; these
labels describe the arithmetic, not recovered object ownership or world units.

| Helper | Client | Server | Bytes / ABI |
| --- | --- | --- | --- |
| `bfv_point_xz` | `0097f070` | `00728f20` | 13 / fastcall ECX point, EDX output, RET |
| `bfv_point_in_symmetric_bounds` | `009d5cc0` | `0078cd10` | 89 / fastcall ECX point, EDX bound, center stack, RET 4 |
| `bfv_region_contains_point2` | `00964ac0` | `0071c280` | 144 / thiscall region, point stack, RET 4 |
| `bfv_region_contains_point3` | `00964b70` | `0071c330` | 34 / thiscall region, point stack, RET 4 |

`artillery_region_score.c` implements the following 73-byte stage at
`009a0031` / `0074a811`, ending at `009a007a` / `0074a85a`. It copies score bits
from frame+0x9c to +0x98. When the driver and region exist and the actual region
predicate's AL is false, it rereads the source score and stores score times 0.75,
with native x87 rounding and no clamp. Both helpers and modifier execute actual
source/native arithmetic without numeric mocks.

Staged and installed EXEs passed 1,067 helper and 592 modifier comparisons per
binary, including projection overlap, signaling/nonfinite values, region/frame
aliases and all supported precision/rounding modes. The structural audits passed
complete bodies, instruction/patch boundaries, ABI cleanup and recorded incoming
references. Metadata is in `reports/*/region-geometry-audit.tsv` and
`artillery-region-score-audit.tsv`. The region build passed 76,726 full comparisons. Matching
hashes passed 16 live vector executions; actual-match region scoring, unmasked
exceptions and full region ownership/lifecycle remain unverified.

## Following category-list scoring stage

`list_search.c` and `artillery_category_score.c` translate the 40-byte iterator
helper at `009bf7c0` / `00775bf0` and following 383-byte score stage at
`009a007a` / `0074a85a`. The alternate target-flag path begins at `009a01f9` /
`0074a9d9` and remains native. The search captures the key once only for a
nonempty range, follows node+0 links and compares node+8 words, then stores the
returned iterator. An empty range does not dereference the key.

The stage captures the second category-list sentinel in EDI, obtains the start
node from the third retrieval and the comparison sentinel from the fourth.
It rereads the descriptor byte before each list call. Callback replacements can
therefore produce different sentinels. It increments the native frame index only
on the matching comparison path, capturing the bot's method table before that
store and rereading score/factor fields after the callback.

Distance/parameter is scaled and rounded before the existing maximum selector;
its result is rounded before the minimum selector. The attenuation remains in
ST0 while class, weapon, target and candidate factors are combined. The candidate
score is stored as float32 without popping ST0, then its extended value is
compared against the best score. Selection is strict; equality and unordered
reject. The winning identity is read after the best-score store, preserving alias
behavior. Object category/list/factor methods remain controlled in the tests.
The source is installed with two guarded patches per target; 611 focused
comparisons per target, 77,948 full comparisons and 16 current-hash live checks
passed. The complete iterator helper is registered; the 383-byte stage remains a
partial evaluator. `AuditCategoryScore.java` passed both originals.

## Remaining work

The direction aiming-limit predicate and its event wrapper now compile from C.
See [AIM-LIMITS.md](AIM-LIMITS.md) for recovered fields, boundaries and native
dependencies. Candidate scoring calls the replacement through guarded entries.
The full aiming controller and firing decisions remain incomplete.

The first candidate-pass inline filter now compiles from `artillery_filter.c`.
It replaces client `0099f6c3` / server `00749ea3` through a guarded seven-byte
entry patch and resumes at the original accept/reject continuations. It resolves
the generation handle, checks the candidate's returned identity against the
excluded ID, checks component/flag gates and signed record state, then compares
the history delta with the native 20.0 window. The meaning of that reference
value is not yet established as elapsed seconds. Native unordered comparisons
accept NaN; the C implementation preserves this behavior and x87 status.

The bridge preserves the evaluator's live frame and register outputs. Controlled
comparisons cover 171 cases per target, including callbacks changing frame state,
null/stale handles, wrapped addresses, infinity, NaN, and x87 precision/rounding.
The next weapon-selection stage is also reconstructed; later candidate scoring
and the second pass still require recovery.
Run `verify_artillery_filter.py` with the same arguments as the history verifier;
the default full verifier also includes these cases.

Recover the remaining target evaluator boundaries, arguments, candidate filtering,
weapon ratings, cached target state, and subordinate helpers. Then recover the
plans that turn selected targets into aiming and firing controls. Replace their
native dependencies and validate complete artillery scenarios against the original
engine. Object/component lifecycle, allocation and initialization also remain
necessary for a standalone source build.

## Alternate first-pass target path

`artillery_target_eligibility.c` reconstructs the complete 399-byte handle helper
at client `0097ede0` / server `00728c90`. It preserves generation checks, low-byte
virtual predicates, callback-dependent receiver rereads and linked traversal.
The event-3 component predicate was already reconstructed and is reused. Native
failed lookups can still fault on their following dereference; no new null guard
is inferred. The nine-byte entry guard ends at an instruction boundary. Both
structural audits and 176 focused comparisons per binary passed; its preserved
checkpoint passed 78,300 full comparisons and 16 matching-hash vector checks.

`artillery_alternate_gate.c` reconstructs the following 107-byte inline gate.
It tests flag bit 3, calls actual eligibility, rereads the current record, captures
the owner receiver/table, and initializes the nested score, metric and class.
The metric result is stored at frame+0x70: native PUSH shifts the apparent store
offset. It passed 164 staged and installed comparisons per target.

`artillery_nested_score.c` reconstructs the following 360-byte linked-object
loop. Component ratings accumulate at frame+0x5c; category/class/weapon factors
accumulate the candidate score at +0x14. Four list calls can return distinct
containers. Search uses the current handle at +0x54, and native traversal writes
its scratch output at +0x70 before the next generation lookup. The first category
and table are captured before the rounded sum store, including when the descriptor
aliases that store. Its 242 staged and installed comparisons per target cover
changing callbacks, linked targets, stale generations, exceptional float values and
all supported x87 precision/rounding modes. Both stage guards passed structural audits.

Object methods and component conversion remain controlled; pool lookup executes
retained native instructions. The combined gate/loop checkpoint passed 79,112 full comparisons
and 16 matching-hash live checks; consult the handoff and hash-specific reports.
Live vector checks do not establish artillery
behavior in a match.

The following 170-byte alternate scaling/selection and the complete 41-byte
traversal helper are now installed. They passed 329 + 154 staged and installed
comparisons per target and both structural audits. Scaling compares a signed
setting against the accumulated rating, skips the additional factor on unordered,
and selects only when the reloaded float32 score is strictly greater than the best.
Exact ties do not select in any of the twelve supported x87 control modes.
The setting field is read directly at bot+0x2c after scaling, including aliases.

The loop now calls `bfv_next_target_handle` directly. Its second stack argument is
volatile because native traversal reads it after the first callback; a supplied
scratch pointer can alias that argument. The wrapper preserves this read timing,
the captured receiver and both RET 8 paths. The combined checkpoint passed 80,078
full comparisons and 16 live checks; the helper is registered. The 170-byte finish is a partial stage,
not a complete inventory entry.

The following 29-byte iterator continuation and 93-byte second-pass setup are
installed. Their 157 + 160 focused comparisons per target and structural audits
passed. Node advance captures the next node and bot table before the frame store,
then compares against the list boundary returned by the callback. Query setup
uses a low-byte pattern predicate, initializes three vector fields and routes
by begin/end equality. It preserves the allocation pointer in EAX at the empty
cleanup continuation. Object methods are controlled; allocation, exception
unwinding, cleanup and subsequent filtering remain native or unverified. The
iterator/setup checkpoint passed 80,712 full comparisons and 16 matching-hash live
checks. Compact reports identify the verified EXE hashes.

The following 229-byte second-pass candidate filter is installed in
`artillery_second_filter.c`, at client `009a04f0` / server `0074acd0`, ending at
`009a05d5` / `0074adb5`. Both read-only structural audits passed; the six-byte
entry boundary has no interior references. Installed focused checks passed
355 comparisons per target. Its checkpoint passed 81,422 full comparisons and 16 matching-hash live checks.

The handle comes from the query iterator at frame+0x1c. Generation mismatches
and zero low indices reject without writing the candidate frame slot; a null
pool object does write zero. Identity callbacks can replace frame+0x18 before
the component and flag reads. Metric callbacks can replace the query iterator
and history receiver at frame+0xa4. History callbacks can change the reference
and timestamp before their x87 comparison. The masked FCOMP status rules retain
unordered behavior, precision, rounding and stack occupancy. Fixtures include
all twelve control modes and caller stack depths 0, 2 and 5. Identity, metric and
history services are controlled; history lookup has separate complete-function
coverage. These checks stop at accept/reject continuations and do not establish
later scoring, query cleanup or behavior in a match. This partial stage adds no
complete inventory entries.

The following 88-byte timestamp-weight stage is reconstructed in
`artillery_second_weight.c` and is installed. Both structural audits and 364
staged and installed comparisons per target passed. It captures the query iterator and bot
table, calls slot+0x1a4 with output frame+0x10c, and uses only the return's low
byte. A false result stores exactly 1 at frame+0x2c. A true result rounds the
scaled difference to float before clamp(0,value,1), then scales and subtracts
with an extended intermediate before the final float store. The tests execute
actual original/source clamp math; timestamp lookup is controlled. They cover
callback mutations, exceptional values, raw return bits, output/iterator alias,
rounding/precision modes and occupied stacks. The second-aim checkpoint passed 83,936 full comparisons and 16 live checks.
Partial stages do not add complete inventory entries.

The next aiming path uses the complete 128-byte normalization helper at client
`004a2a10` / server `00438d30`. `vector_normalize.c` is installed and passed
687 staged and installed comparisons per target plus both structural audits. It rounds squared
length before its tolerance tests, leaves near-unit vectors untouched, zeros
tiny vectors with EAX `ffffb1df`, and otherwise applies an extended reciprocal
square root. Unordered values reach arithmetic. The original overwrites its
saved ECX scratch slot, so ECX returns squared-length bits. Tests compare those
bits, EAX, rounded vector stores, nonvolatile registers, x87 flags/values and
constant-alias behavior. Full/live checks passed in the second-aim checkpoint; the helper is registered. Unmasked faults and overflowing caller x87 stacks are not covered.

The following 127-byte second-pass aiming gate is installed in
`artillery_second_aim.c`, ending at client `009a06ac` / server `0074ae8c`.
It passed 206 staged and installed comparisons per target and both structural
audits. Position, vector difference and normalization execute actual source
helpers. Tests compare full memory, captured component, callback-dependent
candidate reads, low-byte aiming result, bypass/accept/reject routes and x87
state. Object methods and final aiming predicate remain controlled. Its checkpoint
passed 83,936 full comparisons and 16 matching-hash live vector checks. These checks do not
establish target scoring or firing in a match.

The next 74-byte position retrieval and 152-byte distance/direction stages are
now installed. They passed 164 + 262 staged comparisons per target and
both structural audits. Position retrieval calls the actual source event-interface
helper, preserves the captured candidate and copies fallback coordinates in
sequence, including overlapping buffers. Distance arithmetic consumes the old
origin before overwriting a field, retains native store/rounding order and executes
actual length, maximum(length,0.5) and division helpers. Tests include callback
mutations, aliases, nonfinite/random inputs and x87 modes/occupied states.
Installed focused and current-hash live checks passed; full regression is pending; the following weapon
selection is also installed, while later scoring remains native. Neither partial stage adds complete inventory entries.

The following 298-byte second-pass weapon scan is now installed. Both structural audits and 294 comparisons per target passed. It uses
class_rating/(1 + 10/signed_available), converting -1 availability to 65536 and
zeroing nonpositive signed counts. It omits the first-pass history and distance
gates. Table capture precedes the rounded rating store, while method lookup
follows it. Best selection is strict; accepted padding leaves EBX at the actual
scan count. Tests cover callback-dependent container reads, table/vector/frame
aliases, exceptional float parameters and occupied x87 states. Inventory,
classification and availability methods are controlled; installed focused and
current-hash live checks passed, while full verification is pending and later scoring remains native.

The following 201-byte second-pass movement gate is reconstructed but staged
only. Both structural audits and 522 comparisons per target passed. It retains
captured movement/EDI, initializes and captures the velocity vector before
stores, rereads callback-dependent target/source fields, applies low-byte pattern
routing and evaluates magnitude as x*x + z*z + y*y. Tests cover aliases,
callback mutations, flags, nonfinite inputs and occupied x87 states across
rounding/precision modes. Object methods remain controlled; installed/full/live
checks are pending. The following movement score has distinct captured-driver
and live-register behavior and remains unreconstructed.

## Continuing native analysis

`analysis/tools/ghidra/InspectArtilleryEvaluator.java` applies verified driver and
evaluator argument positions to a hash-matched Ghidra program in memory, corrects
the allocator/release declarations, and exports an updated local native analysis.
Run it with `-readOnly`; it does not save changes to the analysis project.

The default Microsoft calling-convention model incorrectly assigns a hidden
return pointer to a `float10` return. The script explicitly assigns ECX, the five
stack arguments and ST0, then checks those storage positions before decompiling.
The resulting storage metadata is published in `reports/*/artillery-abi.tsv`.
Both inspected programs successfully decompiled with these corrections, but
unresolved object layouts, callee types and stack aliases still produce unreliable
expressions. A successful export is not verified reconstructed source.

```powershell
& "$env:GHIDRA_HOME/support/analyzeHeadless.bat" 'D:\Analysis\Projects' BFVBinary -process BfVietnam.exe -noanalysis -readOnly -scriptPath "$PWD/analysis/tools/ghidra" -postScript InspectArtilleryEvaluator.java 'D:\Analysis\Artillery'
```

For the server use the corresponding project and `bfvietnam_w32ded.exe`. Keep the
exported native bodies local. Use instructions and differential/runtime evidence
to reconstruct the remaining candidate search rather than compiling the export.

The diagnostic additionally repairs two missing continuations after heap release.
The original function body excluded 31 executable bytes and falsely exited during
stale-target cleanup or candidate-buffer release. The script checks the release
calls and final `RET 0x14`, clears their erroneous flow overrides, disassembles the
continuations, and restores the verified 8,269-byte interval. Both binaries passed
these guards and decompiled successfully. Counts are published in
`reports/*/artillery-body-recovery.tsv`; the native bodies remain local research.


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

The second movement gate and score were subsequently installed. Installed focused
checks passed 522 + 439 per target and matching-hash live vector checks passed
16 executions total. New full regression remains pending; public installed
reports describe the preserved second-weapons checkpoint. See the current
handoff for sessions/hashes. These inline stages do not add complete functions.

Second-pass movement checkpoint subsequently passed 87,298 full comparisons
and 16 matching-hash live checks. The following second-pass driver predicate
is staged separately: 111 bytes, 338 comparisons per target and structural
audits. It preserves two target position calls, captured driver/receiver/table,
x87 coordinate stores and low-byte predicate result with actual event-word
helper. Object callbacks are controlled; full evaluator behavior is unverified.

The second-pass region modifier is now installed. It passed 593 focused comparisons
per target and the region checkpoint passed 89,160 full comparisons across both
binaries with matching live checks. The surrounding category and target-state
phases remain native or unverified.
