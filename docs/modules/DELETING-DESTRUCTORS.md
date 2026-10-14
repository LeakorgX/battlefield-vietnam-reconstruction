# Scalar deleting destructors

691 complete native functions have individual editable C implementations:
407 client and 284 server entries in `bfv-native-code/src/deleting_destructors.c`,
catalogued in `bfv-native-code/deleting-destructors.json`.

MSVC gives every polymorphic class a *scalar deleting destructor*, the function
in its virtual destructor slot. It destroys the object and frees it when bit 0
of its argument is set (`delete p` versus an explicit destructor call):

| Kind | Native behavior | Client | Server |
| --- | --- | ---: | ---: |
| `destroy` | call destructor; free if `flags & 1`; return object | 308 | 212 |
| `vtable` | store class vtable (inlined trivial destructor); free if `flags & 1` | 55 | 35 |
| `vtable_destroy` | store class vtable; call base destructor; free if `flags & 1` | 44 | 37 |

```c
void *TC bfv_client_deleting_destructor_00403d50(void *object, uint32_t flags)
{
    ((volatile struct BfvVtableSlot *)object)->value = 0x00b427f4u;
    ((BfvDestructor)0x00403bd0u)(object);
    if (flags & 1u)
        BFV_DEALLOCATE(object);
    return object;
}
```

All accepted bodies free through one shared 83-byte deallocator (client
`00403680`, server `00403df0`). That it is the game's `operator delete` is
likely but not proven, so it is named `BFV_DEALLOCATE`. Destructors, vtables
and classes stay native and unnamed.

## Evidence

`analysis/tools/recover_deleting_destructors.py` generates the source and
catalog from the original EXEs (supported hashes only). It accepts a function
only when:

- the bytes at the indexed entry are exactly one of the three shapes. Ghidra's
  indexed size stops 3 bytes early, before the final `ret 4`, and the next
  indexed function must start at or after the real end;
- the free call goes to the shared deallocator;
- the entry passes the same detour audit as the
  [field accessors](FIELD-ACCESSORS.md).

Rejected: client 3 and server 1 interior-pointer cases.

The compiled body is not byte-identical. GCC keeps the object in EBX and calls
through EAX. `bfv-native-code/tools/verify_deleting_destructors.py` (part of
`verify.py`) checks ABI equivalence instead. It stubs the destructor and the
deallocator, then runs original and reconstructed entries side by side for six
flag arguments (`0`, `1`, `2`, `3`, `fffffffe`, `ffffffff`), four object
alignments, and varied CPU flags, x87 stack depths and scratch values. It requires:

- the same ordered events: vtable store (address, size, value), destructor call
  with `ECX` = object, and a deallocation of the object exactly when bit 0 is set;
- EAX = object, the 4-byte argument popped, and EBX/ESI/EDI/EBP restored on
  return (the native body legitimately holds the object in ESI across its calls);
- unchanged x87 state and no other writes outside the function's own frame.

ECX, EDX and flags are scratch and are not compared.

## Not converted (yet)

- Variants that destroy a member subobject first (`lea ecx,[esi+K]` / `call`),
  and vector deleting destructors.
- Adjustor thunks (`sub ecx,K` / `jmp K`), which need a tail call that portable C
  cannot express for arbitrary stack arguments.
