# Static recompilation

`bfv-native-code/tools/static_recomp.py` rebuilds the client and dedicated
server executables from source. Run it through `bfv-native-code/recomp.ps1`.

## Pipeline

1. `build.py` compiles `src/*.c` into `build/<target>/payload.bin` and writes
   `build/<target>/manifest.json`. The manifest lists the payload address,
   every C symbol, every entry detour (guard bytes and target symbol) and every
   vtable slot that points into C.
2. `static_recomp.py` checks the original EXE's SHA-256, then:
   - disassembles `.text` with Capstone and emits `build/<target>-recomp/text.s`
     (GNU as, Intel syntax);
   - emits every other section (`.rdata`, `.data`, `.rsrc`, ...) as `.byte`
     streams, with patched vtable slots written as `.long`;
   - assembles everything with MinGW `as`, places the C payload in a new
     `.bfvmod` section, fixes the PE headers and checksum (`--symbolic`
     assembles the headers from `headers.s` instead);
   - compares the result with the installed `*-editable.exe` and writes
     `recomp-report.json` (`byte_identity`, `differing_bytes`, `mode`).

## What the assembly looks like

```asm
sub_00401000:
  push ecx
  push esi
  {load} mov esi, ecx
  push 0
  lea ecx, [esp + 8]
  call sub_00639042
  mov eax, dword ptr [esi + 4]
  cmp eax, -1
  jae loc_0040101b
  ...
orig_bfv_server_read_word_004224e0:
  {disp32} jmp bfv_server_read_word_004224e0 /* detour to reconstructed C */
```

- **Labels.** `sub_` marks function starts from `reports/<target>/all-functions.tsv`
  plus the entry point, exports, TLS callbacks and catalogue entries. `orig_<name>`
  marks original functions replaced by C. `loc_` marks other branch targets.
  Targets that fall inside another instruction are defined with `.set`.
- **Exact encodings.** Each distinct instruction spelling is assembled once on
  its own. When GNU as would pick a different but equivalent encoding, a
  pseudo-prefix (`{load}`, `{store}`, `{disp8}`, `{disp32}`) selects the
  original one.
- **Raw bytes.** The few instructions GNU as cannot spell exactly (for example
  an imm32 form of a small immediate, or redundant prefixes) and undecodable
  bytes stay as `.byte`, with the disassembly in a comment. That is 617 lines
  in the client and 159 in the server. Symbolic mode spells more of them as
  text (see [Exact spellings](#exact-spellings-in-symbolic-mode)).

## Edit workflow

| Change | Steps |
| --- | --- |
| Reconstructed C | Edit `src/*.c` → `build.ps1` → `recomp.ps1` (regenerates the assembly with the new detours) |
| Original code, same size | Edit `build/<target>-recomp/text.s` → `recomp.ps1 -AssembleOnly -Install` |
| Original code, any size | `static_recomp.py --link-c --insert/--replace` (see [In-function edits](#in-function-edits---insert---replace)) |

Rules for assembly edits in the default mode:

- Every section must keep its size. Data, vtables, jump tables and many
  immediates are still absolute addresses, so moving code would break them.
  The tool rejects size changes with an explicit error. To add logic, write it
  in C and detour to it.
- `recomp.ps1` without `-AssembleOnly` regenerates `text.s` from the image and
  discards hand edits. Keep a copy of edited files.
- An edited build reports `byte_identity: false`. Expect differences only at
  the edited bytes and in the PE checksum.
- `build/` holds original machine code and must never be committed.

## Verified results

Checked against the supported hashes (see [SETUP](SETUP.md)):

| Target | Editable EXE SHA-256 | Instructions | Branches | Detours | `.byte` | Identity |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| Client | `5ed20932…9defbc87` | 2,240,682 | 359,407 | 4,691 | 986 | yes |
| Server | `94824c94…54f4b24a` | 1,275,735 | 194,095 | 3,466 | 448 | yes |

Edit-loop checks: an unchanged `-AssembleOnly` build was identical; a
same-length edit changed only that byte plus the checksum; a size-changing
edit was rejected.

The focused `verify_*.py` comparisons run under Unicorn 2.1.4, which does not
set the x87 exception flags (precision, underflow, overflow, divide, denormal,
invalid) in the status word: checked with `frndint` of 1.5, a float store of
2^24 + 1, a denormal and a signalling NaN, all leaving the status at 0. Their
"status apart from C0-C3" comparison therefore covers the stack top and
summary bits but not the sticky exception flags, and a reconstruction that
drops or adds a flag-raising instruction passes them. Where the C keeps such an
instruction (for example the local A* estimate's float store), it is kept
from the instruction evidence, not from a comparison.

## Symbolic mode (`--symbolic`)

`static_recomp.py --symbolic` writes absolute references as symbols and links
the sections with GNU ld (`ld -m i386pe`, a generated `link.ld` at the original
section addresses) into `build/<target>-symbolic/`. The address list comes
from `tools/symscan` (C++, capstone; build with `tools/symscan/build.ps1`),
which writes `build/symscan/<target>-refs.tsv` (every 4-byte candidate with a
decision and the rule behind it) and `-tables.tsv` (switch jump and byte index
tables inside `.text`, including the biased CRT `memcpy` tables).

- Switch tables become `jt_XXXXXXXX:` with `.long` labels, and index tables
  become `bt_XXXXXXXX:` byte data, instead of being decoded as code.
- Address operands become symbols, e.g. `jmp dword ptr [esi*4 + jt_0041bd28]`
  or `push offset d_00b42654`. Where the text cannot carry the symbol, the
  item is bytes with the field as `.long SYM`.
- Data sections define `d_XXXXXXXX` labels and store pointers as `.long SYM`.
- Values that symscan classifies as constants stay as numbers: string text,
  values inside instructions, flag bits, arithmetic. Each row in `refs.tsv`
  records its rule. Code addresses used as `and` selectors or `cmp` operands
  against a known code pointer are addresses (`select_code_pointer`,
  `compared_code_pointer`).
- PE directory contents are symbols too: import descriptors and thunks,
  export tables, TLS fields and callbacks, and resource data entries are
  written as `.rva SYM` / `.long SYM` (`pe_structure`).
- The headers are assembled from `build/<target>-<mode>/headers.s`
  (`header_source`), not copied from the original image. It has the DOS
  header with named fields, the DOS stub as 16-bit code, and the Rich header
  as decoded (product, build, count) entries. The XOR key is recomputed as
  the checksum and must match. Then come the file and optional headers one
  named field per line, the data directories, and the section table
  including `.bfvmod`. Layout fields come from the link: the section table,
  the data directories (from `d_` symbols at each directory's start and
  end), the entry point, SizeOfImage, SizeOfCode and SizeOfInitializedData.
  CheckSum is written after the file is complete.
- Import and export tables are written as what they are (`import_source`).
  Hint/name entries are `.short HINT` + `.asciz "Name"`, and DLL and export
  names are `.asciz`. The export ordinal list is `.short`, and by-ordinal
  thunks are `.long 0x80000000 | N`. Every thunk and descriptor field has a
  comment naming its DLL and function. Each IAT slot is
  `imp_<DLL>_<function>` (`imp_<DLL>_ord<N>` without a name), and code and
  data use it: `call dword ptr [imp_KERNEL32_GetModuleHandleA]`. Client:
  368 slots, 1,165 uses in `text.s`. Server: 159 slots, 520 uses.

| Target | Tables | Text references | Data words | Identity after the link | Shifted relink |
| --- | ---: | ---: | ---: | --- | --- |
| Client | 373 | 159,927 | 177,948 | yes | 84,221 words moved, 0 unexplained |
| Server | 148 | 89,744 | 120,717 | yes | 47,099 words moved, 0 unexplained |

In the shifted relink, `.data` and every later section are linked 0x10000
higher. Every changed word moved by exactly that amount. (The shifted-relink
counts are from before the `import_name_text` rule, which took 15 client and
5 server import-name words out of the data words. Identity was rerun after
it.)

## Link mode and the unified link (`--link-c`)

`tools/linkmode.py` (C++ rewriter `tools/linkmode/linkmode.cpp`; build with
`tools/linkmode/build.ps1`) rewrites the reconstructed C so that every guest
address is a symbol `a_XXXXXXXX` (`BFV_A`/`BFV_AI`) instead of a number.
Client: 59,928 address symbols across 1,361 files. Server: 28,913 across 884.

- GCC compiles symbols differently from literals (folding, scheduling), so
  the link-mode payload is its own compile, not byte-identical to the
  default one. Guest memory accesses are volatile, so GCC's assumption that
  distinct symbols do not alias changes nothing.
- The payload is gated by a shift relink instead: it is linked twice, the
  second time with every `a_` symbol 0x135791 higher, and
  `build/linkmode/relocheck.exe` (C++) proves that every differing byte is
  a 32-bit field that moved by exactly that shift.
- The hand-written modules and the mods are covered too
  (`linkmode.exe --src`). Every `build/<t>/target.h` name that is an image
  address becomes a symbol (187 per target). Every literal in the generated
  catalog modules (`src/static_initializers.c`, `constructor_calls.c`,
  `dynamic_casts.c` and five more) is decided against the native function
  the C replaces, using the catalog's native bytes: it is a symbol when it is
  a rel32 call/jmp/jcc target there or an immediate that symscan decided is
  an address. Client 10,096 and server 7,322 literals are symbols. One per
  target stays a number: `0x00400000`, the image base, returned as a value
  (symscan: constant). In inline-asm `"i"(...)` operands the bare `a_` array
  symbol is used, because GCC does not accept every integer cast of an
  address as an immediate there. In the mods, the hook stubs' function
  addresses and the `OutputDebugStringA` import slot are symbols as well.
- With those, the whole payload passes the shift relink: client 67,801
  address symbols and 303,112 moved fields, server 34,843 and 168,662, with
  0 unexplained bytes.

`static_recomp.py --link-c` links the image sections and that payload in one
ld run (`build/<target>-linked-c/`). The `a_` symbols and the detour equates
(`.set name, "_bfv_x@16"`) are defined in the `.s` files, not in the linker
script, because large script assignments make ld quadratic. The gate
(`link_c_gate`, `relocheck --map`) compares every image section with the
default build: each difference must be a field (absolute or rel32) that
pointed at a C symbol's old address and now points at its new one.

| Target | Payload vs. separate link | Moved detour fields | Unexplained |
| --- | --- | ---: | ---: |
| Client | identical | 38,497 | 0 |
| Server | identical | 23,921 | 0 |

## Size changes (`--grow-text N`)

In symbolic and link-c builds each section is placed at max(original RVA,
aligned end of the previous section) (`sequential_layout`), so a section can
grow and push the later ones up. In link-c mode the payload section
`.bfvmod` moves up too. `--grow-text N` appends N int3 bytes to `.text` as a
test of this. `grow_gate` checks it against the same build without the
growth (which must exist):

- `.text` ends with exactly N int3 bytes, and its virtual size covers them;
- in every section, each differing byte is a field (VA, RVA or rel32) whose
  target lies in a section that moved, and it moved by that section's
  delta (`relocheck --ranges`);
- the import, export and resource directories parse the same.

`--link-c --grow-text 0x3000` moves `.rdata`, `.data`, `.tls`, `.rsrc`,
`.ctfsnd` and `.bfvmod` by 0x3000. Both targets pass:

| Target | `.text` | `.rdata` | `.data` | `.rsrc` | `.bfvmod` | Unexplained |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Client | 181,731 | 57,387 | 8,030 | 2 | 533,079 | 0 |
| Server | 103,574 | 38,871 | 5,131 | 2 | 297,690 | 0 |

Client `.tls` (1 field) and `.ctfsnd` (10) also pass.

`--grow-at VA` puts the N bytes before the `text.s` label of VA instead, so
the code after it moves inside `.text` as well. The gate then compares
`.text` in two pieces (head unchanged, tail moved by N), takes per-piece
deltas for the field check, and also checks that the entry point and the
export addresses moved with their code. `--link-c --grow-text 0x3000
--grow-at 00401030` (nearly all code moves) passes for both targets:

| Target | `.text` head | `.text` tail | `.rdata` | `.data` | `.rsrc` | `.bfvmod` | Unexplained |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Client | 2 | 154,134 | 165,830 | 12,110 | 2 | 697,486 | 0 |
| Server | 2 | 84,917 | 113,261 | 7,459 | 2 | 389,281 | 0 |

Client `.tls` (1) and `.ctfsnd` (10) pass too. The entry point and exports
moved with their code, and the import, export and resource directories
parse the same. The test was rerun after the symbolic short detours and the
exact `.text` sizing (below) and gave the same counts.

What this test needed:

- Branch targets inside an instruction (a jump into the middle of another
  instruction) are defined from the nearest preceding label in `text.s`
  rather than from `__text_base`, so they move with their code. The value in
  an unmodified build is the same.
- symscan no longer reads import name text as addresses: words inside import
  DLL names and hint/name entries are constants (`import_name_text`; client
  85 rows, 15 of them were addresses before; server 40 and 5).
- The hand-written modules and mods had to be in link mode (above). Their
  numeric addresses did not show up in the plain shift relink, because the
  gate only checks bytes that differ: a field that should move but stays a
  number is invisible there. Moving the code under it is what exposes it.

## Exact spellings in symbolic mode

Symbolic builds spell more instructions as text than the default mode:

- **Wide immediates.** An imm16/imm32 form of a small value (`66 3d ff ff`,
  `cmp ax, 0xffff`) is written as `offset bfv_wide_ffff`, with
  `.set bfv_wide_ffff, 0xffff` at the end of the file. GAS has to keep the
  full-width field for a symbol that is defined later, and the field has no
  relocation.
- **Segment prefixes.** A redundant segment override is written as its
  mnemonic (`ds mov eax, ...`), which GAS emits as given.
- **Branches out of `.text`.** A near `jmp`/`call` into another section (the
  client's `.ctfsnd`) branches to that section's `d_` symbol.
- **Short detours** are written as `.byte 0xeb` followed by
  `.byte sym - . - 1`, so they follow their target when code moves.

`build/<target>-symbolic/holdouts.tsv` lists what is still written as bytes:

| Target | Byte-encoded spellings | Of them L1 instructions |
| --- | ---: | ---: |
| Client | 2,767 (was 3,181) | 0 (was 155) |
| Server | 1,205 (was 1,345) | 0 (was 26) |

None of the remaining ones is an instruction that the L1 lifter reaches
(`l1-insn` addresses from the lifted C). They are bytes inside functions that
no path decodes (embedded data, padding), bytes after a detour's guard, and
data between functions. Both targets stay byte-identical with these
spellings.

## In-function edits (`--insert`, `--replace`)

```
static_recomp.py --target client --link-c \
    --insert "004016ee:.rept 65|push eax|pop eax|.endr" \
    --replace "004016fa:or eax, 4|nop"
```

`--insert VA:ASM` puts the assembly (Intel syntax, `|` between lines) before
the instruction at VA, and branches to VA reach the inserted code.
`--replace VA:ASM` replaces that instruction, and an empty ASM removes it.
The build goes to `build/<target>-<mode>-edited/` and is compared with the
same build without edits, which must exist.

The `.text` gate is `tools/textcheck` (C++, capstone; build with
`tools/textcheck/build.ps1`). `static_recomp.py` writes `items.tsv`, which
lists every item of the original `.text` (code, held bytes, data, jump
tables) and each edit. textcheck pairs every original item with its place
in the edited `.text` and checks:

- **Code.** It is the same instruction. A relative branch keeps its mnemonic
  and its target moved with that code; it may grow from short to near. Every
  32-bit displacement or immediate is the same or moved with its target.
- **Data, held bytes and jump tables.** The bytes are the same except for
  32-bit fields that moved with their target, and jump-table entries moved
  with their code.
- **Edits.** Each edit is printed disassembled.
- **Pieces.** The moved pieces of `.text` then drive `relocheck --ranges`
  over every other section, as in the growth test, followed by the
  entry-point, export and directory checks.

Held bytes keep their bytes. Some of them decode as a relative branch whose
target no longer lines up after the move. These are counted, not failed,
because no L1 path reaches them. Dead bytes after a detour that decode
across a symbol field pass when they are the same bytes with moved fields.

`.text` is assembled with `--no-pad-sections`, and its virtual size grows by
exactly the edit's bytes.

Results:

| Target | Edits | Moved branches | Moved `.text` fields | `.rdata` | `.data` | `.bfvmod` | Mismatched / unexplained |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Client | insert 130 bytes at `004016ee` (its `je` grows to rel32), replace `004016fa` with 4 bytes | 390,900 | 10,864 | 108,449 | 4,080 | 164,451 | 0 |
| Server | insert 130 bytes at `00401b1e`, remove `00401b2a` | 213,591 | 5,227 | 74,389 | 2,328 | 88,857 | 0 |

Both runs are `--link-c`. The entry point and exports moved with their
code, and the import, export and resource directories parse the same. Held
relative branches counted: client 293, server 184. Code items matched as
bytes: client 7, server 4.

In plain `--symbolic` mode the separate C payload keeps numeric addresses,
so C calls into moved original code would be stale. Use `--link-c` for
edits that move code.

## Next steps toward relocatable source

Edits inside functions are gated. The open items are those in
[ROADMAP](ROADMAP.md): readable C for the remaining subsystems.

Whole-game milestones still need readable C for physics, gameplay, networking
and engine services. See [ROADMAP](ROADMAP.md).
