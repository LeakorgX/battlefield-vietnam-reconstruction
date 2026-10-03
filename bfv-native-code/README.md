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

This is a native mod build of the original engine. The AI interpreter compiles
from reconstructed C; the remaining engine is retained as original machine code.
Native numerical helpers, object management and full vehicle candidate evaluation
are still supplied by the original engine. This project does not claim the
decompiler exports have become reconstructed C or that the entire engine is open
source. It provides a working compile-and-run route for editing the listed logic.

## Verification

`verify.ps1` executes both original and newly compiled instructions under Unicorn.
Per target, it compares 1,920 interpreter cases, 32 cached-bailout float32 cases and
32 vehicle-wrapper ABI cases. Context/event methods are controlled test objects;
the vehicle wrapper tests forwarding and returned bits while native scoring remains
intact. Reports are in `build/client/verification.json` and
`build/server/verification.json`. Changes to behavior intentionally make the default
parity comparison fail; add the expected changed-behavior cases when modding.

The compiled client was launched and remained responsive with the replacement
interpreter executing over 491,000 times and the vehicle-rating wrapper over
3,800 times. `startup-smoke.json` records the executable path, loaded patch slots,
compiled bytes and observed invocation counters. This establishes live execution
of the replacements, not whole-game behavioral parity. Test offline or on a
private server you control.

The dedicated-server startup check also verified all three loaded replacements,
5,553 interpreter calls and 48 vehicle-rating wrapper calls in the running server.
That temporary server was stopped after verification. The user confirmed manually
closing the last client test. The runtime reports establish that the compiled code
executed; they do not establish full-match or whole-game behavioral parity. Launch
your desired offline match with `run.ps1` to test your changes.

Toolchain: installed `C:/msys64/mingw32/bin` GCC/binutils and `uv` with `pefile`
and `unicorn`. Outputs require the inspected user-owned original executables and
game files. No originals, decompilation corpus or game assets should be published
with this project.

