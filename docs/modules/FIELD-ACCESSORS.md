# Field and global accessors

889 complete native functions have individual editable C implementations:
576 client and 313 server entries in `bfv-native-code/src/field_accessors.c`,
catalogued in `bfv-native-code/field-accessors.json`. Like the
[word getters](WORD-GETTERS.md), these are small complete bodies, not gameplay
algorithms. Owning classes and field meanings remain unresolved.

| Kind | Native body | C form | Client | Server |
| --- | --- | --- | ---: | ---: |
| `global_word` | `mov eax,[abs32]` / `ret` | `uint32_t f(void)` | 303 | 83 |
| `word_setter` | `mov eax,[esp+4]` / `mov [ecx+d32],eax` / `ret 4` | `void TC f(void *, uint32_t)` | 99 | 86 |
| `float_getter` | `fld dword [ecx+d32]` / `ret` | `float TC f(const void *)` | 91 | 77 |
| `field_address` | `lea eax,[ecx+d32]` / `ret` | `void *TC f(void *)` | 68 | 54 |
| `double_getter` | `fld qword [ecx+d32]` / `ret` | `double TC f(const void *)` | 10 | 10 |
| `byte_set_one` | `mov byte [ecx+d32],1` / `ret` | `void TC f(void *)` | 5 | 3 |

Accesses go through packed `volatile` structs, keeping the single native
(possibly unaligned) access. Setters return the stored value in EAX exactly like
the native body.

## Evidence

`analysis/tools/recover_field_accessors.py` generates the source and catalog from
the original EXEs (supported hashes only). It accepts a function only when:

- the Ghidra-indexed body is exactly one of the shapes above, with a 32-bit
  displacement of at least 128 for object fields;
- the entry is 16-byte aligned and not already detoured;
- no branch target labelled by the static recompiler, and no absolute pointer at
  any byte offset anywhere in the image, lands in the five bytes the detour
  overwrites. Computed runtime code addresses are not excluded by this check.

Rejected: client 4 unaligned and 2 interior-pointer cases, server 3 unaligned.

`bfv-native-code/tools/verify_field_accessors.py` (part of `verify.py`) requires
the compiled C body to be **byte-identical** to the native body, then runs
original and reconstructed entries side by side across eight stored bit
patterns, four object alignments, six CPU-flag/x87-stack scenarios: 48 cases per
entry. It compares EAX, all preserved registers, flags, stack cleanup, full x87
state and the exact data reads and writes with values.

## Not converted (yet)

These common shapes compile to different bytes with GCC, so they stay native
until a translation with an exact register contract is proven:

- byte getters `mov al,[ecx+d32]` (GCC zero-extends into EAX);
- byte setters `mov al,[esp+4]` / `mov [ecx+d32],al` (GCC loads all of EAX);
- `inc`/`dec dword [abs32]` and field increments (GCC uses `add`/`sub`, which
  set the carry flag differently);
- 4-byte bodies with 8-bit displacements (too small for a 5-byte detour).
