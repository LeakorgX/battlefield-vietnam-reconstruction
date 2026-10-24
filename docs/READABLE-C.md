# L2 readable C

`bfv-native-code/readable/<target>/` is the same set of functions as the L1
lifted C ([LIFTED-C.md](LIFTED-C.md)), in the same folders and files, with the
same names, notes and symbols, written as compact C that can be read:

```c
/* 00500b70  BitStream::readBits */
/* thiscall: 2 stack arguments, reads ecx, pops 8 */
void bfv_client_l1_body_00500b70(struct bfv_cpu *c)
{
    L2_BEGIN;
    uint32_t p1, this_endBit, v6, this_data, ...;
#define this ((volatile struct BitStream *)(uintptr_t)ecx)
#define result eax
    uint32_t frame = esp;  /* where the body starts: the return address */
#define arg1 M32(frame + 0x8)
#define local_4 M32(frame - 0x4)
    ...
    p1 = arg1;
    local_4 = (uint32_t)this;
    ...
    this_endBit = this->endBit;
    v6 = result_2 + p1;
    if (v6 > this_endBit) {
        RESTORE(edi);
        this->failed = 1;
        this->error = 2;  /* 1 write overflow, 2 read overflow */
        ...
        RET(8);
    }
```

L1 spells out every flag of every instruction. L2 is about 45% of its size
(client 141 MB vs 312 MB, server 79 MB vs 175 MB).

The registers are still the machine's, but every value has a name: plain
variables where nothing outside the function sees it, `#define` names of its
register where something does (`this` is `ecx`, `result` is `eax`), named
stack slots (`arg1`, `local_4`), addresses (`g_pPlayerManager`,
`typeid_BFSoldier`, `__RTDynamicCast`) and known struct fields. The guest
stack stays explicit. A call is an ordinary statement: the callee returns
into the C body (see "Calls return into the body"). A direct call with a
known ABI is written as an expression, `r = THISCALL(k, Foo, obj, a, b)`
(see "Typed calls"), and known fields of `this` as `this->field`.

## What `tools/lift_l2.py` changes

The generator lifts each function again from the original instructions. It
asserts that the extent, stack gap and resume points match `lifted/<target>/catalog.json`,
so the two tiers can be swapped function for function.

- **Flag liveness.** A backward dataflow over the function's control flow
  decides which flags are still read. Flags are dead after `ret` and at calls,
  where the callee sets them. They are live wherever the body leaves to native
  code. Updates that nothing reads are dropped.
- **Fused conditions.** A `cmp`/`test`/logic/arithmetic instruction becomes a
  single C condition together with the `jcc`/`setcc`/`cmov` instructions that
  read it, for example `if ((int32_t)eax < 5) goto L_...;`. Instructions that
  don't touch the flags may sit in between. If one of them overwrites an operand,
  the operand is copied into `lhs`/`rhs` first. Memory operands are always
  copied once anything sits in between. This happens only when every flag the
  producer sets is dead afterwards; otherwise the full L1 flag code stays
  (client 9,982 places, server 5,138).
- **Plain spelling.** Addresses are inline and operators are plain C. Stack
  and call sequences use the macros in `readable/runtime/l2.h`:
  `PUSH`, `POP`, `CALL(k, target)`, `RET(n)` and `EXIT_TO`. x87/SSE
  instructions stay native as `FPU("...")` / `FPU_MEM("...", address)`.

## Structured control flow (`tools/structure_l2.py`)

After lifting, each body is nested into `if`/`else`, `do`/`while`, `while`
and `for (;;)` with `break`:

- `if (c) goto L; A; L:` becomes `if (!c) { A }`. With `A` ending in
  `goto M; L: B; M:` it becomes `if (!c) { A } else { B }`.
- A label with a later branch back to it becomes a loop, and an exit test
  to just after the loop becomes `break`.
- Inside a loop, a jump to the loop's exit is `break` and a jump to the head
  of a `while`/`for` loop is `continue`. An `else` is found whenever the
  `goto M` at the end of `A` lands inside the enclosing block.
- A jump to a short tail (at most 6 plain statements ending in `RET`/`EXIT_TO`,
  with no call, label, branch or asm) gets a copy of the tail:
  `if (c) goto L;` becomes `if (c) { RESTORE(edi, esi); esp += 0x1c; RET(12); }`.
  The label goes when nothing else jumps to it.
- Jumps that fit none of these stay `goto`. C allows a goto into a block, so
  a label that is still referenced (and the `R_k` restart labels that
  `switch (c->eip)` jumps to, see below) keeps working inside the new blocks.
- Every rewrite is only nesting, so the program is the same.

Each function also gets a one-line calling-convention comment from its
instructions: `ret n`, and `ecx`/`edx` read before written.

`readable/flat-functions.json` lists functions kept goto-only (none now). `--flat`
writes the old goto form for everything.

## Flags across call/ret

`flag_abi()` scans the whole image for code that reads flags it didn't set:
callees that branch on incoming flags (`acos`/`asin` in the MSVC CRT), and
call sites that read the flags a callee returns (`_fload`). For those, flags
stay live at the call or the `ret`. The verifier checks each function alone,
so it can't catch this. The live server crashed without it.

## Register variables (L2b, `tools/regvars.py`)

A register value that no call, `ret` or exit to native code can see gets its
own variable, `v1`, `v2`, ... (declared after `L2_BEGIN`). Values the outside
sees keep the register name:

- at entry every register is the caller's;
- at a call: the callee's input registers and the callee-saved registers
  flowing through it. After the call every register is reloaded from what
  the callee left (`L2_RELOAD`), so a value live across a call stays a
  register name;
- at `ret`: `eax`, the callee-saved registers, and the `ecx`/`edx` that some
  caller reads after the call;
- wherever the body leaves to native code (`EXIT_TO`): every register.

The callee inputs and caller reads come from a whole-image pass
(`tools/reg_abi.py`). It tracks registers in pieces (low/high byte, upper
half) so that `mov dl, [eax]` doesn't read `edx`. A call through a pointer is
assumed to read `eax`/`ecx`/`edx`. Webs are built from reaching definitions;
an instruction that reads and writes a register, a partial write, or an asm
`"+r"` operand keeps one web.

Client 187,281 variables in 24,142 of 33,731 functions; server 104,613 in
15,150 of 20,473. Every other register value is named too (see Names). About a quarter of the register mentions are replaced. The
rest cross calls. Calls now return into the body (see below), and calls with
a known ABI are typed expressions (see "Typed calls"). Keeping values in C
variables across those calls is the next step.

The verifier compares at the ABI level for these functions:
the catalog's `abi` entry lists the registers each call and the `ret` can
see, and the others are left out of call events and final registers. After a
stub returns, the `ecx`/`edx` the caller doesn't read are poisoned on both
machines, so a wrong guess shows up as a mismatch. A `ret` to a random
address is classified on the native run and applied to both.

Risks the verifier can't see, as with the flag ABI: a callee reached through
a pointer that reads a register other than `eax`/`ecx`/`edx`, and caller reads
of `ecx`/`edx` found past the 400-instruction limit of `gives_after`.
`readable/fixed-registers.json` lists functions kept with register names
(client 28, server 18: they passed at L2 and failed with variables).

## Names (`regvars.py`, `symbols_l2.py`, `slots_l2.py`)

Every name is textual: a variable, or a `#define` with exactly the text it
replaces, so the compiled code is the same as with the numbers (checked by
compiling both trees and comparing `objdump -dr`; stack slots below are the
one exception and are verified like everything else).

- **Values.** Each web (above) is named from where it comes from: `this` at
  entry in `ecx`, `result` for what the function returns, `p1` for the
  second stack argument, `r_getLocalPlayer` for a call's result, `vtbl` for
  a loaded vtable, `this_endBit` for a field load through `this`
  (`this_4c` when the field has no name), `playerManager` for a load of
  `g_pPlayerManager`, and `v1`, `v2`, ... otherwise. A callee-saved register
  keeps its name across a call. Names that cross a call are
  `#define name reg` for that function; the rest are variables.
- **Addresses.** Every image address with a note becomes a name in
  `readable/runtime/symbols_<target>.h` (client 18,977, server 11,192):
  `imp_CreateFileA` for imports, `vtbl_X` and `typeid_X` for RTTI, function
  names with `::` as `_`, `g_` globals. A name used twice for different
  addresses gets the address as suffix. `CALL(1, 0x8191a0)` is
  `CALL(1, __RTDynamicCast)`.
- **Stack slots.** `arg_N`/`var_N` notes become `argK` (`M32(frame + 4 +
  N)`) and `local_N` (`M32(frame - N)`); `ADDR(slot)` is the address.
  `frame = esp` is set at body entry, so a slot through ESP is named only at
  instructions that no path from a call or resume reaches (after a call,
  the callee's pop count through a pointer is a guess). Slots through an EBP
  frame are named anywhere, as `M32(<ebp name> - N)`. Client 57,255 uses,
  server 32,812.
- **Fields.** A `field X` or `field Class.member` note on a `[reg + off]`
  access becomes `M32(this + X)` or `M32(this + Class_member)` with
  `#define X off` for that function (client 1,095, server 665). Names that
  clash with anything else in the function stay numbers with the note.

## Calls with arguments (`tools/calls_l2.py`)

The pushes in front of a call become its argument list, first argument first,
and pushes and pops of the caller's ebx/esi/edi/ebp become `SAVE`/`RESTORE`:

```c
    SAVE(esi);                                  /* was PUSH(esi); */
    v3 = ADDR(local_4);
    CALL_ARGS(1, std_string_assign_chars, 0xb441d0, 0x1b);  /* f pops them */
    CALL_CDECL(2, operator_delete, v3);         /* + esp += 4 after the call */
    RESTORE(esi);
```

The macros in `l2.h` expand to the same pushes in the same order. The number
of arguments comes from `esp += n` right after the call, the callee's `ret n`,
or the stack arguments the callee reads (its `arg_N` notes); for calls through
pointers, the pushes right in front up to a save. Statements between the
pushes move in front of them only when they touch no memory and nothing pushed
earlier reads what they write; `esp` in them is rewritten for the higher
`esp`. Memory accesses keep their order against the pushes: the verifier
compares the order of all memory accesses, pushes included, and a first try
that also moved globals, slots and pointer accesses failed 221 more functions.
Client 93,652 calls (26,352 cdecl) and server 51,917 (14,657) have their
arguments; raw `PUSH` lines went from 324,238 to 108,695 (client) and
174,236 to 52,845 (server). What's left: calls with no stack arguments,
pushes split from their call by a memory access or a branch, and CRT
functions whose arguments are read through an EBP frame (their notes carry
no `arg_N`).

## Typed calls (`calls_l2.py`, `l2.h`)

When the callee is a direct target whose ABI is known from instruction
evidence (its pop count from `ret n`, its register inputs from
`tools/reg_abi.py`), the call is an expression whose value is `eax`:

```c
    result = THISCALL(3, BitStream_writeBits, v8, v12, 0x80);
    r_isPressed = THISCALL(2, PlayerInput_isPressed, v9, 0xb);
    r__ftol2 = STDCALL(25, _ftol2);
    r_string_format = CDECL(3, string_format, v28, 0xbf3ac4, this_54_3, 0xbf3acc);
```

- `STDCALL(k, f, a, b)`: f reads no register input (ebx/esi/edi/ebp pass
  through unchanged) and pops its own arguments, or has none.
- `THISCALL(k, f, obj, a, b)`: the same with `ecx = obj` first. `obj` is the
  `ecx` value set right in front (`v20 = esp + 0x108;` is folded in when
  nothing else uses `v20`), else the name `ecx` holds.
- `CDECL(k, f, a, b)`: no register input, and the caller frees the arguments
  right after the call.

The macros expand to the same statements as `CALL_ARGS`/`CALL_CDECL`
(`ecx` is set before the pushes, as it was), so the push order and every
memory access stay the same. `r = ...` is written only where the next read of
`eax` is the call's result (the `eax` name is an alias of `eax`, so the
assignment is a no-op). A callee that reads `eax` or `edx`, an indirect call,
a cdecl call whose arguments are freed later, and a call whose pushes are
split by a memory access keep the plain `CALL` form. Client 41,686 typed
calls, server 24,238; `CALL`/`CALL_ARGS`/`CALL_CDECL` went from 182,506 to
140,820 (client) and 101,873 to 77,635 (server).

## `this->field` (`tools/fields_l2.py`)

The field notes of every method of a class are collected into packed structs
in `readable/runtime/layouts_<target>.h`, at the offset and width the
instructions access:

```c
struct Armor {
    uint8_t _0[0x3c];
    uint32_t maxHitPoints;  /* 0x3c */
    ...
} __attribute__((packed));
```

A method that never writes `this` then sees `ecx` as that struct
(`#define this ((volatile struct Armor *)(uintptr_t)ecx)`) and
`M32(this + maxHitPoints)` becomes `this->maxHitPoints`, the same bytes with
the same width. Other uses of the value are `(uint32_t)this`. A name is a
member only when all its accesses in the class agree on one offset and width
and no other member overlaps it; accesses through other registers keep the
offset names. Coverage follows the field notes: client 86 classes with 100
members (74 uses), server 52 with 66 (49 uses). More field notes give more
members without any change to the tools.

## Calls return into the body (`readable/runtime/l1_runtime.c`)

L1 leaves the body at every call and re-enters it at a resume label
(`switch (c->eip) { case k: goto R_k; }`), so nothing in C survives a call.
The readable runtime runs each body on a per-thread shadow stack instead, so a
call is a plain C call that returns:

- `CALL(k, target)` pushes the call's return stub `..._k` as the return
  address, writes the registers to `*c` (`L2_SPILL`), and calls
  `bfv_call(c, target)`, which records the call on the shadow stack and jumps
  to the target with the guest registers. The target runs natively on the
  game's stack, exactly as before.
- The callee returns into the stub (`BFV_L2_RET`), which finds the record,
  stores the registers the callee left into `*c` and returns from `bfv_call`.
  The body continues after the call with `L2_RELOAD`.
- Shadow stacks: 16 slots of 1 MB in the zero-initialized `.bfvshadow`
  section, keyed by the thread's stack base (`fs:[4]`). A thread claims a slot
  on its first entry and frees it when its outermost body returns. Records left
  by a callee that never returned (an exception unwinding past the body) lie
  below the guest ESP and are dropped at the next entry or return.
- Only restart points that are not calls keep the resume switch (client 41,
  server 18 functions): places where native code re-enters the body with the
  stack restored from EBP.

- `*c` itself lives on the shadow stack: the entry trampoline builds it below
  the gap and copies it to the slot. A body now runs across its calls, so its
  guest ESP can go deeper than the gap (a deep frame, or a callee that pops
  more than it should), and a `*c` left on the guest stack would be overwritten
  by the body's own pushes. Lifted code writes the guest stack only where the
  native code does, plus the dead word below ESP at a call or return.

The verifier (VERIFIER 17; later changes in [LIFTED-C.md](LIFTED-C.md)) gives both machines a TEB and `fs` segment, a flat
32-bit `ss` (without one unicorn wraps pushes at 64 KB), maps the shadow
stacks and treats them like the body's own C frame. Guest-stack accesses by
the body are all compared; only the stubs' pushes below ESP are left out. A
call costs about 100 lifted instructions (spill, `bfv_call`, stub, reload), so
the lifted run of a trace-prefix comparison gets 100 times the native budget,
the same as a full run. The C++ engine checks its
20 s limit from the block hook rather than unicorn's per-start timeout, whose
timer thread costs ~15 ms per call on Windows (44× slower).

## Per-function exceptions

- `readable/strict-flags.json` lists functions lifted with flags live at
  calls. A call through a random pointer can land in native code that reads
  the incoming flags. Real MSVC callees don't, but the verifier sees it.
  Client 51 functions, server 18: those that passed at L1 and failed at L2
  (11 of them added with the shadow-stack runtime).
- `readable/l1-fallback.json` lists functions kept as their verbatim L1 text
  (`"tier": "L1"` in the catalog) because they still failed as strict L2:
  client 16, server 5. Most of them pass as L2 alone but fail after
  relinking: a random-pointer callee reads stack bytes next to a return
  address, and those depend on where the resume stubs land.

## Comments (`tools/comment_l2.py`)

`comment_l2.py` adds comments to every readable file. The code doesn't change:
the tool refuses to write a file unless each function's `code_sha256`
(comments and whitespace stripped) and the whole file's code are the same as
before. The build flags have no `-g` and the runtime has no `__LINE__`, so the
objects are byte-identical too. `lift_l2.py` runs it at the end, so
regenerating keeps the comments. Comments the tool writes start with `/**`.
It removes them before writing them again, so running it twice is safe.

- **x87 formulas.** A symbolic x87 stack turns each float store into one line
  above the code that computes it, for example
  `/** [this + 0x30] = (unsigned)p0 / (unsigned)(p0 + p1) */`. It knows
  constants (`fld1`, `fldln2`, ...), `fyl2x` as `log`/`log10`/`log2`, `fpatan`
  as `atan2`, `fsin`, `fsqrt`, `fxch`, integer operands (`fild`, `fiadd`),
  the unsigned-load idiom (`if <0: + 2^32`), stack slots just stored from a
  variable, and float results of calls (`IAITimer::vf3()`). Branches that use
  an x87 compare get `/** when a <= b */`. At a merge point it keeps the
  formulas only when every path agrees. Loops and labels forget them. Any
  instruction it doesn't model (`fxam`, `fnsave`, ...) stops it for the rest
  of the function. `tools/test_comment_l2.py` assembles random x87 sequences
  with GAS, runs them in unicorn and checks each formula against the stored
  value. All 300 match.
- **Float constants in integer stores** get `= 1.0f` comments.
- **Function headers** get `does:` for simple shapes (getter, setter, constant
  return, field address, forwarder, constructor, destructor), `calls:` (by
  name, including resolved virtual calls), `fields:` (`this+` offsets read and
  written), `globals:` and `loops:`. `slot:` lists the vtable slots that hold
  the function and their names (`vf9 getViewDistance of
  dice::bf::ai::AISettings`), from `lifted/<target>/VTABLES.tsv` and
  `tools/slot_names.py`. A small body the linker folded sits in slots of
  unrelated classes too, so the line can name several.
- **A contents table** at the top of each file lists every function with its
  address and the first sentence of its role (`virtual <name>` when only its
  own class's slot name is known).
- **Hand notes.** `bfv-native-code/notes/<target>/<path>.txt` holds notes
  written by people. A `## file` block goes at the top of the file and a
  `## <address>` block goes into that function's header as `note:`. The notes
  live outside the generated tree, so they survive regeneration.
  `notes/client/bf/ai/AIMain.txt` is the example.
- **Shared names.** `notes/<target>/names.txt` names virtual methods and
  fields once for the whole tree:

  ```
  IAISettings::vf1 = getMaxNBots: returns +0x8, which setMaxNBots writes
  BotBehaviour+0x1c = weights: per-bot float array
  ```

  Every `Class::vfN` in a comment then reads `Class::vfN (getMaxNBots)` in
  headers, and a body line that calls it, or reads or writes a named field of
  `this` (or a copy of `this`), gets a trailing `/** +0x1c weights */`. A name
  also holds for subclasses whose object starts with the class
  (`lifted/<target>/CLASSES.tsv`, from RTTI). Field names apply only inside
  methods of the class, so `+0x1c` elsewhere is left alone.
  `tools/console_names.py` writes `names-console.txt` from the console
  commands (`AiSettings.getViewDistance` names `IAISettings::vf9`; a property
  command's get and set slots are told apart by its handler).
  `tools/code_names.py` writes `names-code.txt` from the machine code, each
  line with its source address: RTTI base parts (`AISettings+0x4 =
  part_IAISettingsModifier`), objects a class creates (`this+N = new X`, so
  `p_X`) and one-field getters and setters of named methods (`getTeam` reads
  +0x1c0, so `team`). Bodies the linker shares between classes are not
  evidence. `names.txt` is read last and wins.

  The binaries carry no debug symbols, so names exist only where the code
  itself shows them: RTTI class names, console command and menu strings, and
  these patterns. Most fields and non-virtual functions have no such evidence
  and keep their offset or address names.

Coverage: the client has 17,710 formulas, 6,159 compares, 4,771 shape
summaries, 282 functions with hand notes (the AI), 2,134 lines with shared
names and 9,927 functions with a `slot:` line. The server has 8,926, 3,539
and 3,362, no hand notes, 1,134 named lines and 6,493 `slot:` lines.

## Commands

```powershell
uv run --with pefile --with capstone python bfv-native-code/tools/lift_l2.py --target both
# a pilot of a few functions rebuilds the output directory with only those, so
# it needs a scratch --out (lift_l2.py refuses --only into readable/)
uv run --with pefile --with capstone python bfv-native-code/tools/lift_l2.py --target client --only 0081c8c6 --out X:/temp/l2-pilot
# C++ verifier core (once): build/vcore/vcore.exe
.\bfv-native-code\tools\vcore\build.ps1
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_lifted.py --target both --lifted-dir readable --states 8 --engine cpp --threads 16
uv run --with pefile --with capstone python bfv-native-code/tools/build.py --tree readable
```

`--engine cpp` runs the emulation in `tools/vcore/vcore.cpp` (unicorn and
capstone from C++, one worker per thread); it gives the same results as the
Python engine (`--engine py`, still the default) and is much faster.

The verifier writes `build/<target>/readable-verification.json`.
`build.py --tree readable` detours the passed, unchanged L2 functions and
writes their summary to `readable/<target>/verified.json`. The detour audit
is the same as for L1. Runtime hashes include `l2.h`.
