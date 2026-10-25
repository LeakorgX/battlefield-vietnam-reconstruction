# Handoff

2026-10-25 client port to the newer `BfVietnam.exe` (branch `client-exe-port`).
The client target builds against `32aec85d...15c0ea`: the reference client
`79655e9c...17a06a5` plus patch code in six new sections (`.rofcap`, `.altfire`,
`.thrfire`, `.vzoom`, `.seats10`, `.fcam`), a 0x2000-byte `.ctfsnd`, an entry
stub at `00b41d8b` in the raw slack after `.text`, two redirected vtable slots
and in-place changes to 44 indexed functions. The C, the catalogs and the
function index stay those of the reference image. `tools/rebase.py` writes
`bfv-native-code/rebase.json` (147 changed byte runs), and the build keeps
native what they touch: 42 lifted/readable entries and 38 hand-written entries
(the 37 bridges of the artillery evaluator `0099f2a0`, whose scale constants
and second-pass radius the patches change, and the Aimer solve `00a02db0`). See
SETUP.md "Rebasing onto a newer build". Hashes are updated in `TARGETS` and the
eleven catalogs. Patch code outside `.text` counts in the detour audit (on this
image it blocks nothing more; entries outside `.text` stay native as before),
and `static_recomp` emits every code section as instructions. Results (one
machine, MSYS2 GCC): `verify_lifted` readable 33,484 passed / 177 failed / 28
unverified / 42 native, L1 33,528 / 133 / 28 / 42. 168 of the 174 readable
functions that passed in the previous summary also fail on the reference image
with this toolchain; 6 fail only on the new image (see VERIFICATION.md).
`build.ps1`: 38,314 guarded entries, 33,387 of them readable C, 24 mod hooks.
`verify.py` on that readable build: 116,436 passed; the evaluator/solve
checks are skipped and listed. It runs on readable builds after three
harness fixes for older problems (a TEB for `native_oracle`'s machines, the
SEH chain in `vector_oracle`, two A* successor-case inputs; see
VERIFICATION.md).
`recomp.ps1` (regenerated and `-AssembleOnly`) and `--symbolic` byte-identical,
0 diffs (`--symbolic` needed far branches masked during convergence, see
STATIC-RECOMP.md).
Not run in the game. Reports made from the reference image (Ghidra exports,
`live-exceptions.json`) are not evidence for this build.

2026-10-25 Vulkan resume: first GPU bring-up passes on RTX 3060; port still
unfinished. `vk/d3d9_device.c` now exists with a partial COM vtable: creation,
clear/readback/Present, windowed Reset, state/constants/blocks and shared layout
helpers. **Next: device draw path and remaining vtable slots. Do not wire this
incomplete backend into the game.** See [VULKAN.md](VULKAN.md) for exact coverage
and remaining ownership rules. `vk/test/run_gpu.ps1 -NoValidation` passes x86;
`-Architecture x64` passes with validation: zero errors/warnings. Clear pixels
and a state-block subset match system D3D9; Present/gamma/reset execute but
displayed pixels are not compared. x86 validation layers are still absent.
`run_dxso.sh`: 61 shaders pass, zero invalid SPIR-V. Fixed pipeline handle
truncation, bit-32 capture for custom blocks (native PIXELSTATE excludes it),
implicit back-buffer device refs, gamma-off descriptor, and Reset window
replacement. Logs/binaries: `bfv-native-code/build/vulkan-test-*`; private x64
validation layer: `../bfv-reference-local/vulkan-validation/layers64`. Temporary
registry entry used for validation was restored. No game files changed, no
commit/push. Earlier `vk/` sources are in c4318d6; `vk/test/out/` is ignored.

2026-10-24 world renderer correction: the owner rejected the colour-only
cinematic pass as insufficient. [WORLD-RENDER.md](WORLD-RENDER.md) describes
the replacement: actual per-pixel mesh/character material lighting, gloss
masks, sky specular, terrain relief and Fresnel water in ten engine effects.
Build `BfVietnam-WorldRender.exe` with `world_render` and the RPG/sky/AI mods,
excluding `cinematic`. New authored `effects_003.rfa` is mounted only by this
mod. All ten effects pass the game's compiler/technique validation. Live
binding logs pass; final visual/gameplay QA remains in progress. No commit/push.

2026-10-24 cinematic mod: [CINEMATIC.md](CINEMATIC.md). Built
`BfVietnam-Cinematic.exe` with the current RPG/sky/AI mods and a new native
D3D9 world colour-grading pass. GPU tests pass with MSAA 0/2/4/8 and Reset;
PE section audit has zero unexplained changes. Existing game process left
running; new build has not been tested in gameplay. No commit or push.

Updated **2026-10-19, Asia/Baghdad**. Start here when resuming. The full
chronological log up to this point is in [archive/HANDOFF-LOG.md](archive/HANDOFF-LOG.md).

**User direction: commit locally when asked; the owner pushes.** The lifted C is
committed (owner's decision, 2026-10-14). Current focus: make modding easier
without breaking verified work (see [MODDING.md](MODDING.md)). Keep original binaries, assets, generated assembly and private
analysis out of the repository.

## Current state

The project is now a **static recompilation**. See [STATIC-RECOMP.md](STATIC-RECOMP.md).

- `bfv-native-code/tools/static_recomp.py` (run via `recomp.ps1`) emits the whole
  `.text` of each executable as symbolic GNU as source, emits the data sections
  as byte streams, links the reconstructed-C payload, and checks byte identity
  with the installed editable EXEs.
- Verified on Windows 2026-10-13 (`recomp.ps1`, regenerate and `-AssembleOnly`):
  both targets byte-identical, 0 diffs.
  Client 2,240,682 instructions, 359,407 symbolic branches, 4,691 detours, 986
  `.byte` lines. Server 1,275,735 / 194,095 / 3,466 / 448.
- `--assemble-only` / `-AssembleOnly` builds from hand-edited `.s` without
  regenerating; `--install` / `-Install` writes the EXE. Size-changing edits are
  rejected because references are still absolute.
- Reconstructed C: 8,121 routines (inventory in `reports/recovery-registry.json`),
  1.46% of client and 2.08% of server indexed code bytes (`reports/c-progress.svg`,
  refreshed by `analysis/tools/update_progress.py`). New on 2026-10-13: 889
  field/global accessors (42,672 focused comparisons) and 3,947 C++ static-object
  initializers (15,788 focused comparisons); then 691 scalar deleting destructors
  (4,146), 460 initializers with constructor arguments (1,840), 544 `dynamic_cast`
  wrappers (3,264) and 357 virtual clones (2,142), see `docs/modules/`.
  The latest full regression (entry-bridge checkpoint, 2026-10-10) passed
  48,056 client + 46,472 server comparisons, plus 8 live executions per target;
  rerun `verify.ps1` to confirm it against the new payload.
- **L1 lifted C** (user goal: working C for modding first, readability later; see
  [LIFTED-C.md](LIFTED-C.md)). `tools/lift_c.py` translates every indexed function
  into mechanical C, and `tools/verify_lifted.py` compares each one against the
  original under emulation. `build.py` detours only verified, unchanged and
  instruction-aligned functions. Installed build (v14 lift, VERIFIER 13): client 33,519
  and server 20,381 lifted functions run as C. "Runs as C": client 38,409/38,657
  (98.32%), server 23,877/23,986 (98.74%) functions. Live exception probes pass;
  the dedicated server runs 180 s and the client 90 s under `crashdbg` with no faults.
  The lifted C is sorted into the developers' folders and files by
  `tools/sort_lifted.py` (client 75%, server 82% placed by evidence, the rest in
  `unsorted/`). `sort_lifted.py` also writes `SETTINGS.md` for each target
  (script setting → functions that read or write it). It follows
  `this->template` at `+0x4c` and constructor copies. See [MODDING.md](MODDING.md).
  Known-good EXEs: `X:\temp\bfvscan\lift\good-v10` (current hashes below).
  Earlier: `good-v9` (server 180 s and client 90 s live runs clean). Later name-only changes live in comments, which
  `code_sha256` ignores, so they rebuild to the same hashes. Earlier: `good-v8`, `good-v7`.
- **L2 readable C** (2026-10-17, see [READABLE-C.md](READABLE-C.md)):
  `tools/lift_l2.py` rewrites every L1 function as compact C in
  `readable/<target>/`. Flag code is removed where flags are dead, compares
  and branches are fused into `if`, and the PUSH/CALL/RET macros come from
  `readable/runtime/l2.h`. Functions, symbols and resume points are the same
  as L1. Bodies are then nested into `if`/`else`/`while`/`do`/`for` by
  `tools/structure_l2.py` (about 261,000 structured statements, 42,000 gotos left).
  Verified with `verify_lifted.py --lifted-dir readable` (client 33,608 passed /
  31 failed / 92 unverified; server 20,425 / 9 / 39; goto-only L2 was 33,615 and
  20,426, L1 33,587 and 20,412). 16 functions (13 client, 3 server) failed after
  relinking because random-pointer callees read stack bytes next to a return
  address, which depends on stub layout; they're kept as L1 text (`l1-fallback.json`).
  `build.py --tree readable` detours 33,540 client and 20,394 server functions.
  The whole-image flag-ABI pass (`flag_abi` in lift_l2.py) keeps flags live where
  the MSVC x87 helpers hand them across call/ret (`_fload` -> `acos`/`asin`).
  Without it, the L2 server crashed at ~130 s. A bisection of live runs found it,
  and the verifier can't see it. With the fix the L2 server ran 200 s clean.
  The client exits with code 1 at ~120 s, and the original BfVietnam.exe does the same.
  One later client rerun exited at 7.6 s; not investigated (the owner asked to
  stop live runs). Goto-only L2 EXE hashes: client d3494fe6...24d6, server 0d78bbb6...46d6.
  The structured build (client 5041e847...8052, server 5771b009...97a2) was
  checked only statically (verifier and build), with no live game run, per the owner.
  2026-10-23: the owner started the full readable client build and it crashed at
  startup: access violation in `bfv_client_l1_body_00405470` (`ctype<char>::ctype`)
  +0x126 (Windows Application log). Not investigated yet. Mods are played with
  `build.py --tree readable --mods-only` (see MODDING.md, AI Commander).
  Next: typed prototypes and locals instead of registers. Naming stack slots as
  locals was deferred: after a call through a pointer the callee's pop count is
  unknown, and the verifier can't catch a wrong frame guess.
- **L2b register variables** (2026-10-18, see [READABLE-C.md](READABLE-C.md)):
  `tools/regvars.py` renames register values that no call, `ret` or exit can
  see to `v1`, `v2`, ... (client 187,281 in 24,142 functions, server 104,613 in
  15,150), using the whole-image register ABI from `tools/reg_abi.py`. Values
  live across a call stay registers, because `CALL` leaves the body. VERIFIER 14
  compares those functions at the ABI level (catalog `abi`). Full verify: client
  33,616 passed / 25 failed / 90 unverified, server 20,427 / 7 / 39. The 46
  functions that passed at L2 but failed with variables are in
  `readable/fixed-registers.json` (register names kept). 8 of them (client 5,
  server 3) still fail after relinking and stay native code in the build.
  `build.py --tree readable` detours 33,548 client and 20,396 server functions:
  client d836decc...2998, server 11f333a6...63bc. Checked statically only, no
  live run. The `l2.h` header comment still says registers are the native
  ones; it was left unedited so the verified runtime hash stays current.
  The previous L2 tree and reports are in `X:\temp\bfvscan\readable-l2-backup`
  and `rv-<target>-l2.json`.
- **L2b names** (2026-10-18, see READABLE-C.md "Names"): every register value
  is named (`this`, `result`, `p1`, `r_getLocalPlayer`, `vtbl`,
  `this_endBit`, ...; names of values that cross calls are `#define`s of the
  register). Image addresses with notes are named in
  `readable/runtime/symbols_<target>.h` (client 18,977, server 11,192, by
  `tools/symbols_l2.py`). `arg_N`/`var_N` stack slots are `argK`/`local_N`
  (`tools/slots_l2.py`; ESP slots only where `frame = esp` is exact, client
  57,255 uses, server 32,812). Known fields are function-local `#define`s
  (client 456, server 54). Names and addresses compile to identical objects
  (checked with `objdump`). Full verify: client 33,594 passed / 46 failed / 91
  unverified, server 20,418 / 14 / 41. 37 functions that passed before now
  fail (client 26, server 11). 35 of them compile to the same function code
  as before and fail on random-pointer reads of the relinked image or on
  state left over from earlier functions in the same part. Two (server
  005835d0, 007c0010) differ only by `frame` slots, and 005835d0 passes when
  checked alone. They stay native in the build, which `--reuse` can't change
  because it keeps failures. `build.py --tree readable` detours 33,526 client
  and 20,387 server functions: client 1a5cf569...100f, server bf42c8ca...b21e.
  Checked statically only, no live run. The previous tree is in
  `X:\temp\bfvscan\readable-l2b-backup`; copies of the new reports are
  `X:\temp\bfvscan\rv3-<target>.json`. The `l2.h` comment still describes
  registers; `ADDR` was added.
- **L2b calls and fields** (2026-10-18, see READABLE-C.md "Calls with
  arguments"): `tools/calls_l2.py` (called by lift_l2.py) writes calls with
  their arguments (`CALL_ARGS`/`CALL_CDECL` in `l2.h`, same pushes in the
  same order) and saves/restores of callee-saved registers as `SAVE`/
  `RESTORE`. Client 93,652 calls, server 51,917. Only register-only
  statements move in front of the pushes. A first version that also moved
  memory accesses failed 221 more functions, because the verifier compares the
  order of all memory accesses, pushes included (that report is
  `X:\temp\bfvscan\rv5-<target>-loose.json`). `field Class.member` notes
  are now named too (`Class_member`): client 1,095 fields, server 665.
  Verify (full, then `--reuse` for the strict rule): client 33,590 passed /
  48 failed / 93 unverified, server 20,422 / 12 / 39. 26 client and 5
  server functions that passed before now fail. 18 and 3 of them pass when
  checked alone, and the rest differ in values read through random-pointer
  callees (layout-dependent, as in the entry above); 22 client and 9
  server functions that failed before now pass. `build.py --tree readable`
  detours 33,522 client and 20,391 server functions. Checked statically
  only, no live run. The previous tree is in
  `X:\temp\bfvscan\readable-l2c-backup`, reports `rv4-<target>.json`.
- **L2b calls return into the body** (2026-10-18, see READABLE-C.md): the
  readable runtime (`readable/runtime/l1_runtime.c`) runs each body on a
  per-thread shadow stack (16 x 1 MB, inside `.bfvmod`), and a call returns
  through a `BFV_L2_RET` stub back into the C body. There is no resume
  switch, and register variables survive calls. `*c` (struct bfv_cpu) is
  copied onto the shadow stack, so the body writes the guest stack only
  where the native code does. VERIFIER 17 adds TEB/`fs`, a flat `ss`,
  excludes the shadow stacks, compares all body guest-stack accesses and
  gives the lifted run 100x the prefix budget. The C++ verifier core
  (`tools/vcore`, `--engine cpp --threads 16`) checks its deadline in the
  block hook instead of using unicorn's timeout thread (44x faster). Full
  verify of both targets takes about 8 minutes. `strict-flags.json` gained 10
  client functions and 1 server function (now 51 and 18). Verify: client
  33,598 passed / 45 failed / 88 unverified, server 20,430 / 4 / 39. 24
  client functions that passed in the run before the strict additions now
  fail. All 24 pass when lifted alone, and also with their 68 direct callees
  lifted, so they are layout-dependent (see above). `build.py --tree
  readable` detours 33,530 client and 20,399 server functions. Checked
  statically only, no live run.
- **L2b typed calls, `this->field`, fewer gotos** (2026-10-19, see
  READABLE-C.md "Typed calls" and "`this->field`"): `calls_l2.py` writes a
  direct call whose callee ABI is known (pop count, register inputs from
  `reg_abi.py`) as `r = THISCALL(k, f, obj, a, b)` / `STDCALL` / `CDECL`
  (new expression macros in `l2.h`, same pushes in the same order). Client
  41,686 typed calls, server 24,238. The new `tools/fields_l2.py` collects
  field notes per class into `readable/runtime/layouts_<target>.h` and
  writes `this->member` in methods that never write `this` (client 86
  classes / 100 members / 74 uses, server 52 / 66 / 49; it is limited by the
  field notes). `structure_l2.py` adds `break`/`continue`, a looser `else`
  and copies of short `RET` tails: `goto L_` went from 29,332 to 22,386
  (client) and 12,721 to 9,071 (server). Full verify: client 33,613 passed /
  30 failed / 88 unverified, server 20,423 / 10 / 40. 12 client and 8
  server functions that passed before now fail. All 20 pass when lifted and
  verified alone (three of them have unchanged text), so they are the
  layout-dependent churn described above. 27 client and 2 server functions
  that failed before now pass. `build.py --tree readable` detours 33,545
  client and 20,392 server functions: client a56e8715...3c1a, server
  39eedc3f...eb02. Checked statically only, no live run. The previous tree
  and reports are in `X:\temp\l2t\readable-prev` and `X:\temp\l2t\vbak`.
- **Mods** (2026-10-19, see [MODDING.md](MODDING.md)): `bfv-native-code/mods/`
  holds hooks in plain C (`mods/client`, `mods/server`, `mods/shared` for
  both). `tools/mods.py` (called by `build.py`) finds `MOD_BEFORE`/`MOD_AFTER`
  hooks and generates `build/<target>/mod_hooks.c`: an entry stub with the
  hooked function's own trampoline and gap that runs `bfv_mod_run`
  (`mods/runtime/mod_runtime.c`) around the unchanged lifted body. The
  function's detour points at that stub. Mods are compiled with SSE math, and
  fxsave/fxrstor keep the game's x87 state. Game calls go through `bfv_call`
  on the game stack, and logging calls the game's `OutputDebugStringA` import.
  `build/<target>/mod_fields.h` (generated) names the field offsets of the
  readable C (494 client, 287 server), so a `shared/` mod builds for both. The
  example `mods/shared/example_repair_factor.c` doubles repairFactor.
  `tools/test_mods.py` builds a test client (`build/client-custom/`) with
  `mods/tests/client/` and checks the hooks under unicorn. Mods are not
  verified against the original and have not been run live. test_mods.py
  passes 11 of 11 checks. With no files in `mods/`, the build is unchanged: the
  client rebuilt to a56e8715...3c1a in 3 min with cached objects (was 13).
  The installed client EXE still has the example hook (f1827e0f...). `build.py` now caches lifted objects by
  content hash in `build/<target>/l1|l2`, and has `--output`. `build.ps1`
  defaults to `-Tree readable`. `find.py` uses INDEX.tsv (about 1.5 s per
  query) and has `--hook`. `lift_l2.py` refuses to overwrite edited readable
  functions without `--force`.
- **Comments** (2026-10-19, see READABLE-C.md "Comments"):
  `tools/comment_l2.py` comments all of `readable/` without changing code.
  It adds x87 formulas, float-compare conditions, `does:`, `calls:`,
  `fields:` and `globals:` headers, a contents table per file, and hand notes
  from `bfv-native-code/notes/<target>/<path>.txt`.
  - Client: 17,710 formulas. Server: 8,926.
  - Only `notes/client/bf/ai/AIMain.txt` has hand notes so far.
  - `lift_l2.py` runs it at the end.
  - `tools/test_comment_l2.py` checks the formulas against GAS and unicorn
    (300 of 300 match).
  - The tool refuses a write that changes any function's `code_sha256`.
  - The commented trees rebuilt to the verified hashes: client a56e8715...3c1a
    and server 39eedc3f...eb02 (BFV_MOD_DIR empty,
    `--output build/<target>-custom/`).
  - The comments are not intent: hand notes are still needed for most
    functions.
- **VERIFIER 23** (2026-10-21, see LIFTED-C.md "Verifier"): the Phenom CPU
  model (3DNow!), `.tls` slack and `.ctfsnd` as code, pushad events in CPU
  order, the native ABI map for native calls from lifted bodies, and opaque
  calls counted against the budget. `VCORE_WHY` writes a diagnostic file.
  - Unicorn 2.1.4 bug: with `emu_start(count=N)`, a `rep` string instruction
    right after a flag-setting one loses the lazy flags when its first
    access faults into a lazily mapped page. Both engines now count the
    budget in the block hook instead (client 00ab4d24).
  - Lifter: frame restarts after `lea esp,[ebp+4]; mov ebp,[ebp]` resume
    after the load (client 0081c8c6).
  - `lift_l2.py --only` now refuses the default `--out readable`, because it
    replaces the whole output tree.
  - Readable results: client 33,694 passed, 9 failed, 28 unverified; server
    20,458 / 1 / 14. Detoured to readable C: client 33,658, server 20,438.
  - No-mod readable builds (`BFV_MOD_DIR` empty, `--output
    build/<target>-custom/`): client 62267c3c...d175, server
    3b10d82e...0963. `static_recomp.py --target both`: identity=True, 0 diffs.
  - Left native: client 008948f0 (`rdtsc`), and 00499d90, 007f2180 and
    00878f20 (stale-stack reads through random pointers).
- **Symbolic relink** (2026-10-21, see STATIC-RECOMP.md "Symbolic mode"):
  `tools/symscan` (C++) classifies every absolute-looking 4-byte value and
  finds the switch tables. `static_recomp.py --symbolic` emits tables,
  operands and data pointers as symbols and links with GNU ld. Both targets
  are byte-identical after the link. A relink with `.data` and later sections
  moved by 0x10000 changes only symbol fields, each by exactly the shift.
  `spelling()` now writes far-pointer loads as `fword ptr`. A bare `ptr`
  had been an undefined symbol. PE directories (imports, exports, TLS,
  resource data entries) are emitted as symbols, and the header's layout
  fields come from the link (`compose_image`). symscan gained
  `select_code_pointer` and `compared_code_pointer` (one each per target).
- **Link mode, unified link and growth** (2026-10-21, see STATIC-RECOMP.md):
  `tools/linkmode.py` makes the C payload's addresses symbols (gated by a
  shift relink with `tools/linkmode/relocheck.cpp`). `--link-c` links it with
  the image in one ld run; `--grow-text 0x3000` moves every later section and
  the payload by 0x3000. Both gates pass for both targets with 0 unexplained
  bytes.
  - `--grow-at 00401030` inserts the bytes near the start of `.text`, so
    almost all code moves too. It passes for both targets after three
    fixes: in-instruction branch targets are anchored to the nearest label,
    symscan treats import name text as constants (`import_name_text`), and
    `linkmode.exe --src` makes the hand-written `src/` modules, `target.h`
    and the mods link-mode symbolic. That covers 187 `target.h` names, plus
    10,096 client and 7,322 server catalog literals, decided against the
    native bytes each function replaces.
  - Re-verified after these changes: `--symbolic` identity for both
    targets, and `--link-c` with payloads identical to `linkmode.py`'s.
  - Exact spellings in symbolic mode: wide immediates (`offset bfv_wide_X`),
    segment-prefix mnemonics, near branches into `.ctfsnd` and symbolic short
    detours. Byte-encoded spellings: client 3,181 → 2,767, server
    1,345 → 1,205. L1-reachable ones: 155 → 0 and 26 → 0. Identity holds for
    both targets.
  - In-function edits (`--insert VA:ASM`, `--replace VA:ASM`) are gated by
    `tools/textcheck` (C++): instruction by instruction, then relocheck over
    the other sections. With `--link-c`, a 130-byte insert plus a replace
    passes on the client, and an insert plus a removal passes on the server,
    with 0 unexplained bytes. `.text` is assembled with `--no-pad-sections`,
    and its virtual size grows exactly.
- Whole-game acceptance: **0 of 7** milestones (`reports/project-milestones.json`).

Installed editable EXE hashes (match `build/<target>/manifest.json`):

```text
client 526c4142e5b394441a6fa863a41e8d5cc069980d4059c1b8f3eba81a7632938f
server 3b25eeee948546598835c5a5d8aea1b913f4e4ba4dd0ebee4297595f6f675c66
```

## Pending

0. L1 remaining gaps (see [LIFTED-C.md](LIFTED-C.md)): unverified functions
   (no random state completes natively, even with the 400k budget), failures
   that read stale return addresses from dead stack through random pointers,
   entries blocked by the detour audit (interior branch targets, misaligned
   index entries) and `stack_untracked` CRT SEH-prolog functions. (Code in
   `.ctfsnd` is handled since VERIFIER 23.)
1. Relocatable image: symbolization and the ld relink are done (`--symbolic`).
   PE directories and header layout fields are generated from the link. The
   headers are assembled from `headers.s` and the import/export tables are
   written as names, hints and ordinals, with `imp_<DLL>_<function>` symbols
   on the IAT slots (identity holds for both targets). The
   C payload links with the image in one run (`--link-c`), and sections grow
   with the later ones moving up (`--grow-text`, gated). Code moves inside
   `.text` too (`--grow-at`, gated). Real instruction edits inside functions
   (`--insert`/`--replace`, gated by `tools/textcheck`) pass for both
   targets under `--link-c`. No byte-encoded holdout is an L1 instruction.
   See STATIC-RECOMP.md.
2. Continue C reconstruction where it unlocks milestones. The artillery
   evaluator's intervals are replaced from `0099f2a0` through the final
   selection (`artillery_search_prefix` covers `0099f3cf..0099f6c3`, and
   `artillery_weapons` covers `0099f8d7..0099fa61`; see `tools/build.py`).
   Trajectory, firing and live parity remain open. Follow the evidence rules in
   [AGENTS.md](../AGENTS.md).
   The bot Aimer (`src/aimer.c`, [AIMER](modules/AIMER.md)) compiles from C as of
   2026-10-21. `verify_aimer.py` passed 1570 cases per target against `--output`
   builds; the full `verify.py` run and `export_reports.py` have not been rerun
   with it. The strategic A* neighbour expansion and grid queries
   (`src/astar_strategic.c`, [ASTAR-STRATEGIC](modules/ASTAR-STRATEGIC.md),
   13 entries per target) followed the same day; `verify_astar_strategic.py`
   passed against `--output` builds and is wired into `verify.py`, also not yet
   rerun in full. The AI task scheduler routines (`src/task_scheduler.c`,
   [TASK-SCHEDULER](modules/TASK-SCHEDULER.md), 5 entries per target) came
   next; `verify_task_scheduler.py` passed 3000 cases per target against
   `--output` builds and is wired into `verify.py` the same way.
   The strategic search setup, bit set init and search run with its disc mask
   (`009dec20`, `009ceda0`, `009cf5c0`; 3 more entries per target in
   `src/astar_strategic.c`) followed; `verify_astar_setup.py` passed 3000 cases per target
   against `--output` builds and is wired into `verify.py` the same way.
   `TaskAStarLocalSearch::execute` and `TaskAStarStrategicSearch::execute`
   (`009de160`, `009de230`; 2 more entries per target in
   `src/task_scheduler.c`) came next; `verify_search_task.py` passed 2000 cases
   per target against `--output` builds and is wired into `verify.py`.
   Task unschedule and queue and the two player map search executes
   (`009de020`, `009de030`, `009de300`, `009de390`; 4 more entries per target)
   followed, then `TaskStrategicSearch::execute` (`009de410`, a state machine
   on `+0x300`), `TaskGetStrategicCellInfoNo::execute` (`009de710`) and the
   strategic cell distance (`009de5c0`, x87 compare and `_ftol2` kept in a
   small asm helper; the output register must be early-clobber because
   FNSTSW writes EAX), then the cell query and player map search inits
   (`009de100`, `009de670`; naked entry stubs keep ECX, which native callers
   rely on); `verify_task_queue.py --count 4000` passed 12000
   cases per target against `--output` builds and is wired into `verify.py`.
   The A* searcher step itself (`00a09340` / `007bd990`, the time-sliced
   open-heap loop with open and closed hashes and relaxation) is in
   `src/astar_searcher.c` ([ASTAR-SEARCHER](modules/ASTAR-SEARCHER.md));
   so are its heap helpers (push `00a09090`, update `00a08e70`, pop
   `00a08a60`, sift-down `00a08870`, closed-list removal `00a089e0`; equal
   totals are a live coin flip from the game's random generator), reset
   (`00a09190`), node alloc (`00a092c0`) and the result output: path copy `00a08ad0`, path into a ring
   `00a08b50` with the ring's extension `00a08440`, closed-list points
   `00a08c60`, and the run-to-completion wrapper `00a09880` (a naked frame,
   because the step reads the time pointer's low byte) and the path delivery
   `00a098b0` (path through a deque to a callback object, then reset). The pool allocator and free and the vectors' push_back and
   resize stay native. `verify_searcher.py`
   passed 2000 step cases per target against `--output` builds (5789 expansions,
   12561 heap pushes or updates, reopenings from the closed list included)
   and 2000 direct heap-helper cases per target (640 tie draws, 346
   push_backs) and 2400 direct reset, alloc and path-output cases per
   target, caught twenty-two mutations (relax `<` as `<=`, a kept successor slot, an
   inverted tie, a sift-down that keeps its moving entry, unclose advancing
   from the node; 0-based ring ids, the extension returning the request, a
   skipped second closed list, a signed node-limit compare, an off-by-one
   path cut, a wrong wrapped-ring range; reset's EAX, heap size and free
   loop; the run wrapper's in-progress flag, running slot and time slot; the
   delivery's step id, deque block wrap, skipped reset, uncleared offset and a
   single-node path left undelivered) and is wired into `verify.py`.
   `009e4620` (named `astar_engine` in the spec) is not the A* loop: it is a
   setup routine that allocates and names CellMaps (`0093dfb0`) and fills
   lists and trees, with many native callees. The scheduler's run loop was
   not found among the AI routines: the only callers of the slice timer are
   the two search tasks' `execute`, and every Task `execute` found so far is C.
   `AStarLocalSearch` estimate (vf2, `009e5180`), goal test (vf4, `009e63f0`),
   set start (vf6, `009e5da0`) and set goal (vf7, `009e5e10`) are in
   `src/astar_local.c` ([ASTAR-LOCAL](modules/ASTAR-LOCAL.md)); the layer
   lookup, terrain height, rounding helper and ftol2 stay native.
   `verify_astar_local.py` passed 4000 cases per target (nodes overlapping the
   search fields included), caught seven of eight mutations and is wired into
   `verify.py`. The missed one drops a float store that only raises the
   precision flag: Unicorn sets no x87 exception flags, so no focused verifier
   checks them (see STATIC-RECOMP, Verified results).
   Next AI candidates: any task `execute` methods not yet found, the local
   search's successor generation (vf3), and `SAI.c` / `AIMain.c` in the
   lifted tree.
   Mods use the L2 runtime (`bfv_call`, shadow stack), so `build.py --tree lifted`
   (its default) leaves `mods/` out and says so; `build.ps1` defaults to readable.

## Commands

Run from the repository root in PowerShell, with `$env:BFV_GAME_DIR` set.

```powershell
.\bfv-native-code\build.ps1                       # C payload + editable EXEs
.\bfv-native-code\recomp.ps1                      # regenerate assembly, check identity
.\bfv-native-code\recomp.ps1 -AssembleOnly        # build from edited .s
.\bfv-native-code\verify.ps1 -Target client       # full regression (also server)
uv run --with pefile --with capstone python bfv-native-code/tools/verify_live_exceptions.py --game-dir $env:BFV_GAME_DIR --target both
C:\Python314\python.exe bfv-native-code/tools/export_reports.py
C:\Python314\python.exe analysis/tools/update_progress.py
```

Ghidra (read-only) usage is in `analysis/tools/ghidra/README.md`. Private
checkpoints live in `..\bfv-reference-local\checkpoints`.
