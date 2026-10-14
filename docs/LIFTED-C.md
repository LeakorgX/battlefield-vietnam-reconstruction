# L1 lifted C

L1 is the first readability tier: a mechanical, instruction-by-instruction
translation of every indexed function into C. It is not readable source. It
does make the game run from C that can be edited, rebuilt and verified, which is
the basis for modding and for the later L2/L3 clean-up passes (see
[ROADMAP](ROADMAP.md)).

The lifted C is generated from your own game files and is not committed
(`bfv-native-code/lifted/client/` and `lifted/server/` are git-ignored). Only the
tools and the shared runtime (`lifted/runtime/`) are in the repository.

## Pipeline

```
original EXE ──► lift_c.py ──► lifted/<target>/lifted_NNN.c + catalog.json
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

## Verifier (`tools/verify_lifted.py`, VERIFIER 12)

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
- Only `.text` is executable, so random pointers into data fault instead of
  executing it. The whole scratch code page is reset before every run.
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

A function passes when every completed state matches and at least one state
completed. *Unverified* means no state completed natively, so there was nothing
to compare. Those functions stay native.

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

## Status (2026-10-14)

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
