# Aimer

`bfv-native-code/src/aimer.c` reconstructs the four methods of the bot `Aimer`
(lifted source `bf/AIBotActionPlan/Aimer.c`): target tracking, position
prediction, projectile offset and the iterative ballistic solve.

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_aimer_track` | `00a02a70` | `007b7090` | thiscall position pointer, velocity pointer, float time; AL boolean, RET 0Ch |
| `bfv_aimer_predict` | `00a02bc0` | `007b71e0` | thiscall float time, output pointer; RET 8 |
| `bfv_aimer_projectile_offset` | `00a02c30` | `007b7250` | thiscall output, direction pointer, float time; RET 0Ch |
| `bfv_aimer_solve` | `00a02db0` | `007b73d0` | thiscall float time, float search scale, byte high arc, direction, target, optional aim-time output; AL boolean, RET 18h |

Client and server bodies are instruction-identical apart from addresses.

Field names come from how the code uses each offset. `drag` (offset 0) is the
coefficient `k` in the drag formulas; `gravity` (8) and `speed` (0Ch) come from
the solver math. The object also holds the start and last sample times (10h/14h),
three position/velocity pairs from 18h, two zeroed vectors (60h/6Ch), the
sample count (78h) and half the last flight time (7Ch).

- **Track:** ignores a sample whose time is not later than `last_time` plus an
  epsilon (only once a sample exists). Otherwise it copies position and velocity
  into all three pairs, sets both times, zeroes the two vectors and counts the sample.
- **Predict:** linear extrapolation from `start_time`. The x product stays in
  extended precision; y and z round to float32 before the add, as in the original.
- **Projectile offset:** separate drag-free and drag paths. Drag is reread for the
  final scale and the output z is reread before the last add, so aliasing behaves natively.
- **Solve:** predicts the target, starts from `atan2` of its height over horizontal
  distance (or the high-arc midpoint), then steps the elevation angle for
  `_ftol2(search_scale*8+2)` refinements, at most 400h passes. It re-predicts the target
  at each flight time, fails on a non-finite distance or height (`_finite`), halves
  the step when the error changes sign, and clamps to the native angle-bit limits. On
  success it writes the unit direction and checks its squared length, then writes the aim time.

Control flow is plain C; small x87 helpers keep compare status, trig and `_ftol2`
truncation identical. `_ftol2` and `_finite` stay retained runtime dependencies.

`verify_aimer.py` runs original and rebuilt entries from FNINIT with all 12
precision/rounding control words. Each target passed 1570 cases: 149 track, 66
predict, 155 projectile offset and 1200 solve (786 of them succeed). They cover the
sample gate, special and non-finite times, aliasing into the object, zero/negative/NaN
drag, high arc, zero/negative search widths and a null aim-time output. Each case
compares AL, return FPU status, object and output memory, the trace ring, ESP and
saved registers. The default full verifier includes these cases.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_aimer.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

For a build made with `--output`, add `--work '{target}-custom'`.

This is the aim math only. Who calls the Aimer, target choice and firing
decisions remain native and unverified.
