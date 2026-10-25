# L1 lifted C

L1 is the first readability tier: a mechanical, instruction-by-instruction
translation of every indexed function into C. It is not readable source. It
does make the game run from C that can be edited, rebuilt and verified, which is
the basis for modding and for the later L2/L3 clean-up passes (see
[ROADMAP](ROADMAP.md)).

The lifted C is committed (owner's decision, 2026-10-14) together with
`lifted/<target>/verified.json`, the list of functions that passed, so a fresh
clone builds the same detours. It is sorted into the developers' own folders and
files by `tools/sort_lifted.py` (see [MODDING](MODDING.md)).

## Pipeline

```
original EXE ──► lift_c.py ──► sort_lifted.py ──► lifted/<target>/<folder>/<File>.c + catalog.json + INDEX.tsv
                                   │
                 verify_lifted.py ─┴─► build/<target>/lifted-verification.json
                                   │
                        build.py ──┴─► detours only for verified, unchanged functions
```

```powershell
uv run --with pefile --with capstone python bfv-native-code/tools/lift_c.py --target both
# parallel verification (N parts, then merge); --reuse keeps earlier passes
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_lifted.py --target client --states 8 --part 0/9
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_lifted.py --target client --merge 9
.\bfv-native-code\build.ps1
```

Before starting parallel parts, link each target's verifier blob once with a single
`--only <address>` run. Otherwise the parts race to build it.

## Lifter (`tools/lift_c.py`)

- **Integer state.** General registers and flags are C locals; GCC drops flag
  computations that are never read. Memory goes through volatile `M8/M16/M32`,
  which keeps native order, width and count.
- **x87, MMX, 3DNow! and SSE.** Each instruction is a one-instruction `asm`
  statement over the real hardware registers. Lifted files are compiled with
  `-mgeneral-regs-only`, so compiler code never touches FPU or SIMD state.
- **Stack.** Pushes and pops use the real game stack through the guest ESP.
  The C body runs `BFV_L1_GAP` (0x400) bytes below the entry ESP. A function whose
  static stack depth exceeds `GAP-0x80` gets its own gap G (depth + 0x80, rounded
  up to 0x100, at most 0xf00) through `BFV_L1_ENTRY_DEEP`/`BFV_L1_RESUME_DEEP`.
  Deeper functions are rejected (`stack_too_deep`). G stays below one page so a
  stub never skips the stack guard page.
- **Frame restarts.** `lea esp, [ebp+d]` with an untracked EBP leaves the body
  through a resume stub, which rebuilds the C frame below the new ESP. The
  trampoline's continuation word sits at ESP-4, so for
  `lea esp, [ebp+4]; mov ebp, [ebp]` (which reads the saved EBP from that same
  word) the body leaves after the load instead (client d3dx `0081c8c6`).
- **Exits.** Direct jumps out of the function, falling through into the next
  indexed entry (shared tail code) and reaching a trap instruction all leave the
  C body and continue natively at that address.
- **Short functions.** A function shorter than the 5-byte detour is still lifted
  when the bytes after it up to entry+5 are `int3`/`nop` padding.
- **Calls.** Every call pushes a per-call *resume stub* as the return address
  and leaves through the trampoline. The callee (native, hand-written C or
  lifted) returns into the stub, which re-enters the body after the call. Jumps
  that leave the function continue at their native target.
- **Instruction coverage.** Handled: string instructions (`movs/stos/lods/cmps/scas`,
  plain or `rep/repe/repne`), rotates of every width, `shld/shrd`, `rcl/rcr` by
  one, and `bt/bts/btr/btc` including a register bit offset into memory (the
  offset selects the word, as on the CPU). A shift or rotate of memory by a zero
  count still reads and writes the operand back, like the native instruction.
  Rejected and reported: `in`, `popaw` and 16-bit push/pop (one function each).

Current output (v13): client 33,706 of 38,657 indexed functions, server 20,460 of
23,986. The rest are hand-written C (which always wins), or rejected:
untracked ESP (CRT SEH-prolog functions and code decoded from data), too short
to detour, entries that are themselves traps, two functions deeper than 0xe00,
and the unhandled instructions above.

## Runtime (`lifted/runtime/`)

- **`l1.h`.** CPU struct, memory and flag macros, and the entry and resume stub
  macros.
- **`l1_runtime.c`.** The shared `_bfv_l1_enter` trampoline:
  1. Stores the guest registers and EFLAGS.
  2. Runs the body.
  3. Reloads the registers and continues at `cpu->eip`.

  `_bfv_l1_enter_deep` is the same entry for deep stubs, which pass their own
  gap; it joins the common tail after building the CPU struct.

  The only guest-stack word written on the way out is `final ESP - 4`, which is
  where the native `ret` leaves its return address anyway.

## Verifier (`tools/verify_lifted.py`, VERIFIER 23)

The verifier runs the original function and the compiled lifted body on two
Unicorn machines from the same random state. It compares:
- general registers, ESP, EFLAGS;
- x87, MXCSR and XMM state;
- every memory event;
- every call event (target, registers, 20 bytes of stack arguments).

The machines are set up as follows:
- Callees are stubs that return a fixed value.
- A call into non-code memory is an *opaque call*: it is recorded and returns a
  value derived from the target, the same on both machines.
- Pages touched outside mapped memory are mapped lazily, with contents that
  depend only on the page and the state.
- The image, the stubs and the blob are restored before every run.

States and isolation:
- 8 random states per function; when they do not complete, up to 3x as many
  draws, the later ones *small* (counts and indices from 0..16 or below 64,
  some arena words pointing at themselves).
- A state whose native run loses the stack, leaves the function or runs out of
  budget is retried once with a frame-like stack (`frame_state`). A native run
  that still exceeds its budget or leaves the function is skipped without
  running the lifted body.
- Only the code sections are executable, so random pointers into data fault
  instead of executing it. Code sections are every section marked executable:
  `.text`, the client's `.ctfsnd` and patch sections (`.rofcap`, `.altfire`,
  `.thrfire`, `.vzoom`, `.seats10`, `.fcam`), and the raw slack the loader
  maps with a section's last page: the 1.21 patch code after the 9-byte `.tls`
  section (0xe23018..0xe231a0) and the client's entry stub after `.text`
  (0xb41d8b). The whole scratch code page is reset before every run.
- A catalogued function whose bytes the executable's own patches changed since
  the image it was lifted from (`rebase.json`, see SETUP.md) is not run: its
  result is `native`, and build.py keeps it native.
- Every indirect call the function makes into `.text` is opaque on both
  machines (recorded with target, registers and arguments, then it returns a
  fixed value), like the stubbed direct callees. Real code reached through a
  random pointer may read its return address, which differs between the two.
- A function none of whose states completed because the native run kept running
  out of its 20,000-instruction budget gets a second round with 400,000.
- Runs are bounded by instruction counts and by 20,000 recorded events. A
  20-second wall-clock limit only catches runs the count misses; such a run is
  inconclusive (skipped), never a mismatch. Earlier a 2-second limit cut lifted
  runs short at random when many parts ran in parallel.

- A function that still has no completed state is compared on *trace
  prefixes*: native runs stopped at 20,000 instructions, against a lifted run of
  the same state; the lifted events must start with the whole native trace.
- A direct call of the function to itself is opaque on both machines too
  (recursion would otherwise run native code on one side and C on the other).

- Both machines use unicorn's Phenom CPU model (`UC_CPU_X86_PHENOM`) with
  CR4 = 0x200, so 3DNow! (d3dx and the client's vector code) runs instead of
  faulting, and SSE stays enabled.
- `pushad` events are reordered to the CPU's order: unicorn stores EDI first,
  at the lowest address; the CPU and the lifted body store EAX first.
- A native call made from a lifted body (EXIT_TO, resume stubs) gets the
  callee's register ABI from the native machine's map, so ECX/EDX are
  poisoned (0x5a5a5a5b) after it exactly when they are on the native side.
- Running out of opaque calls (64) counts as running out of budget, not as a
  mismatch.
- The instruction budget is counted in the block hook (instructions per block,
  cached), not with `emu_start`'s `count`. The count adds a code hook on every
  instruction, and with it unicorn 2.1.4 loses the lazy flags of the
  instruction before a `rep` string instruction whose first access faults into
  a lazily mapped page: `cmp; rep movsd; jb` then falls through natively
  (client `00ab4d24`). As before, the budget applies per `emu_start`.
- With `VCORE_WHY=<file>` the C++ core appends, per non-passed function, the
  native run errors that kept its states from completing.

A function passes when every completed state matches and at least one state
completed, or, with no completed state, when every prefix matches and at least
one prefix was compared. *Unverified* means no state completed natively, so there was nothing
to compare. Those functions stay native.

## Sorting (`tools/sort_lifted.py`)

Gives each function a home from evidence in the EXE, in this order: the
developers' source paths in assert strings, RTTI vtables (virtual methods; class
and namespaces from the mangled name), single-class vtable stores
(constructors/destructors), runs between two anchors of the same file or folder
(MSVC keeps an object file's code together), and functions called only from one
file or folder. The rest goes to `unsorted/`. Only whole function texts move, and
each is checked against its catalog `c_sha256` before and after, so
verification carries over. `lift_c.py` runs it after every full lift, and
refuses to run while any function is edited unless given `--force`.

It also writes `CONSOLE.tsv`: each console command class (`ConsoleClassN` in an
anonymous namespace), its name and argument types from the constructor's
strings, its handler methods, and the direct calls from those handlers that
fewer than nine commands share (the game function it reaches, when it is not
inlined).

Names and notes (listed in [MODDING.md](MODDING.md)) are comments only.
`build.py` compares `code_sha256` (the C without comments and blank space) with
the lift, so renaming and annotating never counts as an edit. Moving functions
between files changes link order and so the EXE hash, not the code. The
evidence comes from `tools/game_names.py`:
- console command categories, by replaying the C++ static initializers;
- class IDs, from `mov eax, [id]; ret` methods;
- fields and methods that command handlers get or set, from a small symbolic
  evaluator over each function (`this`, arguments, globals, loads, calls);
- `this` types, propagated through direct calls;
- singleton pointers;
- virtual-call targets of objects of known class.

## Build integration (`tools/build.py`)

A lifted function is detoured only when **all** of these hold:

1. It passed, with the same `c_sha256` (function text) and `runtime_sha256`
   (runtime sources) that were verified.
2. No hand-written C owns it.
3. Its entry is an instruction start of the linear `.text` decode, the same
   decode `static_recomp.py` reassembles byte-exact. The function index has a
   few entries that start inside another function's instruction. Patching such
   an entry cuts a native instruction in half. On 2026-10-13 this crashed the
   dedicated server ~40 s after loading a level: server `0x757063` sits inside
   `lea edi,[esp+ebp*4+0xc4]` of `0x7559f0`.
   Exception (*resynced*): the linear decode often runs through the end of a jump
   table into the next function. Such an entry is still detoured when another
   indexed function calls or jumps to it directly and no branch targets the
   decode instruction that covers it.
4. Nothing enters its first five bytes other than at the entry:
   - no branch target there, unless every branch to it is a direct branch
     inside the same lifted function and that function has no indirect jump;
   - no image pointer there: an absolute immediate or displacement of a `.text`
     instruction, a 4-aligned `.text` dword next to another `.text` address
     (jump table), or a 4-aligned dword of another section;
   - no exit of another lifted function there.
5. It does not contain the vector-insert catch funclet that
   `verify_live_exceptions.py` instruments.
6. The detour doesn't overlap another patch.

`python build.py --no-lifted` builds without any lifted detours.

## Status (2026-10-14, v14 lift, VERIFIER 13)

Client 33,587 passed / 49 failed / 95 unverified; server 20,412 / 19 / 42.
Detoured: client 33,519, server 20,381. Runs as C: client 98.32%, server 98.74%
of functions. Live exception probes pass; the server ran 180 s and the client
90 s under `crashdbg` with no faults. The table below is the earlier v13 run.

### Earlier (v13, VERIFIER 12)

v13 lift, full verification under VERIFIER 12 (parallel parts, `--reuse`):

| Target | Lifted | Passed | Failed | Unverified | Detoured in the EXE |
| --- | ---: | ---: | ---: | ---: | ---: |
| Client v13 / VERIFIER 12 | 33,706 | 33,368 | 59 | 279 | **33,267** |
| Server v13 / VERIFIER 12 | 20,460 | 20,303 | 30 | 127 | **20,259** |

Passed functions that are not detoured: client 39 misaligned, 42 resynced,
60 interior targets, 1 instrumented funclet, 1 hand-written; server 18, 13, 25
and 1 respectively.

Hand-written C plus detoured lifted C now runs **38,157 of 38,657 client
(96.78%)** and **23,755 of 23,986 server (97.03%)** indexed functions
(`update_progress.py`, "runs as C").

Checks on both builds:
- `verify_live_exceptions.py` passes for both targets.
- The editable dedicated server runs 180 s and the editable client runs 90 s
  under `crashdbg` with no faults (only the normal thread-name exception).

The remaining failures read stale return addresses from dead stack through
random pointers, use `rdtsc`, are hook stubs outside `.text` (`.ctfsnd`), or
exceed the lifted budget; they stay native.

Whole-game milestones are unchanged (0 of 7): L1 is mechanical C, and parity
runs beyond these smoke checks are not done yet.

## Next

- Recorded input states from live sessions, to verify the functions that fault
  on random states.
- A per-function gap for `stack_too_deep`; the remaining unhandled instructions.
- L2 structuring (prototypes, locals, `if`/`while`, flag elimination).
