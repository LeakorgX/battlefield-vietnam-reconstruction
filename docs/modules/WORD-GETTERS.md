# Object-word getters

175 complete native functions now have individual editable C implementations:
96 client entries and 79 server entries in `bfv-native-code/src/word_getters.c`.
Each reads one 32-bit word at a documented byte offset from its ECX object pointer
and returns the word in EAX. These are complete small bodies, not recovered
gameplay algorithms. Owning classes and semantic field types remain unresolved.

The source uses the native `thiscall` convention and a packed, volatile field
load. This retains unaligned access and preserves the native flags and register
contract. Returned words may represent integers, pointers or float bits; the
source does not guess a semantic type. An invalid receiver is not repaired or
silently replaced with a default value.

The build compiles these C files and installs guarded entry jumps into copies of
the original EXEs. It does not regenerate the source, so contributor edits persist.
The original object storage and most engine behavior remain native dependencies.

## Structural evidence

`analysis/tools/ghidra/AuditWordGetters.java` checks the supported original binary
hash, aligned entry, exact contiguous seven-byte body, six-byte MOV followed by
RET, field displacement, and recorded incoming references. No recorded reference
may target an interior byte of the replaced MOV. The generator independently
checks all seven native bytes and publishes the audit tables and entry catalog.
Static references do not exclude every computed runtime code address.

Four-byte getters are excluded: safely replacing them would require additional
proof about padding, neighboring instructions and shared return tails.

## Verification

```powershell
uv run --with pefile --with unicorn python bfv-native-code/tools/verify_word_getters.py --game-dir $env:BFV_GAME_DIR --target both
```

Every original and compiled entry runs with eight field bit patterns and six
flag/x87 scenarios: 4,608 client comparisons and 3,792 server comparisons.
The fixtures cover all four receiver alignments, returned EAX bits, stack cleanup,
all non-result integer registers, CPU flags and complete retained x87 state.
They require exactly one four-byte field read, one return-address read, no memory
writes and no delegation outside the appended source payload.
The compiled seven-byte getter bodies also match the original MOV/RET bytes
exactly; they were emitted by the C compiler from the editable source.

The default verification suite includes these checks. Published reports name the
specific executable hashes tested. These controlled objects do not establish
owning-class lifetimes, behavior under concurrent field writes, invalid-pointer
fault equivalence or full-game parity.

To reproduce source recovery, supply the two audited TSV files to
`analysis/tools/recover_word_getters.py --game-dir ... --audit-dir ...`.
The optional `--overwrite` replaces existing contributor edits intentionally.
