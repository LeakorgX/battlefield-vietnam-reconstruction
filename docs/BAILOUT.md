# Bailout rating reconstruction

`bfv_bailout` in `bfv-native-code/src/native_ai.c` now implements both branches
of the inspected `BBBailOut` slot-9 method in C. It no longer calls the original
whole bailout routine. The client entry is `00984c20`; the server entry is
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
meaning have not been recovered. The two curve helpers remain original machine
code: client `009d7200` / `009d6b90`, server `0078c5f0` / `0078bf80`.
They take one float stack argument, pop four bytes and can return unrounded x87
values. C preserves extended precision between the explicit float32 stores.
The 26-byte pattern helper's selector/flag operation is reconstructed inline.

`bailout_oracle.py` executes the original and rebuilt routines with the same
controlled objects and deterministic curve-table fixtures. The real original
numeric helpers and original pattern helper execute in the reference run.
It compares returned float32 bits, all rating-cache slots, pattern flags, virtual
call order, the group argument and stack cleanup. It covers low-byte predicate
results, conditional influence, metric boundaries, signed zero, scaling and
selectors that change between calls.
Additional fixtures move cache and flag-table pointers inside selector callbacks,
checking that field reads occur in the same order as the native instructions.

These fixtures do not prove the meaning of the real gameplay tables or behavior
under invalid handles, nonfinite inputs, every floating-point mode or a sustained
match. The component lookup preserves the original exceptional address-wrap
branch; it does not silently repair invalid native objects.
