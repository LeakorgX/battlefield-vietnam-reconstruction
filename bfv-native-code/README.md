# Editable native Battlefield Vietnam build

Close the generated game before rebuilding. Edit `src/mod_rules.c` or
`src/native_ai.c`, then compile:

```powershell
.\bfv-native-code\build.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam'
.\bfv-native-code\verify.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam'
.\bfv-native-code\run.ps1 -GameDirectory 'D:\Games\Battlefield Vietnam'
```

For the dedicated server, use `run-server.ps1`. It uses the installed game's
server settings in `Mods/BfVietnam/settings/ServerSettings.con` and its map list.

The build writes `BfVietnam-editable.exe` and `bfvietnam_w32ded-editable.exe` beside
your installed game. Both launch with the existing game data. The originals are
read-only inputs; builds check their SHA256 and the original vtable entries.

## Editable logic

- `src/native_ai.c`: the reconstructed AI plan interpreter. You can edit plan
  dispatch, object-type masks, fallback/override selection, context notifications,
  disabled-plan events, output flags and cleanup decisions. It uses the recovered
  native object interfaces and exact x86 `thiscall` conventions.
  Bailout recomputation, cache writes and pattern flags are also implemented here;
  its two numerical helpers are in `src/numeric_curves.c`. See [the recovered interface](../docs/BAILOUT.md).
- `src/native_collision.c`: AI collision-callback control flow, event argument
  assembly, actor decisions, record-counter updates and notification argument
  preparation. The actor dispatcher is also reconstructed. Remaining registry and
  allocation services remain native. `src/collision_geometry.c` implements
  the planar distance calculation and its line-distance helper. See [the recovered interface](../docs/COLLISION.md).
- `src/collision_events.c`: event construction, generation-checked object lookup
  and indexed interface lookup. Vector insertion is also reconstructed; heap and compiler exception
  runtime services remain native. See [event layout and dependencies](../docs/EVENTS.md).
- `src/word_vector.c`: word-vector insertion, overlap-safe copying, filling,
  growth arithmetic and x86 exception registration/catch bridges. See
  [algorithm and runtime limits](../docs/VECTORS.md).
- `src/artillery.c`: artillery driver rating routing, component predicate and
  per-bot cache/eligibility updates and cached-target validation. Candidate search
  remains native. See
  [ARTILLERY.md](../docs/ARTILLERY.md).
- `src/target_history.c`: unsigned target-history map lookup and initialization of
  new timestamps. A guarded native function-entry jump connects both candidate
  passes to this code. Raw allocation remains native.
- `src/history_tree.c`: target-history insertion, duplicate detection, predecessor
  traversal, node construction and red/black balancing. These six functions per
  binary compile from C; protected allocation and string/exception services remain native.
- `src/artillery_weapons.c`: first-pass per-weapon ratings, minimum-distance gating,
  best-weapon selection and score padding. The rest of candidate scoring and firing
  remains incomplete. The default verifier includes 303 comparisons per target.
- `src/mod_rules.c`: plan eligibility, bailout ratings and vehicle-change ratings.
  For example, changing `return native_rating;` in `bfv_bailout_rating` to
  `return native_rating * 2.0f;` changes the rating returned to the actual engine.
  Default rules preserve native behavior.
- `tools/build.py`: verified addresses, section linking and function-table
  replacements for the client and dedicated server. Adding a reconstructed native
  function requires its verified ABI, original slot, replacement symbol and tests.
  Every `.c` file in `src` is compiled. Additional replacements can be registered
  in `extra-patches.json`, with hex `vtable_slot`, hex `expected_function` and a
  `replacement` symbol beginning with `bfv_` for the corresponding target. This
  supports replacing more engine methods as their C implementations are recovered.

This is a native mod build of the original engine. Interpreter, bailout logic/math, artillery driver routing and collision-callback control flow compile
from reconstructed C; the remaining engine is retained as original machine code.
Table initialization, collision detection/response, object lifecycle and full vehicle candidate evaluation
are still supplied by the original engine. This project does not claim the
decompiler exports have become reconstructed C or that the entire engine is open
source. It provides a working compile-and-run route for editing the listed logic.

## Verification

`verify.ps1` executes both original and newly compiled instructions under Unicorn.
Per target, it compares 1,920 interpreter cases, 32 cached-bailout float32 cases,
554 bailout recomputation cases (including cache/flag pointer changes during callbacks),
3,840 numerical-helper comparisons of the full 80-bit x87 return,
129 collision-callback, 64 notification-helper and 148 actor-dispatcher comparisons,
2,136 geometry comparisons, 251 event comparisons, 507 vector/exception-bridge comparisons, and
1,536 artillery driver comparisons, 708 artillery evaluator/integration checks,
432 target-history lookup comparisons, 4,678 history-tree comparisons and 32 vehicle-wrapper ABI cases. Context/event methods are controlled test objects;
the vehicle wrapper tests forwarding and returned bits while native scoring remains
intact. Reports are in `build/client/verification.json` and
`build/server/verification.json`. Changes to behavior intentionally make the default
parity comparison fail; add the expected changed-behavior cases when modding.

For focused numerical tests, run `tools/verify_curves.py` through `uv` with
`pefile` and `unicorn`, passing `--game-dir` and optionally `--target client` or
`--target server`. The full `verify.ps1` includes these comparisons.

An earlier compiled client was launched and remained responsive with the replacement
interpreter executing over 491,000 times and the vehicle-rating wrapper over
3,800 times. `startup-smoke.json` records the executable path, loaded patch slots,
compiled bytes and observed invocation counters. This establishes live execution
of the replacements, not whole-game behavioral parity. Test offline or on a
private server you control.
Those live observations predate the C bailout recomputation; the new routine has
controlled instruction-comparison evidence, but no retained live-match sample yet.

The dedicated-server startup check also verified all three loaded replacements,
5,553 interpreter calls and 48 vehicle-rating wrapper calls in the running server.
Those older live samples cover three loaded replacements; the new build adds
the collision callback as a fourth replacement, without a retained live sample yet.
That temporary server was stopped after verification. The user confirmed manually
closing the last client test. The runtime reports establish that the compiled code
executed; they do not establish full-match or whole-game behavioral parity. Launch
your desired offline match with `run.ps1` to test your changes.

Toolchain: installed `C:/msys64/mingw32/bin` GCC/binutils and `uv` with `pefile`
and `unicorn`. Outputs require the inspected user-owned original executables and
game files. No originals, decompilation corpus or game assets should be published
with this project.


For one binary only, add `-Target client` or `-Target server` to `verify.ps1`.
Focused event checks use `tools/verify_events.py` through `uv` with `pefile` and
`unicorn`, passing `--game-dir`. The complete verification includes these checks.

The opt-in `tools/verify_live_exceptions.py` check runs original/reconstructed vector
cases inside temporary Windows client/server processes. It passed 16 live executions
with actual C++ exception dispatch, cleanup/rethrow and restored FS linkage. See
[the probe and its limits](../docs/VECTORS.md#live-windows-exception-propagation).
