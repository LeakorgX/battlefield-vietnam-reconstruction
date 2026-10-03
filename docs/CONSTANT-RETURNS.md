# Individually editable constant-return functions

`bfv-native-code/src/constant_returns.c` reconstructs 954 complete short functions:
541 from the client and 413 from the dedicated server. Each native body returns
one fixed 32-bit word in EAX, then returns with its original stack cleanup.
Every entry has its own C function, so a contributor can edit one without changing
all other entries that happen to return the same value.

Search the original address or observed identifier in the C file. For example,
each body contains an ordinary editable constant:

```c
static const volatile uint32_t result = 0x00000024u;
return result;
```

The `volatile` load preserves CPU flags. A normal C `return 0` can become XOR EAX,
which changes flags even though the returned word is correct. The generated
functions contain no delegated native calls. Some returned values point into the
original executable's data; those data dependencies are explicitly retained.
This batch does not reconstruct that data or the surrounding AI/physics logic.

The names identify native entry addresses, not guessed original C++ method names.
`uint32_t` describes returned EAX bits. Semantic pointer/integer/enum types and
names of ignored inputs remain unknown. Placeholder stack parameters preserve
RET cleanup; they are not a recovered high-level API. Observed identifier comments
describe printable original data, not inferred ownership or class declarations.

## Evidence and exclusions

The discovery pass accepts only the complete `MOV EAX, immediate; RET` shape.
`AuditConstantReturns.java`, run read-only on hash-matched Ghidra projects, then
checks exact contiguous function boundaries, instruction lengths, stack cleanup,
16-byte entry alignment, and recorded references into the five overwritten bytes.
Unaligned or otherwise unsupported candidates remain excluded. Audit results,
including exclusions, are in `reports/*/constant-return-audit.tsv`.

The builder reads `bfv-native-code/constant-returns.json`, verifies the original
binary hash and every five-byte prefix, and installs a jump to each selected C
function. It compiles only the target's functions. Original RET bytes remain
after each entry jump; no native code is copied into these C bodies.

`verify_constant_returns.py` executes every actual original/replacement entry in
six scenarios. It checks return bits, stack cleanup, preserved integer registers,
CPU flags, complete x87 state and no memory writes. Source functions may only
read source-owned constant storage and their return address. Two scenarios retain
two caller x87 values. Immutable emulators are reused with explicit trampoline
translation-cache invalidation; no numeric or engine services are mocked.

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_constant_returns.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

The client passes 3,246 cases and server passes 2,478. The default full verifier
includes these checks. Static-reference auditing cannot exclude every computed
runtime reference or code-inspection path; complete engine behavior and original
data independence remain unverified.

## Regeneration

The build never regenerates the C file: hand edits remain intact. To create a
fresh baseline, prepare an input TSV with address, constant, stack_cleanup and
indexed_bytes columns, run `AuditConstantReturns.java` against each supported
project with `-readOnly`, then invoke `analysis/tools/recover_constant_returns.py`
with `--game-dir` and `--audit-dir`. The audit directory must contain
`client-audit.tsv` and `server-audit.tsv`.

The generator refuses to replace an existing C file unless explicitly given
`--overwrite`. Review edits before using that option. Intentional return-value
changes need their own expected-behavior checks; original-equivalence checks will
correctly fail for deliberate modifications.
