# Bailout rating reconstruction

`bfv_bailout` in `bfv-native-code/src/native_ai.c` implements both branches
of the inspected `BBBailOut` slot-9 method in C. Its numerical helpers are now
implemented in `src/numeric_curves.c`; it no longer calls the original bailout
routine or the original two curve helpers. The client entry is `00984c20`; the server entry is
`0072eef0`. Supported binary hashes are in [SETUP.md](SETUP.md).

## Recovered behavior

The method receives `this` in ECX, then an object pointer, a recomputation flag,
and a float scale on the stack. It returns through x87 ST0 and pops 12 argument
bytes. Only the low byte of the flag determines recomputation.

With recomputation disabled, it obtains the object's selector and returns the
cached rating. With recomputation enabled, it:

1. Resolves the object's generation-checked handle through the native object pool.
2. Starts with influence 1. If two object predicates permit it, reads an influence
   float from the returned record.
3. Calls the entity component's metric method, computes `1 - metric`, and stores
   this intermediate as float32.
4. Calls the first numerical curve helper, multiplies by influence and 100, then
   stores another float32 intermediate before calling the second helper.
5. Multiplies the helper result by the input scale and stores a float32 rating in
   the selector's cache slot.
6. Queries the selector again and sets its pattern flag, then queries once more
   to obtain the returned cached rating. These three selectors can differ.

The editable `bfv_bailout_rating` rule applies to the returned value after this
sequence, preserving the existing modification interface.

## Recovered layout and dependencies

| Interface | Offset | Observed use |
| --- | --- | --- |
| Behavior field | `+04` | Group byte passed to object predicate |
| Behavior field | `+0c` | Pattern object |
| Behavior field | `+1c` | Float rating cache |
| Entity field | `+24` | Metric component pointer |
| Component virtual method | `+10` | x87 metric return |
| Object virtual methods | `+70`, `+6c` | Influence predicates; low-byte results |
| Object virtual method | `+cc` | Entity handle |
| Object virtual method | `+dc` | Cache/pattern selector |
| Object virtual method | `+e0` | Influence record, float at `+08` |
| Pattern field | `+04` | Selector-indexed byte flags |

Names describe observed use; the original field names and the metric's gameplay
meaning have not been recovered. Reconstructed numerical helpers correspond to
client `009d7200` / `009d6b90`, server `0078c5f0` / `0078bf80`.
They take one float stack argument, pop four bytes and can return unrounded x87
values. C preserves extended precision between the explicit float32 stores.
The 26-byte pattern helper's selector/flag operation is reconstructed inline.
Object methods, the object pool, and table storage/initialization remain supplied
by the original engine. Replacing the helper algorithms does not replace those
services or every other original caller of these helpers.

## Numerical helpers

The first helper clamps inputs at or above 1 to 1 and negative inputs to 0. The
second uses table interpolation from 0 through 1, then a linear extension above 1.
Both otherwise use 100 bins: truncate `value * 100` to obtain the fractional
weight, separately truncate `value * -100` for the table address, and blend two
neighboring float32 entries while retaining x87 arithmetic precision.

The two products must remain separate under directed rounding. The build uses
`-frounding-math` to prevent replacing the second multiplication with a sign
change. The extension coefficient is stored as float32 before promotion; evaluating
a C literal with excess precision can otherwise change the native coefficient.

The original integer-conversion runtime rounds to int64 using the current x87
mode, then adjusts toward zero from a float32 residual. `truncate_index` recovers
that adjustment in C, with a two-instruction inline-assembly primitive for the
hardware int64 conversion. It preserves the masked-invalid conversion's low-word
result instead of relying on undefined C casts of NaNs.

Table initialization is still native: client `009d6c20` / `009d65b0`, server
`0078c010` / `0078b9a0`. Each allocates 101 float entries. At exactly 1 the score
helper also reads the next word and multiplies it by zero; if that word is NaN,
it can affect the result. The reconstructed helper preserves this access.

`bailout_oracle.py` executes the original and rebuilt routines with the same
controlled objects and deterministic curve-table fixtures. The reference run uses
the original numeric and pattern helpers; the rebuilt run uses reconstructed math.
It compares returned float32 bits, all rating-cache slots, pattern flags, virtual
call order, the group argument and stack cleanup. It covers low-byte predicate
results, conditional influence, metric boundaries, signed zero, scaling and
selectors that change between calls.
Additional fixtures move cache and flag-table pointers inside selector callbacks,
checking that field reads occur in the same order as the native instructions.

`curve_oracle.py` separately captures the full 80-bit helper return, checks stack
cleanup and an empty x87 stack, and checks that the control word is preserved.
Its inputs cover neighboring float32 values, signed zeros, subnormals, infinities,
quiet/signaling NaN payloads and a NaN neighboring table word. It exercises all
four rounding directions at 24-, 53- and 64-bit arithmetic precision. Synthetic
tables and tables produced by the original initializers are both compared;
original table data is regenerated locally and is not included in the repository.

These fixtures do not prove the meaning of the real gameplay tables or behavior
under invalid handles, unmasked floating-point exceptions or a sustained match.
The end-to-end bailout fixtures use finite inputs and the default control word;
nonfinite and alternate-mode tests are scoped to the curve helpers. Floating-point
status flags beyond control-word/stack preservation are not compared.
The component lookup preserves the original exceptional address-wrap
branch; it does not silently repair invalid native objects.
