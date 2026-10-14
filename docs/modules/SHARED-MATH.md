# Shared scalar and vector math

`bfv-native-code/src/scalar_vector_math.c` reconstructs five complete native
helpers used by artillery and other engine code. The build installs guarded
entry jumps at their original addresses, so existing native callers use the
compiled source implementations.

| Source function | Client | Server | Calling convention |
| --- | --- | --- | --- |
| `bfv_float_minimum` | `00422f30` | `005a3340` | stdcall, two float arguments, ST0 return |
| `bfv_float_maximum` | `00422f90` | `004ac530` | stdcall, two float arguments, ST0 return |
| `bfv_float_clamp` | `004a94a0` | `006f0470` | stdcall, lower/value/upper floats, ST0 return |
| `bfv_vector_length` | `0049aac0` | `0048c2b0` | thiscall, pointer to three floats in ECX, ST0 return |
| `bfv_vector_divide` | `005184c0` | `0048caf0` | thiscall, one float divisor, modified vector pointer in EAX |

For finite inputs the selectors return the minimum/maximum or clamp the value.
Equality and unordered comparisons have specific native selection rules:
minimum returns the right operand on equality or NaN; maximum returns the left.
Clamping first tests the lower bound, then the upper, even for reversed bounds.
These rules differ from assuming a generic standard-library min/max operation.

Vector length computes `sqrt((x*x + y*y) + z*z)` using extended intermediates.
Vector division computes one extended reciprocal and multiplies each component,
rounding each output separately to float32. It does not guard a zero divisor.
Replacing that sequence with three ordinary divisions can change rounded results.

The public logic is C. Small inline x87 primitives preserve operand loading,
NaN propagation, status flags and precision/rounding behavior that C expressions
alone do not guarantee. These helpers contain no calls to original engine code.
The surrounding executable still does.

`verify_scalar_vector_math.py` runs the actual original and replacement entries.
No numeric services are mocked. It compares extended returns, vector memory,
calling conventions, nonvolatile registers and x87 status. Inputs include signed
zero, subnormals, large finite values, infinities, quiet/signaling NaNs, reversed
bounds, all supported precision/rounding modes, and two retained caller x87 values.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_scalar_vector_math.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

The default verifier also includes these comparisons. This establishes the
tested helper behavior, not complete physics, artillery scoring, live-match
stability or a standalone engine. Unmasked floating-point exceptions and
asynchronous exception handling need separate native Windows evidence.

The inspected client and server each passed 7,369 shared-math comparisons.
`reports/*/shared-math-callers.json` lists distinct direct caller functions found
in the static call graph. It helps locate dependent code; indirect/unindexed
callers and complete runtime coverage are not established by that index.
