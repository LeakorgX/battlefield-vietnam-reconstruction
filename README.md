# Battlefield Vietnam — engine reconstruction in progress

**Status: incomplete. The full Battlefield Vietnam executable has NOT been
reconstructed into buildable source code or open-sourced.**

The project's goal is to recover the **original, unmodified Battlefield Vietnam
engine**, including gameplay, AI, physics and multiplayer, as readable code that
can be edited and compiled. Matching the original game's behavior is the target.
A procedural game inspired by Battlefield Vietnam does not satisfy that target.

What exists today is a working native AI replacement/modding build, a collection
of executable-analysis tools, evidence from local analysis, and an earlier Bevy
prototype. This README explains exactly which parts work and which do not.

## Quick answers

| Question | Answer today |
| --- | --- |
| Is this the full game source code? | **No.** Most engine logic remains in the original executable. |
| Can I build the original game without its original EXE? | **No.** The native build reads a hash-checked original EXE and retains most of its machine code. |
| Can I edit some actual original-game AI behavior and compile an EXE? | **Yes.** The reconstructed interpreter and rating rules can be edited and compiled into client/server copies. |
| Can I edit all gameplay and physics in readable C/C++? | **No.** Those systems have not been reconstructed. |
| Is artillery aiming, target selection and firing fully reconstructed? | **No.** That specific behavior has not yet been recovered into editable implementations. |
| Is the Bevy prototype an exact remake? | **No.** It uses procedural stand-ins and different game logic. It is retained as an earlier experiment. |
| Does decompiling 41,591 functions mean 41,591 functions are buildable? | **No.** Most of those outputs are approximate pseudocode, generated locally. |
| Is complete multiplayer compatibility verified? | **No.** The native build uses the existing original networking engine. The Bevy experiment has its own protocol. |
| Are the original game binaries/assets in this repository? | **No.** You supply your own installation. |

## What was actually accomplished

### 1. Read-only analysis of the client and dedicated server

The installed 32-bit client and dedicated-server executables were imported into
Ghidra 12.1.4. Tooling recovers MSVC runtime type information (RTTI), class-linked
virtual function tables, candidate method entries and direct call relationships.

| Analysis result | Client | Dedicated server |
| --- | ---: | ---: |
| RTTI class candidates | 5,533 | 3,924 |
| AI class candidates | 242 | 240 |
| Gameplay class candidates selected by name | 132 | 127 |
| Corrected AI/gameplay function seeds | 2,164 | 2,032 |
| C-like function exports completed locally | 25,507 | 16,084 |
| Functions whose C decompilation failed | 2 | 1 |
| Unresolved seeds after executable-section filtering | 0 | 0 |

The total **41,591 exports counts outputs across both binaries**. It is not a count
of independent source functions, a percentage of reconstruction, a source-code
coverage measurement, or proof that all executable code has been identified.
Client/server implementations overlap. Function discovery and inferred signatures
can be wrong. Indirect calls need separate analysis.

The original binaries are identified by SHA256:

```text
BfVietnam.exe
79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5

bfvietnam_w32ded.exe
86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d
```

These identify the inspected local files. They are not independent certification
that every installation file or asset archive is an authentic unmodified release.
The installation also contains modified assets and an Oblivion custom map; the
current native runtime tests therefore do not certify a pristine vanilla corpus.

### 2. An editable, compilable AI interpreter

[`bfv-native-code/src/native_ai.c`](bfv-native-code/src/native_ai.c) reconstructs the
control flow of the `BAPInterpreter` virtual method:

- Early return and output flags.
- Plan type and object type selection.
- Per-object-type plan eligibility masks, including x86 shift masking.
- Native fallback-plan dispatch and specialized-handler dispatch.
- Context reset, begin/end bookkeeping and context notifications.
- Disabled-plan events and generation-checked object-handle lookup.
- Correct x86 `thiscall` entry/return conventions.

This is a real implementation that is compiled and executed by the original game.
It is more than a decompiler text dump. It still depends on native object interfaces,
context helpers and ownership routines in the original engine.

### 3. Editable AI rating rules and a native EXE build route

[`bfv-native-code/src/mod_rules.c`](bfv-native-code/src/mod_rules.c) exposes editable
plan eligibility, bailout rating and vehicle-change rating rules.

The build compiles C into a new native PE section and redirects three verified
virtual-table entries in copies of the original executables:

| Replacement | What runs as new source | What still runs in the original engine |
| --- | --- | --- |
| AI plan interpreter | Reconstructed dispatch/control flow and eligibility rule | Native objects, plan handlers, context/list helpers and event receivers |
| Bailout rating method | Cached rating retrieval and editable returned-rating rule | Recomputed rating's complete native numerical calculation |
| Vehicle-change rating method | Editable returned-rating rule and ABI wrapper | **All original candidate evaluation and scoring** |

The original EXEs remain unchanged. Generated files are named:

```text
BfVietnam-editable.exe
bfvietnam_w32ded-editable.exe
```

Those generated EXEs contain original engine bytes and are **not included here**.
Adding another C file to the source folder and a guarded entry to
`extra-patches.json` extends the replacement mechanism; it does not automatically
reconstruct that method's implementation or prove its behavior.

### 4. Verification against actual original instructions

The current verification executes original and compiled machine code under
Unicorn, using the same controlled inputs.

| Checks per binary | Count | What they establish |
| --- | ---: | --- |
| Interpreter comparisons | 1,920 | Results, flags, dispatch, context notification phases, event calls and stack cleanup for the enumerated cases |
| Cached bailout comparisons | 32 | Exact returned float32 bits, including negative zero and rounding-sensitive values |
| Vehicle wrapper ABI checks | 32 | `this`/argument forwarding and returned float bits with a controlled native callee |
| **Total per binary** | **1,984** | Scoped verification; not complete game parity |
| **Client + server total** | **3,968** | Both inspected targets passed |

The interpreter cases vary plan types 0, 1, 7, 31 and 33; object types 0 and 3;
early return; allowed/disabled masks; fallback/specialized handlers; handler results;
optional context; event components; and output-flag combinations.

**Limits:** callbacks and context/event helpers are controlled in these tests.
They do not prove every real object lifetime, every native callback's behavior,
full vehicle scoring, artillery behavior, long-running stability, physics,
network synchronization, rendering or a complete match.

Earlier 384 checks executed original instructions only. They were reference
oracles, **not tests of a rebuilt engine**. The later 3,968 checks include newly
compiled replacement code. See [verification details](docs/VERIFICATION.md).

### 5. Runtime execution in the actual client and server

Read-only inspection of test processes confirmed:

- The correct generated executable was running.
- All three replacement virtual-table pointers were loaded.
- The compiled payload's immutable bytes matched the linked code.
- The client had executed the reconstructed interpreter **545,378 times** and
  the vehicle wrapper **3,523 times** at the retained observation.
- The dedicated server had executed the interpreter **5,553 times** and the
  vehicle wrapper **48 times** at its retained observation.

The user confirmed manually closing the latest client test. The temporary
dedicated server was stopped after verification. These counters establish live
execution, not whole-match or whole-game equivalence. The retained observations
contain no PID, profile name, local installation path or network address.

### 6. Earlier Rust/Bevy multiplayer experiment

The initial [`bevy-remake/`](bevy-remake/) project is included because it was work
done during the project. It builds a separate procedural conquest prototype with
its own client/server simulation and networking. Its earlier validation included
11 tests and a Clippy pass.

**The user rejected it as an exact remake.** It does not implement original BFV
physics, original AI or the original multiplayer protocol, and must not be
presented as evidence that those systems were recovered. It is retained for
history and potentially reusable scaffolding, not as the finished target.

## What “decompiled,” “reconstructed,” and “open source” mean here

1. **Disassembly** describes executable instructions, addresses and byte-level
   control flow. It does not recover the original source project.
2. **Decompiler output** translates instructions into approximate C-like text.
   Unknown names such as `DAT_...`, guessed types and artificial expressions are
   common. A successful export does not mean it compiles or behaves correctly.
3. **Reconstructed source** recovers interfaces/layouts/logic sufficiently to write
   and compile an implementation. Each implementation still needs comparison
   against the original behavior.
4. **A full reconstructed engine** would compile the engine from source without
   retaining the original EXE as its implementation. That has **not been achieved**.

The first attempt to compile a raw interpreter export failed on missing types,
engine globals and non-C decompiler expressions. The subsequent native project
reconstructed that interpreter and supplied the correct binary interfaces. It did
not solve the equivalent problems for all the other exports.

## What remains

| System | Current state | Work still required |
| --- | --- | --- |
| AI interpreter | Reconstructed and scoped comparisons pass | More real-object traces, invalid/state-changing cases, lifetime behavior and sustained matches |
| AI behavior selection/scheduling | Class/function leads recovered | Reconstruct behavior priorities, transitions, scheduling and object state |
| Artillery AI | **Not reconstructed** | Trace target selection, firing decisions, aim/drop calculations, vehicle entry/exit and plan handlers |
| Bailout AI | Editable returned rating; recomputation remains native | Recover numerical helpers, initialization and the full decision path |
| Vehicle AI | Native scoring wrapped with editable returned rating | Recover candidate evaluation, driving/flying, avoidance and state transitions |
| Weapons/projectiles | Names and function leads | Recover ammunition, firing cadence, spread, damage, projectile simulation and interactions |
| Infantry movement | Not implemented from native source | Recover controller integration, collision responses, timing and animation dependencies |
| Physics/collisions | Function leads; collision-handler C export fails | Recover shapes, integration, forces, constraints, response and object ownership |
| Networking | Native engine retained | Recover serialization, event IDs, replication, tick timing, reliability and client/server state transitions |
| Rendering/UI/audio | Native engine retained | Recover engine interfaces and behavior; establish a deliberate replacement strategy |
| Startup/memory/object management | Native engine retained | Reconstruct globals, initialization, allocation, destruction and runtime dependencies |
| Build/link system for full engine | Not available | Supply recovered types/headers, complete implementations, dependency interfaces and a real program entry point |
| Vanilla 1:1 validation | Not complete | Establish a pristine version-matched corpus and reproducible state/input/render/network traces |
| Whole-engine standalone source build | **Not achieved** | Remove dependence on original executable code after subsystem reconstruction and validation |

There is no defensible completion percentage or reliable completion date at this
stage. Counting exported functions would give a misleading impression of progress.
See the [roadmap](docs/ROADMAP.md) for milestones and acceptance criteria.

## Build the current native mod

The native build requires Windows, 32-bit MinGW GCC/binutils, Python through `uv`,
and your own matching game installation. Ghidra is needed for additional analysis,
not for compiling the existing native source.

```powershell
# From this repository. Choose your own installation path.
.\bfv-native-code\build.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam'
.\bfv-native-code\verify.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam'
.\bfv-native-code\run.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam'
```

Close generated client/server processes before rebuilding; Windows locks running
executable files. Default source rules preserve the tested behavior. Intentional
rule changes can make the baseline comparison fail; add explicit expectations for
the modified behavior rather than treating that failure as original-game parity.

For a private dedicated server:

```powershell
.\bfv-native-code\run-server.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam'
```

The server uses the installation's settings and map list. A previous attempted
`+config` override did not change the observed bind ports and was removed; the
launcher makes no claim that this flag selects an isolated configuration.

See [setup and tooling](docs/SETUP.md) for compiler paths, local binary-analysis
regeneration, and how to run the earlier Bevy experiment.

## Repository contents

```text
bfv-native-code/       Compilable reconstructed AI, editable rules, guarded PE builder,
                       verification fixture, runtime inspection and launch scripts
bevy-remake/          Earlier Rust/Bevy prototype and read-only analysis tooling
  tools/ghidra/       Export/inspection scripts for local Ghidra projects
reports/client/       Analysis indexes and sanitized build/verification/runtime summaries
reports/server/       Dedicated-server equivalents
docs/                 Roadmap, verification details, setup and progress history
```

### Included versus local-only work

Included: project-authored source and scripts, the earlier prototype, reconstruction
notes, RTTI/vtable metadata, function seeds/indexes, direct call graphs, export-status
manifests, inspected input hashes and scoped verification/runtime summaries.

Local only: original executables/DLLs, game assets/archives, generated patched EXEs,
raw decompiler function bodies, assembly dumps containing original code, Ghidra
projects, extracted configurations/assets, profile backups and raw machine logs.
The tools to regenerate analysis from a user-owned installation are included.

This distinction is deliberate: the repository documents all work streams and
preserves their tooling without uploading original game files or presenting the
generated pseudocode corpus as an independently buildable open-source engine.

## Contributing toward the actual goal

Choose a verified native function or subsystem, record the target binary hash and
ABI, recover its inputs/outputs and relevant fields, and implement readable source.
Compare original and compiled behavior with meaningful cases and runtime traces.
Only move a subsystem's status to reconstructed or verified when the evidence
supports that label. A new hook, renamed symbol or successful compilation alone
does not establish full recovery or 1:1 behavior.

The roadmap's endpoint remains the **full original-engine reconstruction**. The
current AI mod build is a useful partial result, not a redefinition of that goal.

## Ownership and distribution

This is an independent, unfinished research/modding project, not an official EA
source release. The original game and its content remain their owners' property.
The repository's license applies to the project-authored files, not to original
game files supplied locally or retained in generated executables. See [NOTICE](NOTICE.md).
