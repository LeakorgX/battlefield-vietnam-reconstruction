# Affine transform composition

`bfv-native-code/src/affine_matrix.c` reconstructs the entire client helper at
`0049acd0` and server helper at `00438590` (334 indexed bytes each). Both use
thiscall: ECX is the destination, two stack arguments are input matrix pointers,
RET 8 cleans those arguments, and EAX returns the destination.

Matrices contain sixteen row-major floats. For columns 0–2, the basis is the
three-term product of the left row and right column. Row 3 also adds the right
translation. Column 3 is written as `0,0,0,1`; input values in that column do not
participate in the calculation. This is affine composition, not arbitrary 4×4
multiplication.

The C loop keeps the original column-first output order and the original
per-element product/addition order. The small x87 primitive retains extended
intermediates, operand order for NaNs, and individual float32 output rounding.
It reads inputs immediately before each output store. Copying inputs up front
or changing the store order changes behavior when buffers overlap.

| Output row | Column 0 product order | Column 1 product order | Column 2 product order |
| --- | --- | --- | --- |
| 0 | x, y, z | y, x, z | x, z, y |
| 1–3 | z, y, x | z, y, x | x, z, y |

Each row's products are accumulated left to right in this order. The source also
retains which operand is loaded first for each multiplication, which matters
when both operands are NaNs.

The helper calls no original engine/runtime code. Existing native callers enter
through a guarded jump; the reconstructed aiming predicate calls the C function
directly through `affine_matrix.h`. This removes its native matrix-composition
dependency. Inverse sine and other engine services still remain native.

`verify_affine_matrix.py` compares the actual original and compiled entries,
without mocked numeric services. It checks the complete backing memory, return
pointer, stack cleanup, preserved registers, x87 control/status, and retained
caller values. Cases cover finite/special values, deterministic random bit
patterns, ignored homogeneous input fields, nine buffer layouts, all supported
precision/rounding modes, and two or six occupied caller x87 registers.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_affine_matrix.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

The default verifier also runs the matrix comparisons and aiming integration
cases. Passing these checks does not establish full physics, rendering, aiming,
live-match stability or a standalone engine build. Unmasked floating-point
exceptions and invalid/unmapped pointer behavior are outside this test scope.
