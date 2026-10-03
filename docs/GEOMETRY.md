# Reconstructed collision geometry

`src/collision_geometry.c` contains readable implementations of two original
helpers. The AI collision dispatcher now calls this C instead of native geometry.
Original entries remain intact for any other callers still in the native image.

| Function | Client | Server | Observed calculation |
| --- | --- | --- | --- |
| `bfv_line_distance_squared` | `009d4640` | `00789fa0` | Squared point-to-line distance |
| `bfv_collision_distance` | `009d46d0` | `0078a030` | Point-to-segment distance in the X/Z plane |

Both use x86 `stdcall`, three vectors by value, 36 bytes of stack arguments,
and an unrounded x87 ST0 return. Vectors contain raw float32 words; the shared
header preserves their 12-byte layout. Names describe the calculation rather
than recovered original identifiers.

The line helper forms the cross product of point-minus-origin with direction,
squares its components, and divides by the squared direction length. The first
two cross components are stored as float32; the third remains extended precision.
The addition order is preserved. A zero direction retains the original invalid
division behavior; this implementation does not invent a fallback.

The segment helper first stores direction components as float32. It retains
the native Y subtraction even though the geometric result discards height. It tests the
point's projection from each endpoint. A point inside uses the line helper with
all Y components zero, then takes the square root. A point outside uses the
appropriate endpoint's X/Z distance. Height is ignored. A zero-length segment
can reach the original invalid division and return NaN. Unordered projection
comparisons take the observed endpoint path.

Readable C expresses the arithmetic and branches. Volatile float and long-double
intermediates preserve observed storage and operation ordering with the existing
`-frounding-math` build. A short `fsqrt` primitive uses the original hardware
operation directly without introducing a runtime math-library dependency.

## Verification

`geometry_oracle.py` and `verify_geometry.py` compare 2,136 cases per binary.
They execute original and compiled instructions and capture all 80 return bits.
They also check stack cleanup, empty x87 register stack, and control-word
preservation. Fixtures exercise the four rounding directions and three precision
modes, endpoint/interior paths, zero-length segments, deterministic random vectors,
float32 extremes, subnormals, signed zeros, infinities and selected NaN payloads.
Unmasked exceptions, status flags and exhaustive combinations are not verified.

The dispatcher oracle additionally executes ten cases with real geometry, rather
than substituting a distance result, while controlling object/event interfaces.
This verifies the compiled dispatcher/helper boundary and resulting event effects.
It does not establish the engine's physical collision detection/response or
whole-match equivalence.
