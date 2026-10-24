# Modding

There are two ways to mod the game, and both rebuild the executables with
`bfv-native-code/build.ps1`:

1. **Mods (start here).** Your own C files go in `bfv-native-code/mods/`. A
   mod hooks a game function: your code runs before it, after it or instead of
   it. Mods can do float math, keep variables between calls and call game
   functions. They are kept apart from the generated code, so regenerating the
   C never touches them.
2. **Editing the readable C.** You change a game function's own code in
   `bfv-native-code/readable/<target>/`. This suits small changes inside a
   function, such as a constant or a condition.

Your game files are never changed. The build writes `BfVietnam-editable.exe`
and `bfvietnam_w32ded-editable.exe` into the game folder, and `run.ps1` /
`run-server.ps1` start them.

## Quick start: a mod

`mods/shared/example_repair_factor.c` makes repair packs repair twice as fast:

```c
#include "bfv_mod.h"

MOD_AFTER(Cmd_ObjectTemplate_repairFactor_execute, double_repair_factor)
{
    if (M32(MOD_THIS + 0x1c) != 1) return;  /* 0 = the command reads the value */
    uint32_t template = GAME_THISCALL(getActiveTemplateAs, M32(HandFireArmsTemplate_classID));
    float factor = F32(template + ObjectTemplate_repairFactor) * 2.0f;
    F32(template + ObjectTemplate_repairFactor) = factor;
    MOD_LOGF("repairFactor set to %f\n", factor);
}
```

The game's `Objects.con` runs `ObjectTemplate.repairFactor 0.5`, which calls the
handler `Cmd_ObjectTemplate_repairFactor::execute`. The hook runs after the
handler has stored the value and doubles it.

Build and run it:

```powershell
.\bfv-native-code\build.ps1            # both executables; -Target client for one
.\bfv-native-code\run.ps1
```

The build prints `client: mods hook 1 function(s): 00626890 ...`. `MOD_LOGF`
writes to the Windows debug output, which [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview)
shows.

### Where mod files go

| Folder | Built into |
| --- | --- |
| `mods/client/*.c` | the client only |
| `mods/server/*.c` | the dedicated server only |
| `mods/shared/*.c` | both |

Function, global and field names have each target's own address or offset
(`ObjectTemplate_repairFactor` is `+0x618` on the client and `+0x5ec` on the
server), so a file in `shared/` works for both. Addresses (`0x00626890`) belong
to one target, so a mod that uses them goes in that target's folder. To build
without any mods, move the files out of `mods/`.

### Write a hook

`find.py --hook` prints the function's calling convention, whether it can be
hooked, and a starting skeleton:

```bash
python bfv-native-code/tools/find.py 00626890 --hook
```

```
00626890  Cmd_ObjectTemplate_repairFactor::execute  (thiscall: 0 stack arguments, reads ecx)
hookable: yes
```

A hook is `MOD_BEFORE(function, name)` or `MOD_AFTER(function, name)` followed
by a body. `function` can be:

- an address: `0x00626890`;
- a name from `readable/runtime/symbols_<target>.h`: `getActiveTemplateAs`;
- an `INDEX.tsv` name with `::` written as `_`:
  `Cmd_ObjectTemplate_repairFactor_execute`.

If a name matches more than one function, the build says so; use the address
instead. `name` is your C function name and must be unique.

Inside a hook:

| Use | Meaning |
| --- | --- |
| `MOD_THIS` | `this` (ecx at entry) |
| `MOD_ARG(i)`, `MOD_ARG_F(i)` | stack argument `i` (0 = first) as a 32-bit value or a float; assign to change it in a before-hook |
| `MOD_RESULT`, `MOD_SET_RESULT(v)` | the integer/pointer result (eax), in an after-hook |
| `MOD_RESULT_F`, `MOD_SET_RESULT_F(v)` | the float result (x87 ST0), in an after-hook |
| `MOD_RETURN(v)`, `MOD_RETURN_F(v)` | in a before-hook: skip the function and return `v` |
| `M8` / `M16` / `M32(addr)`, `F32(addr)` | read or write game memory (integer or float) |
| `GAME_THISCALL(f, obj, args...)` | call a game method; `GAME_STDCALL`, `GAME_CDECL`, `GAME_FASTCALL(f, ecx, edx, args...)` for the other conventions |
| `GAME_THISCALL_F(...)` etc. | the same for a function that returns a float |
| `GAME_FLOAT(x)` | pass a float as an argument |
| `GAME_VIRTUAL(obj, n)` | the address of virtual method `n` of `obj` |
| `GAME_METHOD(obj, Class, name)` | the same by name: slot `VF_Class_name` (`GAME_METHOD(obj, AISettings, getViewDistance)`) |
| `MOD_LOG(text)`, `MOD_LOGF(format, ...)` | debug output (`%d %u %x %s %f`) |

Hooks are ordinary C. Global variables keep their value between calls, and you
can write helper functions. `mod_sqrtf`, `mod_fabsf`, `mod_minf`, `mod_maxf` and
`mod_clampf` cover the common math, because there is no C library.

Rules:

- Only functions that the build runs as C can be hooked (`hookable: yes`).
  There are about 33,500 such functions on the client and 20,000 on the
  server. The build stops with an error if you hook another one.
- `MOD_RETURN` needs the function's stack cleanup, which the build reads from
  its `ret` instructions. A function that pops different amounts on different
  paths can't be skipped. The mod stops with a debug message and a breakpoint.
- An after-hook runs only when the function returns to its caller. It doesn't
  run when the function hands its tail to other code with a jump.
- Mods are not checked against the original game, so test them in play.
  Everything else in the build stays byte-checked.

## Find the code

- **Search tool:** looks up a string, class, file name or address and prints
  `file:line` in the readable C (`--tree lifted` for the L1 C):

  ```bash
  python bfv-native-code/tools/find.py repairFactor
  python bfv-native-code/tools/find.py BotMain --target server
  python bfv-native-code/tools/find.py 00626890 --calls     # what it calls
  python bfv-native-code/tools/find.py 00674010 --callers   # who calls it (slow: reads every file)
  python bfv-native-code/tools/find.py repairFactor --uses  # every instruction that uses it (slow)
  python bfv-native-code/tools/find.py FireArms --class     # its virtual methods (slots) and named fields
  ```

- **Names in mods:** the build writes `build/<target>/mod_fields.h` (field
  offsets the readable C and `names.txt` name, e.g. `AISettings_maxNBots`) and
  `mod_slots.h` (virtual method slots, e.g. `VF_AISettings_getViewDistance 9`,
  for `GAME_METHOD`). Only names with one value in the target are kept;
  overloads are left out, so use `GAME_VIRTUAL(obj, n)` with the slot number
  `find.py Class --class` shows for those.

- **Settings → code (start here):** `lifted/<target>/SETTINGS.md` lists every
  script setting with a known field (`ObjectTemplate.fireDelay`, `magSize`,
  `reloadTime`, `Game.gamePlayMode`, ...). Under each one are the functions
  that read (r) or write (w) it. Weapon, soldier and vehicle code reaches its
  settings through `this->template` (`[this + 0x4c]`) or a copy the constructor
  makes (`FireArms + 0x290` = `fireDelay`), and both are followed. To change how
  a setting behaves, hook or edit a reader listed there. The links point into
  `lifted/`; the same file and function exist in `readable/`, and
  `find.py <address>` gives its readable line.
- **Console commands:** `lifted/<target>/CONSOLE.tsv` lists every console and
  script command (client 2,251, server 1,679): name, argument types, file, the
  handler functions that run it and the game functions they call. Commands like
  `ObjectTemplate.repairFactor` in the game's `.con` files appear there as
  `repairFactor`. A handler's `execute` is a good hook: `[this+0x1c]` is 0 =
  get, 1 = set; `[this+0x20]` the arguments; `[this+0xa4]` the result.
- **By folder:** `bf/ai` (bots), `bf` (game rules), `ref2/world` (objects,
  soldiers, vehicles, weapons), `ref2/geom`, `ref2/rend` (graphics), `ref2/io`,
  `mills` (sound), `network`, `meme` and `bf/menu` (UI). `unsorted/` holds what
  couldn't be placed yet. Files named `console_<id>.c` hold the console commands
  of one source file.
- **Index:** `lifted/<target>/INDEX.tsv` lists every function with its address,
  file, class, evidence and strings. `FIELDS.tsv` lists every named field and
  virtual method with its object and offset. `CLASSES.tsv` lists every class
  with its vtable, number of virtual methods and base classes, and
  `VTABLES.tsv` the function in each slot of those vtables.
- **Names:** `bfv-native-code/notes/<target>/names.txt` names virtual slots and
  fields (`IAISettings::vf1 = getMaxNBots`, `BotBehaviour+0x1c = weights`);
  `names-console.txt` next to it is generated from the console commands and
  `names-code.txt` from the machine code (`tools/code_names.py`). Add a
  line when you work out what a slot or field is, then rerun `comment_l2.py`:
  every use in the tree shows the name.

## Read a function

The readable C (`readable/<target>/`) is the same code as the lifted C, written
with variables, `if`/`while` and calls by name:

```c
/* thiscall: 0 stack arguments, reads ecx */
void bfv_client_l1_body_00626890(struct bfv_cpu *c)
{
#define ObjectTemplate_repairFactor 0x618  /* field offset */
    ...
    this_1c = M32(this_2 + 0x1c);
    if (this_1c == 0) {
        v4 = M32(HandFireArmsTemplate_classID);
        CALL(1, getActiveTemplateAs);
        ...
```

- `M8`/`M16`/`M32(addr)` read or write game memory. `CALL(k, f)` calls a game
  function with the current registers and stack.
- `eax`, `ecx`, ... are the CPU registers. The `#define`s at the top name them
  where they hold one value (`this`, `result`, `r_<function>`).
- The comment above the body gives the calling convention, which a hook uses.

Comments starting with `/**` explain the code. Each file starts with a table of
its functions. Each function header lists what it calls and which `this+`
fields it reads and writes. Float math, which the C does as x87 instructions,
is written out above it as a formula (`/** speed = sqrt(x * x + y * y) */`),
and float compares as `/** when a <= b */`. Write your own notes about what a
function is for in `bfv-native-code/notes/<target>/<path>.txt`, under
`## <address>`, then run
`uv run --with pefile python bfv-native-code/tools/comment_l2.py`. Notes outside
the generated tree survive regeneration. See
[READABLE-C.md](READABLE-C.md#comments-toolscomment_l2py).

The lifted C (`lifted/<target>/`) has one commented C block per original
instruction. Its notes after `;` name fields, globals, virtual call targets,
strings and float constants. `find.py <text> --uses` searches those notes.

## Edit the readable C

1. Edit the function in `readable/<target>/`. You can change a constant, flip
   a condition or skip a call.
2. Rebuild with `.\bfv-native-code\build.ps1`. The build lists every edited
   function.

Rules:

- Keep the register and stack state consistent at the end of the function
  (`esp` and the return through `c->eip`). The rest of the game still expects
  the original calling convention.
- The generated C is compiled with `-mgeneral-regs-only`, so it can't do float
  math. Write float values as bit patterns (1.0f = `0x3f800000`), or use a mod.
- Only functions that passed verification run as C. If the build says "never
  verified: stays native", your edit has no effect.
- `lift_l2.py` and `lift_c.py` refuse to regenerate a target while any of its
  functions is edited. `--force` overwrites your edits, so keep a copy first.

## Included mod: AI Commander

`mods/shared/ai_commander.c` builds into both executables. It runs only on the
machine that hosts the battle: the dedicated server, or a client hosting a
game. Players who join need no mod.

- **Frontline.** A team can capture a flag only if it holds a flag next to it.
  Each flag is linked to its two nearest flags. A team that holds no flag can
  capture any flag. Flags whose map sets `onlyAffectedByTeam` keep the map's
  rule.
- **Commander.** Every 20 seconds each team gets an attack order for the
  nearest flag it can capture. When a team loses a flag, it gets a
  counterattack order for that flag for 60 seconds. Captures and flags under
  attack are announced. The messages go out through `Game::sayAll`: on a
  client that hosts, they appear as the host's chat.
- **Bots follow the front.** The basic temperature of each AI strategic area
  near a flag is scaled from the map's own value: objectives and flags under
  attack ×3, other front flags ×2, flags behind the front ×0.6.
- **Campaign.** The game process counts battles won, by flags held at the end
  of each round, and announces the score when the next round starts. Restarting
  the server or game resets it.

Not included: fire support, and changes to tickets or spawn waves.

Build only this mod into otherwise original game code (about the size of the
original EXE; only the two hooked functions run as C):

```bash
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/build.py --tree readable --mods-only --mods ai_commander --target client --output "X:/Battlefield Vietnam - Copy/BfVietnam-AICommander.exe"
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/build.py --tree readable --mods-only --mods ai_commander --target server --output "X:/Battlefield Vietnam - Copy/bfvietnam_w32ded-AICommander.exe"
```

`--mods-only` detours only the hooked functions (the hand-written C in `src/`
stays). `--mods a,b` picks mod files by name. The full readable build
(`--tree readable` alone) runs about 33,600 functions as C and is 59 MB; on
2026-10-23 it crashed at startup in `ctype<char>::ctype` (00405470), so play
with the `--mods-only` build.

## Included mod: RPG-7 HD

`mods/client/rpg7_hd.c` adds a high-detail RPG-7V and PG-7VR with a PBR
shader, MSAA, iron sights, a visible heavy rocket in flight and bigger warhead
effects. It loads them from `_002` override archives. [RPG7-HD.md](RPG7-HD.md)
describes the archives, the script-override mechanism, the asset pipeline and
the weapon data.

## Included mod: HD sky

`mods/client/hd_sky.c` replaces every map's sky with one 2048x2048-per-face
sky baked from a CC0 Poly Haven HDRI, turned to each map's sun, and sets the
fog to its horizon colour. Its data is in the `rpg7_hd` override archives, so
build both. [HD-SKY.md](HD-SKY.md) describes the bake and the hooks.

## Included mod: Aggressive AI

`mods/shared/ai_aggressive.c` builds into both executables. It changes the
values the retail AI scripts pass to the AI console commands, before each
command runs; no script is edited.

- Soldier behaviour weights (`StandardWeights`, `FireunarmedWeights`): Fire
  x1.35, MoveTo x1.5, Scout x1.2, TakeCover x0.5, Change x2.2. Change is the
  urgency to enter a better vehicle or seat (`BBChange`, 009856f0), so bots
  take tanks, APCs and helicopters much more often. Idle and Avoid are kept
  (an Idle urgency of 0 crashes the AI).
- Strategies: `aggression` is forced to 1, `numberOfAttacks` +1,
  `numberOfDefences` -1 when above 1, and `setStrategicObjectsModifier` x1.5 for
  hostile and neutral objects.
- `AIStrategicArea.vehicleSearchRadius` is doubled where it is positive.

It works with AI Commander, which steers bots to the front. Bot skill stays the
menu's setting.

## Included mod: Cinematic finish

The owner rejected this colour-only experiment. Use the
[world renderer upgrade](WORLD-RENDER.md) below for the current work.

`mods/client/cinematic.c` adds a compiled D3D9 colour-grading pass after the
world and before HUD icons/menus: cooler shadows, restrained saturation,
contrast and a light vignette. Build with `--mods-only` and include
`ai_commander,ai_aggressive,rpg7_hd,hd_sky,cinematic`. The current output is
`BfVietnam-Cinematic.exe`. [CINEMATIC.md](CINEMATIC.md) covers the shader,
research, rebuild commands and verification. GPU tests pass; gameplay and
visual tuning remain untested.

## Included mod: Fast start

`mods/client/fast_start.c` skips the four startup movies, so the game opens
on the main menu. `game/Startup.con` plays them with `renderer.playBink`;
the hook on that command's execute (`0047f760`) returns early for files
under `Movies/`. The menu's Play Intro entry and background movies still
work. The loading screen redraws every 0.5 s instead of every 0.1 s; each
redraw waits for vertical sync. Each load's time goes to `fast_start.log`
in the game folder. No game file is changed. Included in
`BfVietnam-WorldRender.exe`.

## Included mod: World renderer

`mods/client/world_render.c` and `mods/shaders/world/` replace ten engine
effects with per-pixel mesh/character lighting, gloss-dependent highlights,
prefiltered sky reflections, terrain detail relief and Fresnel water.
`BfVietnam-WorldRender.exe` includes the RPG, sky and AI mods, excluding the
colour-grade `cinematic` mod. It mounts the new authored `effects_003.rfa`.
F8 compares the new and retail-style lighting models. [WORLD-RENDER.md](WORLD-RENDER.md)
documents scope, evidence, packaging, build commands and validation limits.

## Build options

```powershell
.\bfv-native-code\build.ps1 -Target client                      # one executable
.\bfv-native-code\build.ps1 -Tree lifted                        # build from the L1 C instead (no mods)
.\bfv-native-code\build.ps1 -Target client -Output X:\test.exe  # somewhere else
```

The first build compiles all of the generated C. Later builds compile only the
files that changed (the objects are cached in `bfv-native-code/build/<target>/`).

## Test the mod runtime

`tools/test_mods.py` builds a test client with the hooks in
`mods/tests/client/`, writes it to `build/client-custom/`, and runs the hooked
functions in an emulator. It checks float results, argument changes, skipping a
function, game calls from a hook, logging, and that the game's registers and
x87 state survive:

```bash
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/test_mods.py
```
