# Reconstructed subsystems

Notes on the C routines in `bfv-native-code/src`: recovered interfaces, object
layouts, evaluation order, verification scope and remaining native dependencies.

| Note | Source |
| --- | --- |
| [ARTILLERY](ARTILLERY.md) | `artillery.c`, `artillery_*.c`, target history and tree |
| [BAILOUT](BAILOUT.md) | `native_ai.c`, `numeric_curves.c` |
| [COLLISION](COLLISION.md) | `native_collision.c`, `collision_geometry.c` |
| [EVENTS](EVENTS.md) | `collision_events.c` |
| [VECTORS](VECTORS.md) | `word_vector.c` (insertion, exception bridges) |
| [GEOMETRY](GEOMETRY.md) | `collision_geometry.c`, region and movement geometry |
| [SHARED-MATH](SHARED-MATH.md) | `scalar_vector_math.c`, `vector_normalize.c` |
| [AIM-LIMITS](AIM-LIMITS.md) | `aim_limits.c` |
| [AFFINE-MATRIX](AFFINE-MATRIX.md) | `affine_matrix.c` |
| [CONSTANT-RETURNS](CONSTANT-RETURNS.md) | `constant_returns.c` |
| [WORD-GETTERS](WORD-GETTERS.md) | `word_getters.c` |
| [FIELD-ACCESSORS](FIELD-ACCESSORS.md) | `field_accessors.c` |
| [STATIC-INITIALIZERS](STATIC-INITIALIZERS.md) | `static_initializers.c`, `constructor_calls.c` |
| [DELETING-DESTRUCTORS](DELETING-DESTRUCTORS.md) | `deleting_destructors.c` |
| [DYNAMIC-CASTS](DYNAMIC-CASTS.md) | `dynamic_casts.c` |
| [GLOBAL-STORES](GLOBAL-STORES.md) | `global_stores.c` |
| [CLONES](CLONES.md) | `clones.c` |
