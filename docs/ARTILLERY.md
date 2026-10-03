# Artillery AI reconstruction

`bfv-native-code/src/artillery.c` reconstructs the rating stage of
`BBFireArtilleryDriver`. The client method at `009a13a0` and server method at
`0074bb80` are replaced in the generated EXEs. The underlying target evaluator
still executes original machine code; this is not yet complete artillery AI.

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
| Artillery target evaluator | `0099f2a0` | `00749a80` | Native |
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

## Remaining work

Recover the large target evaluator's boundaries, arguments, candidate filtering,
weapon ratings, cached target state, and subordinate helpers. Then recover the
plans that turn selected targets into aiming and firing controls. Replace their
native dependencies and validate complete artillery scenarios against the original
engine. Object/component lifecycle, allocation and initialization also remain
necessary for a standalone source build.
