# Modding with the lifted C

The game code is in `bfv-native-code/lifted/client/` and `.../server/`. It's sorted
into the developers' own folders and files: `bf/ai/AIMain.c`, `ref2/world/BFSoldier.c`
and so on.

## Find the code

- **Search tool:** looks up a string, class, file name or address and prints
  `file:line`:

  ```bash
  python bfv-native-code/tools/find.py repairFactor
  python bfv-native-code/tools/find.py BotMain --target server
  python bfv-native-code/tools/find.py 00626890 --calls     # what it calls
  python bfv-native-code/tools/find.py 00674010 --callers   # who calls it
  ```

- **Console commands:** `lifted/<target>/CONSOLE.tsv` lists every console and
  script command (client 2,251, server 1,679): name, argument types, file, the
  handler functions that run it and the game functions they call. Commands like
  `ObjectTemplate.repairFactor` in the game's `.con` files appear there as
  `repairFactor`.
- **By folder:** `bf/ai` (bots), `bf` (game rules), `ref2/world` (objects,
  soldiers, vehicles, weapons), `ref2/geom`, `ref2/rend` (graphics), `ref2/io`,
  `mills` (sound), `network`, `meme` and `bf/menu` (UI). `unsorted/` holds what
  couldn't be placed yet. Files named `console_<id>.c` hold the console commands
  of one source file.
- **Index:** `lifted/<target>/INDEX.tsv` lists every function with its address,
  file, class, evidence and strings.

## Read a function

Each original instruction is a commented C block:

```c
/* 009b96d2  mov dword ptr [eax], 0xbf75e4 */
ea = (uint32_t)(eax);
M32(ea) = 0xbf75e4u;
```

- `eax`, `ecx`, ... are the CPU registers.
- `M8`/`M16`/`M32(addr)` read or write game memory.
- `cf`, `zf`, ... are the CPU flags.
- Jumps are `goto L_<address>`. A call leaves the function and comes back at `R_<k>`.
- For a method, `ecx` holds `this` at entry.

## Worked example: what `repairFactor` sets

```bash
grep -P '^repairFactor\t' bfv-native-code/lifted/client/CONSOLE.tsv
```

The handlers are `00626890` and `006268f0` in `ref2/world/console_08f57028_2.c`.
`00626890` (comments only):

```c
/* 006268a2  mov ecx, dword ptr [0xb77ee8] */   // a global (probably the template manager)
/* 006268a8  call 0x674010 */                    // probably returns the active template
/* 006268e4  mov dword ptr [eax + 0x618], ecx */ // stores the value at +0x618
```

The scripts set it with `ObjectTemplate.repairFactor 0.15`
(`objects/HandWeapons/RepairPack/Objects.con`), so `repairFactor` is most likely
the float at offset `0x618` of the active object template. Any
function that reads `+ 0x618` from a template uses it. To force every template's
value to 1.0, change the C lines under `006268e4`:

```c
    /* 006268e4  mov dword ptr [eax + 0x618], ecx */
    ea = (uint32_t)(eax + 0x00000618u);
    M32(ea) = 0x3f800000u;  /* mod: repairFactor = 1.0f (was: ecx) */
```

Lifted files are compiled with `-mgeneral-regs-only`, so C code there can't do
float math (the game's own x87 state must stay untouched). Write float values as
their bit patterns (1.0f = `0x3f800000`, 2.0f = `0x40000000`, 0.5f = `0x3f000000`).

## Change it

1. Edit the function. You can change a constant, flip a condition, skip a call
   with a `goto`, or add C code.
2. Rebuild: `.\bfv-native-code\build.ps1`. The build lists every edited function.
3. Run: `.\bfv-native-code\run.ps1` (or `run-server.ps1`).

Rules:

- Keep the register and stack state consistent at the end of the function
  (`esp` and the return through `c->eip`). The rest of the game still expects the
  original calling convention.
- Only functions that passed verification run as C. If the build says
  "never verified: stays native", your edit has no effect.
- `lift_c.py` refuses to regenerate a target while any function is edited.
  `--force` overwrites your edits, so keep your mods in git.
- Your original game files are never changed; the build writes `*-editable.exe` copies.
