# bfv-native-code

The static recompiler, the reconstructed C, and the build, run and verify
scripts. Commands assume the repository root and `$env:BFV_GAME_DIR` pointing at
your game installation. Close the game before rebuilding.

## Scripts

| Script | What it does |
| --- | --- |
| `build.ps1` | Compiles `src/*.c` into the payload and writes `BfVietnam-editable.exe` / `bfvietnam_w32ded-editable.exe` into the game folder (`tools/build.py`) |
| `recomp.ps1` | Static recompilation from assembly source, with a byte-identity check (`tools/static_recomp.py`). `-AssembleOnly` keeps hand edits, `-Install` writes the result to the game folder |
| `verify.ps1` | Runs original and reconstructed instructions side by side under Unicorn (`tools/verify.py`) |
| `run.ps1` / `run-server.ps1` | Launch the editable client / dedicated server |

All scripts take `-Target client|server|both` (`run*.ps1` excepted) and
`-GameDirectory`. See [docs/STATIC-RECOMP.md](../docs/STATIC-RECOMP.md) for
the recompiler and the edit workflow.

## Reconstructed C (`src/`)

Every `.c` file in `src/` is compiled into the payload. Original functions
detour to these routines; see `build/<target>/manifest.json` for the exact
addresses.

| Area | Files | Notes |
| --- | --- | --- |
| AI plan interpreter | `native_ai.c`, `numeric_curves.c` | Interpreter, bailout recomputation and numeric helpers ([bailout](../docs/modules/BAILOUT.md)) |
| Editable rules | `mod_rules.c` | Plan eligibility, bailout and vehicle ratings. Defaults preserve native behavior |
| Collision | `native_collision.c`, `collision_geometry.c` | Callback control flow, actor dispatcher, planar distance ([collision](../docs/modules/COLLISION.md)) |
| Events and vectors | `collision_events.c`, `word_vector.c` | Event construction, lookups, vector insertion, exception bridges ([events](../docs/modules/EVENTS.md), [vectors](../docs/modules/VECTORS.md)) |
| Target history | `target_history.c`, `history_tree.c`, `list_search.c`, `target_traversal.c` | History map lookup, red/black tree insertion and balancing |
| Artillery evaluator | `artillery.c`, `artillery_*.c` | Driver rating, entry/cache gateway, first and second candidate passes, scoring and final selection ([artillery](../docs/modules/ARTILLERY.md)) |
| Geometry and math | `aim_geometry.c`, `aim_limits.c`, `affine_matrix.c`, `movement_geometry.c`, `region_geometry.c`, `scalar_vector_math.c`, `vector_normalize.c` | Position, aiming-limit, matrix and vector helpers ([math](../docs/modules/SHARED-MATH.md), [aim limits](../docs/modules/AIM-LIMITS.md), [matrix](../docs/modules/AFFINE-MATRIX.md), [geometry](../docs/modules/GEOMETRY.md)) |
| Catalogues | `constant_returns.c`, `word_getters.c` | 954 constant-return stubs and 175 field getters, generated from `constant-returns.json` / `word-getters.json` ([constants](../docs/modules/CONSTANT-RETURNS.md), [getters](../docs/modules/WORD-GETTERS.md)) |
| Catalogues | `field_accessors.c`, `static_initializers.c` | 889 field/global accessors and 3,947 C++ static-object initializers, generated from `field-accessors.json` / `static-initializers.json` ([accessors](../docs/modules/FIELD-ACCESSORS.md), [initializers](../docs/modules/STATIC-INITIALIZERS.md)) |
| Catalogues | `constructor_calls.c`, `deleting_destructors.c`, `dynamic_casts.c`, `clones.c` | 460 initializers with constructor arguments, 691 scalar deleting destructors, 544 `dynamic_cast` wrappers and 357 virtual clones, generated from the matching `.json` catalogues ([initializers](../docs/modules/STATIC-INITIALIZERS.md), [destructors](../docs/modules/DELETING-DESTRUCTORS.md), [casts](../docs/modules/DYNAMIC-CASTS.md), [clones](../docs/modules/CLONES.md)) |

Example rule change: in `mod_rules.c`, changing `return native_rating;` in
`bfv_bailout_rating` to `return native_rating * 2.0f;` doubles the rating the
engine receives. Behavior changes intentionally make the parity verifier fail,
so add expected-behavior cases for them.

To register an extra vtable replacement, add an entry to `extra-patches.json`
with hex `vtable_slot`, hex `expected_function` and a `replacement` symbol
starting with `bfv_`.

## Tools (`tools/`)

| File | Purpose |
| --- | --- |
| `build.py` | Payload compile/link, entry detours, vtable patches, manifests |
| `static_recomp.py` | Static recompiler |
| `verify.py` | Full regression: runs every focused verifier |
| `verify_*.py` | Focused original-vs-C comparisons, one per reconstructed stage |
| `verify_live_exceptions.py` | Opt-in live check inside temporary Windows client/server processes |
| `*_oracle.py` | Reference models used by the verifiers |
| `export_reports.py` | Exports compact verification reports to `../reports` (requires current full and live results) |
| `smoke.py`, `native-probe/` | Startup smoke test and live probe helpers |

Toolchain: MinGW GCC/binutils at `C:\msys64\mingw32\bin` and `uv`, which
supplies `pefile`, `capstone` and `unicorn`. Outputs need your own matching
original EXEs. Never publish originals, assets or `build/`.
