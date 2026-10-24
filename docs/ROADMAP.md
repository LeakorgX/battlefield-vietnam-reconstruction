# Full-engine reconstruction roadmap

The goal is readable source for the original engine and a standalone source build
that preserves original behavior. The static recompilation is the build path: assembly
for original code plus reconstructed C, gated on byte identity.
The [whole-project tracker](PROGRESS.md) counts completed acceptance milestones.
It does not estimate completion dates or remaining effort.

## Static recompilation track

Done: whole-image rebuild from symbolic assembly plus the C payload,
byte-identical for client and server ([STATIC-RECOMP](STATIC-RECOMP.md)).

Done since: references symbolized and data sections labelled (`--symbolic`).
Size-changing edits relink and are gated (`--grow-text`, `--grow-at`,
`--insert`/`--replace`). PE headers and the import/export tables are
assembled from generated source (`headers.s`, `import_source`).

Next:

1. Progressively replace assembly with readable C, subsystem by subsystem,
   using the evidence rules in AGENTS.md. This is what the milestones below measure.

## Milestone 0 — establish the reference

- Identify the exact vanilla client and dedicated-server release.
- Obtain a user-owned, pristine version-matched installation for comparison.
- Separate modified archives/custom maps from the vanilla reference corpus.
- Record hashes and runtime configuration, control floating-point settings and
  establish repeatable offline/private-server scenarios.

Acceptance: someone else with the matching installation can reproduce the same
reference traces and identify every tested binary/data dependency.

## Milestone 1 — recover trustworthy interfaces

- Audit native function boundaries and RTTI/vtable association.
- Recover calling conventions, argument counts and return types from instructions.
- Build explicit object layouts, inheritance adjustments and field ownership.
- Trace indirect calls; preserve aliases instead of trusting one arbitrary label.
- Recover globals, initialization, allocation/destruction and external interfaces.

Acceptance: new code can call and replace the relevant native methods without
guessed stack cleanup, invented layouts or silently missing dependencies.

## Milestone 2 — complete AI subsystem reconstruction

- Broaden interpreter verification to real native callbacks and full lifecycle.
- Reconstruct scheduling and behavior selection, plan implementations, pathfinding,
  strategic state, target selection, weapon use and vehicle control.
- Artillery driver rating routing, its component predicate and cached-target
  validation now compile from C.
  Recover artillery target selection, aiming, trajectory/drop handling, firing
  conditions and entry/exit behavior from actual code and traces.
- The bot Aimer (target tracking, prediction, projectile offset and ballistic
  solve) now compiles from C ([AIMER](modules/AIMER.md)).
- The strategic A* neighbour expansion and the strategic-grid queries it uses
  now compile from C ([ASTAR-STRATEGIC](modules/ASTAR-STRATEGIC.md)), as do the
  search setup and the run that builds its disc mask. The A* searcher's loop,
  heap, node alloc and path output compile from C too
  ([ASTAR-SEARCHER](modules/ASTAR-SEARCHER.md)); the local search problem
  remains native.
- The AI task scheduler's queueing, removal, sleep and slice timing now
  compile from C ([TASK-SCHEDULER](modules/TASK-SCHEDULER.md)), as do the local
  and strategic A* search tasks' `execute`, task unschedule and queue, and the
  player map, strategic search and cell query tasks' `execute`. The run loop and the other task
  `execute` methods remain native.
- Recover bailout table initialization and replace vehicle candidate evaluation
  with validated source. Bailout numerical helpers now compile from reconstructed C.

Acceptance: defined AI scenarios produce equivalent state transitions and decisions
from reconstructed source, including edge cases and sustained runtime operation.

## Milestone 3 — reconstruct gameplay and physics

- Infantry movement and controller integration.
- Weapon/projectile simulation, ammunition, damage and interactions.
- Vehicle engines, steering, aircraft/helicopter control and transport behavior.
- Collision shapes, detection/response, constraints, forces and integration.
- Spawning, capture points, tickets, round state and objective rules.

Acceptance: input/state traces match the reference at known tick rates and floating-
point settings. Test interactions between subsystems, not only isolated methods.

## Milestone 4 — reconstruct multiplayer

- Recover protocol/event IDs and serialization structures.
- Trace state replication, ownership, ordering, reliability and timing.
- Reconstruct client/server tick state transitions, joins, disconnects and round
  transitions. Preserve a documented private-server test matrix.
- Establish whether and where original-client/server compatibility is achieved;
  do not infer it from using the original networking code inside the partial build.

Acceptance: declared compatibility scenarios pass against controlled original
client/server instances and replicated state matches recorded reference traces.

## Milestone 5 — replace engine services and build from source

- Startup, memory management, object registries and global state.
- Rendering/UI/audio and filesystem/resource interfaces.
- Required runtime dependencies and their documented interfaces.
- Recovered headers/types and a coherent full-engine build/link system.
- A source program entry point and source-owned initialization/shutdown.

Acceptance: engine code compiles without copying original executable machine code
into the output. User-provided game assets are a separate dependency.

## Milestone 6 — establish 1:1 claims with evidence

- Validate a pristine version-matched corpus.
- Exercise weapons, movement, physics, AI, vehicles, maps, UI and audio.
- Compare repeatable input/state/render/network traces.
- Publish the test matrix, discrepancies, unresolved cases and observed stability.

Acceptance: each stated compatibility or equivalence claim has reproducible
evidence. Unresolved differences stay visible in documentation.

## Present position

We have partial interface recovery, a compiled AI interpreter, bailout logic/math
and collision callback/notification/dispatcher control flow, editable rating
rules, a build mechanism retaining original engine bytes, controlled comparisons,
and live execution observations. We have not completed any whole-engine milestone.
