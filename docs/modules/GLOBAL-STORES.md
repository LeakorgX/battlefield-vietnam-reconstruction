# Constant global stores

171 complete native functions have individual editable C implementations:
164 client and 7 server entries in `bfv-native-code/src/global_stores.c`,
catalogued in `bfv-native-code/global-stores.json`.

Each function only stores constants into globals and returns:

```
(mov dword [global], imm32 | mov byte [global], imm8)+
ret
```

Almost all are C++ static-object initializers whose constructor was inlined
down to a vtable, self-pointer or flag store. Some reset small global records,
for example three `1.0f` words.

| Stores per function | Client | Server |
| --- | ---: | ---: |
| 1 | 145 | 7 |
| 2 | 1 | 0 |
| 3 | 8 | 0 |
| 4 | 10 | 0 |

```c
/* Native 00abddf0; globals 00e21210, 00e21214. */
void bfv_client_global_store_00abddf0(void)
{
    ((volatile struct BfvWord *)0x00e21210u)->value = 0x00000000u;
    ((volatile struct BfvWord *)0x00e21214u)->value = 0x00000000u;
}
```

Packed volatile accesses keep the native order and width. The object types and
the meaning of each global remain native and unnamed.

## Evidence

`analysis/tools/recover_global_stores.py` generates the source and catalog from
the original EXEs (supported hashes only). It accepts a function only when the
indexed bytes match the pattern exactly and the entry passes the same detour
audit as the [field accessors](FIELD-ACCESSORS.md). No candidates are
currently rejected.

`bfv-native-code/tools/verify_global_stores.py` (part of `verify.py`) checks
behavior, not bytes. Before each run it poisons the store targets so that a
skipped store is visible. It then runs original and reconstructed entries side
by side under four combinations of CPU flags, scratch registers and retained x87
stack depth (0 to 2). It requires:

- exactly the catalogued stores (address, width, value) in native order;
- no reads outside the function's own frame;
- the same final global contents;
- a balanced stack (`ret`), EBX/ESI/EDI/EBP preserved, and unchanged x87 state.

The functions return void, so EAX, ECX, EDX and flags are scratch. Current
result: client 656 and server 28 comparisons.
