# Direction aiming limits

`bfv-native-code/src/aim_limits.c` reconstructs two complete native predicates
used by artillery candidate scoring and another indexed engine caller.

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_aim_direction` | `0099eae0` | `007492c0` | thiscall component, direction pointer, AL boolean |
| `bfv_aim_within_limits` | `009be880` | `0078e3d0` | thiscall returned view, direction pointer, AL boolean |

The outer function obtains the component owner's receiver and dispatches event 5.
It passes the returned view to the angular predicate. Event dispatch consumes
only the event ID. The original prepares the direction argument on the stack
before dispatch, then reuses it for the following predicate; it is not a second
event argument.

The predicate captures the view's mount, asks its owner for a world transform,
and composes it with the mount transform at offset `0x80`. It then:

1. Accepts the exact full-range bit pair `-π`/`+π` at mount offsets `0x68`/`0x74`.
   Transform composition still runs before this shortcut.
2. Projects the supplied direction onto the composed first row, clamps it to
   `[-1,1]` in extended precision, rounds to float32, and calls inverse sine.
3. Projects onto the third row. A negative result selects the opposite
   half-plane: `sign(angle) * float32_pi - angle`.
4. Tests the captured mount's lower and upper angle limits, including endpoints.

The predicate does not normalize the supplied direction. Exact comparison,
NaN and rounding behavior follows the inspected instructions. Native unordered
range comparisons accept NaN; this is retained rather than silently corrected.
It also preserves the incoming pointer slot's reuse as a temporary while keeping
the direction pointer needed by the second projection.

The control logic is editable C; small x87 primitives preserve operation order
and status flags. Matrix composition now calls [reconstructed C](AFFINE-MATRIX.md)
directly; inverse sine and the original runtime remain dependencies.
This is not the full artillery aiming controller, trajectory
solver or firing behavior.

| Math helper | Client | Server | Recovered interface |
| --- | --- | --- | --- |
| Source-owned affine composition | `0049acd0` | `00438590` | thiscall destination, two 16-float matrix pointers, RET 8 |
| Clamped inverse-sine wrapper | `00516f80` | `0048b570` | stdcall float32 input, ST0 return, RET 4 |

The composition produces the 3×3 basis and translation row, then writes the
homogeneous column as `0,0,0,1`. Its per-element arithmetic order and direct
write order must be retained when recovering it, including any input/output
aliasing. `reports/*/aim-abi.tsv` records the two reconstructed entries, inspected
body sizes and indexed direct callers.

`verify_aim_limits.py` compares both original and replacement predicates, including
event dispatch and stack cleanup. It covers endpoints, reversed/full/NaN bounds,
zero and nonfinite projections, opposite half-planes, callback pointer/limit
mutations, x87 status and retained caller values. Selected cases execute the
actual original/source matrix composition and native inverse-sine runtime. Controlled trig cases
cover unusual return values without claiming CRT exceptional-path equivalence.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_aim_limits.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

The default full verifier includes these cases. Original-game complete-match
behavior and unmasked/asynchronous floating-point exceptions remain unverified.
Each inspected binary passed 782 aiming cases: 698 controlled-dependency cases,
60 with actual inverse sine, and 24 with actual matrix composition and inverse sine.
