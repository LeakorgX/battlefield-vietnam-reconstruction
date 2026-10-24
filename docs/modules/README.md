# Reconstructed subsystems

Notes on the C routines in `bfv-native-code/src`: recovered interfaces, object
layouts, evaluation order, verification scope and remaining native dependencies.

| Note | Source |
| --- | --- |
| [ARTILLERY](ARTILLERY.md) | `artillery.c`, `artillery_*.c`, target history and tree |
| [BAILOUT](BAILOUT.md) | `native_ai.c`, `numeric_curves.c` |
| [AI](AI.md) | Bot decision loop, weights, curves, skill, SAI data and modding recipes |
| [COLLISION](COLLISION.md) | `native_collision.c`, `collision_geometry.c` |
| [EVENTS](EVENTS.md) | `collision_events.c` |
| [VECTORS](VECTORS.md) | `word_vector.c` (insertion, exception bridges) |
| [GEOMETRY](GEOMETRY.md) | `collision_geometry.c`, region and movement geometry |
| [SHARED-MATH](SHARED-MATH.md) | `scalar_vector_math.c`, `vector_normalize.c` |
| [AIM-LIMITS](AIM-LIMITS.md) | `aim_limits.c` |
| [AIMER](AIMER.md) | `aimer.c` (bot target tracking and ballistic solve) |
| [ASTAR-STRATEGIC](ASTAR-STRATEGIC.md) | `astar_strategic.c` (strategic A* setup, disc-mask run, neighbour expansion and grid queries) |
| [ASTAR-LOCAL](ASTAR-LOCAL.md) | `astar_local.c` (local A* estimate, goal test, start and goal end points) |
| [ASTAR-SEARCHER](ASTAR-SEARCHER.md) | `astar_searcher.c` (the A* searcher's time-sliced step: open heap, open and closed hashes, relaxation; heap push, update, pop, sift-down and closed-list removal; reset and node alloc; path, ring and closed-list output) |
| [TASK-SCHEDULER](TASK-SCHEDULER.md) | `task_scheduler.c` (AI task tree: sleep, add, remove, slice time left; A* search task execute; queue, player map, strategic search and cell query tasks) |
| [AFFINE-MATRIX](AFFINE-MATRIX.md) | `affine_matrix.c` |
| [CONSTANT-RETURNS](CONSTANT-RETURNS.md) | `constant_returns.c` |
| [WORD-GETTERS](WORD-GETTERS.md) | `word_getters.c` |
| [FIELD-ACCESSORS](FIELD-ACCESSORS.md) | `field_accessors.c` |
| [STATIC-INITIALIZERS](STATIC-INITIALIZERS.md) | `static_initializers.c`, `constructor_calls.c` |
| [DELETING-DESTRUCTORS](DELETING-DESTRUCTORS.md) | `deleting_destructors.c` |
| [DYNAMIC-CASTS](DYNAMIC-CASTS.md) | `dynamic_casts.c` |
| [GLOBAL-STORES](GLOBAL-STORES.md) | `global_stores.c` |
| [CLONES](CLONES.md) | `clones.c` |
