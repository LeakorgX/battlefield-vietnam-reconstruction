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
     `.bfvmod` section, fixes the PE headers and checksum;
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
  in the client and 159 in the server.

## Edit workflow

| Change | Steps |
| --- | --- |
| Reconstructed C | Edit `src/*.c` → `build.ps1` → `recomp.ps1` (regenerates the assembly with the new detours) |
| Original code | Edit `build/<target>-recomp/text.s` → `recomp.ps1 -AssembleOnly -Install` |

Rules for assembly edits:

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

## Next steps toward relocatable source

1. Symbolize absolute references: immediates and displacements that point
   into the image, jump tables, vtables, and pointers stored in data.
2. Emit data sections with labels instead of raw byte streams.
3. Once references are symbolic, allow sections to change size and relink.
4. Replace `.byte` holdouts and generate the PE headers and imports from
   source instead of copying them from the original image.

Whole-game milestones still need readable C for physics, gameplay, networking
and engine services. See [ROADMAP](ROADMAP.md).
