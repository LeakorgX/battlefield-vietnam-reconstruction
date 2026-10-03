# Artillery AI reconstruction

`bfv-native-code/src/artillery.c` reconstructs the rating stage of
`BBFireArtilleryDriver`. The client method at `009a13a0` and server method at
`0074bb80` are replaced in the generated EXEs. The underlying target evaluator
now has source-owned cached-target validation. Candidate search still executes
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
bytes, initializes the float to -10000, and passes the handle/pointer pair to native
insertion. The helper returns its allocation even if insertion reports a duplicate;
substituting the existing value or silently freeing the allocation would change
original behavior.

The map is embedded at behavior offset `0x38`, with its header pointer at `0x3c`.
Node links are left/parent/right at offsets `0/4/8`, key at `12`, float pointer at
`16`, and sentinel flag at `21`. The native helper addresses are `0099f040` for
client and `00749820` for server. Native insertion remains at `0099ef80` and
`00749760`, respectively. The map must already be initialized by native lifecycle
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
are controlled services; balancing, allocation failure, concurrent map access and
complete candidate-search decisions are not established by these tests.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_target_history.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

## Remaining work

Recover the large target evaluator's boundaries, arguments, candidate filtering,
weapon ratings, cached target state, and subordinate helpers. Then recover the
plans that turn selected targets into aiming and firing controls. Replace their
native dependencies and validate complete artillery scenarios against the original
engine. Object/component lifecycle, allocation and initialization also remain
necessary for a standalone source build.

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
