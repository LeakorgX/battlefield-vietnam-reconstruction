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
