# Virtual clone functions

357 complete native functions have individual editable C implementations:
199 client and 158 server entries in `bfv-native-code/src/clones.c`,
catalogued in `bfv-native-code/clones.json`.

These are small virtual `clone()` methods (`return new T(*this)`) for 12- to
24-byte classes. They allocate the object, copy one data word, store the class
vtable and return the copy (or null if the allocation failed):

```c
/* Native 0052d0b0; 12-byte object, vtable 00b6326c. */
void *TC bfv_client_clone_0052d0b0(const void *self)
{
    void *copy = BFV_ALLOCATE(12u);
    if (!copy)
        return 0;
    BFV_WORD(copy, 8u) = BFV_WORD(self, 8u);
    BFV_WORD(copy, 0u) = 0x00b6326cu;
    return copy;
}
```

Every accepted body allocates through one routine (client `00403610`, server
`00403d80`). That it is `operator new` is likely but not proven, so it is named
`BFV_ALLOCATE`. Vtables and class identities stay native.

## Evidence

`analysis/tools/recover_clones.py` generates the source and catalog from the
original EXEs (supported hashes only). It accepts a function only when:

- the indexed bytes match the allocate/copy/vtable/null-path shape exactly and
  the null branch lands on the shared `xor eax,eax` tail;
- the allocation goes to the shared routine;
- copied fields are aligned, inside the allocated size and not the vtable slot;
- the entry passes the same detour audit as the
  [field accessors](FIELD-ACCESSORS.md).

No candidates were rejected.

The compiled body is not byte-identical. `bfv-native-code/tools/verify_clones.py`
(part of `verify.py`) checks ABI equivalence. It stubs the allocator to return
either a fresh buffer or null and runs original and reconstructed entries side
by side over six scenarios with four object alignments and varied CPU flags and
x87 stack depths. It requires:

- the same ordered events: allocation with the catalogued size, field read,
  copy write with the same value, vtable write;
- EAX = the new buffer (or 0), a balanced stack, and EBX/ESI/EDI/EBP restored on
  return (the native body holds `this` in ESI across the allocation);
- unchanged x87 state and no other writes outside the function's own frame.

ECX, EDX and flags are scratch and are not compared. Current result: client
1,194 and server 948 comparisons.

## Not converted (yet)

Clones that copy several fields, call a copy constructor or allocate larger
objects.
