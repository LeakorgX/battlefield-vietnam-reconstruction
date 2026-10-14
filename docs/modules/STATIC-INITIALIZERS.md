# C++ static-object initializers

3,947 complete native functions have individual editable C implementations:
2,258 client and 1,689 server entries in `bfv-native-code/src/static_initializers.c`,
catalogued in `bfv-native-code/static-initializers.json`.

MSVC emits one of these for every global object that has a constructor and a
destructor (`static Foo g_foo;`). The CRT calls each one from its initializer
table at startup:

```
mov ecx,object / call constructor / push destructor_thunk / call _atexit / pop ecx / ret
```

The C form makes the same two calls:

```c
void bfv_client_static_init_00ac9550(void)
{
    ((BfvConstructor)0x004044e0u)((void *)0x00d43d98u);
    BFV_ATEXIT((void (*)(void))0x00b27cc0u);
}
```

Constructors, destructors and object types stay native and unnamed. Naming the
globals and their classes comes later.

## Evidence

`analysis/tools/recover_static_initializers.py` generates the source and catalog
from the original EXEs (supported hashes only). It accepts a function only when:

- the Ghidra-indexed body is exactly the 22-byte shape above;
- the second call goes to the CRT `_atexit` (client `007c2cd2`, server `0063b363`);
- the entry passes the same detour audit as the
  [field accessors](FIELD-ACCESSORS.md): 16-byte aligned, not already detoured,
  and no recompiler label or absolute image pointer in the five overwritten bytes.

Rejected: client 5 interior-pointer and 3 unaligned cases, server 2 unaligned.

The compiled body is **not** byte-identical. GCC keeps a 16-byte-aligned frame
and calls through EAX. `bfv-native-code/tools/verify_static_initializers.py`
(part of `verify.py`) therefore checks ABI equivalence instead. It stubs the
constructor and `_atexit` with recording hooks and runs original and
reconstructed entries side by side across four CPU-flag/x87-stack/scratch
scenarios. It requires:

- the same calls in the same order: constructor with `ECX` = object, then
  `_atexit` with the destructor thunk;
- the `_atexit` result returned in EAX;
- a balanced stack, EBX/ESI/EDI/EBP preserved both at each call and on return,
  and unchanged x87 state;
- no memory writes outside the function's own stack frame;
- execution that stays inside the native body or the `.bfvmod` payload.

ECX, EDX, flags and EAX before each call are scratch and are not compared.

## Initializers with constructor arguments

460 more initializers pass immediate arguments to the constructor: 253 client
and 207 server entries in `bfv-native-code/src/constructor_calls.c`, catalogued
in `bfv-native-code/constructor-calls.json` by
`analysis/tools/recover_constructor_calls.py`. Of these, 124 client and 121
server entries also register a destructor; the rest construct only.

```
push imm ... / mov ecx,object / call constructor / [push thunk / call _atexit / pop ecx] / ret
```

```c
void bfv_client_construct_00b243e0(void)
{
    ((void (TC *)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))0x00413700u)
        ((void *)0x00e0eeb0u, 0x00bf4b60u, 0x00000028u, 0x00000a00u, 0x00000000u, 0x00000001u);
    BFV_ATEXIT((void (*)(void))0x00b40180u);
}
```

C arguments are the pushes in reverse order, and `push imm8` is sign-extended.
The same verifier runs them (`--catalog constructor-calls.json`). Its
constructor stub reads the stack arguments and pops them like the thiscall
callee. It also requires EAX to be the last callee's result. Rejected: 1
unaligned client entry.
