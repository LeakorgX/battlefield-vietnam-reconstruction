# dynamic_cast wrappers

638 complete native functions have individual editable C implementations:
344 client and 294 server entries in `bfv-native-code/src/dynamic_casts.c`,
catalogued in `bfv-native-code/dynamic-casts.json`.

Each wrapper calls the MSVC runtime cast
`__RTDynamicCast(object, 0, &source_type, &target_type, 0)` (client `007c334d`,
server `0063cbf8`). The two pushed constants are RTTI TypeDescriptors, so every
entry is named by the classes it converts between, for example
`dice::WType -> dice::bf::GameModeManager`.

| Kind | Native behavior | Client | Server |
| --- | --- | ---: | ---: |
| `holder` | read the pointer stored at the stack argument; return its cast (`ret 4`, ECX unused) | 161 | 153 |
| `handle` | `this` is a handle: return 0 if empty, the raw pointer if the flag byte is clear, otherwise the cast | 130 | 100 |
| `handle_arg` | as `handle`, but the handle is the stack argument (`ret 4`, ECX unused) | 45 | 33 |
| `pointer` | cast the stack argument itself (`ret 4`, ECX unused) | 6 | 6 |
| `self` | cast `this` (ECX) | 2 | 2 |

```c
/* Native 0052bfd0; dice::WType -> dice::bf::GameModeManager. */
void *TC bfv_client_dynamic_cast_0052bfd0(const void *handle)
{
    void *object = ((const volatile struct BfvPointer *)handle)->value;
    if (!object || !((const volatile struct BfvFlag *)((const unsigned char *)handle + 8u))->value)
        return object;
    return BFV_DYNAMIC_CAST(object, 0, (const void *)0x00cb3a10u, (const void *)0x00cad0f0u, 0);
}
```

The runtime cast and the class layouts stay native. What the handle's flag byte
means is not yet known.

## Evidence

`analysis/tools/recover_dynamic_casts.py` generates the source and catalog from
the original EXEs (supported hashes only). It accepts a function only when:

- the indexed bytes are exactly one of the five shapes and the call goes to the
  runtime cast;
- both pushed constants are TypeDescriptors (type_info vtable, zero spare word,
  decorated `.?A...@@` name);
- the entry passes the same detour audit as the
  [field accessors](FIELD-ACCESSORS.md).

Rejected: client 3 interior-pointer cases.

The compiled body is not byte-identical. `bfv-native-code/tools/verify_dynamic_casts.py`
(part of `verify.py`) checks ABI equivalence. It stubs the runtime cast with a
recording hook and runs original and reconstructed entries side by side over six
scenarios: empty and non-empty pointers, clear and set flag bytes, null and
non-null cast results, plus varied CPU flags and x87 stack depths. It requires:

- the same data reads and the same casts with all five arguments;
- the expected EAX for each scenario, and the same stack cleanup (4 bytes for
  `handle` and `self`, 8 for the stack-argument kinds);
- EBX/ESI/EDI/EBP preserved at the call and on return, unchanged x87 state, and no
  writes outside the function's own frame.

ECX, EDX and flags are scratch and are not compared. Current result: client
2,064 and server 1,764 comparisons.

## Not converted (yet)

Other cast wrappers (reference casts, casts with a non-zero `vfdelta`, and
wrappers that adjust the result).
