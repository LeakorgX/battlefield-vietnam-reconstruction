# Modding with the lifted C

The game code is in `bfv-native-code/lifted/client/` and `.../server/`. It's sorted
into the developers' own folders and files: `bf/ai/AIMain.c`, `ref2/world/BFSoldier.c`
and so on.

## Find the code

- **By file and class:** names come from the game's own class names (`dice::bf::ai::BotMain`)
  and source paths. Folders: `bf/ai` (bots), `bf` (game rules), `ref2/world` (objects,
  soldiers, vehicles, weapons), `ref2/geom`, `ref2/rend` (graphics), `ref2/io`,
  `mills` (sound), `network`, `meme` and `bf/menu` (UI). `unsorted/` holds what
  couldn't be placed yet.
- **By text:** the comment above every function lists the strings it uses. Search
  for a console command, setting or message:

  ```bash
  grep -rn '"beginRepair"' bfv-native-code/lifted/client
  ```

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
- Don't rerun `lift_c.py` once you've edited: it regenerates every file and
  overwrites your changes. Use git to keep your mods.
- Your original game files are never changed; the build writes `*-editable.exe` copies.
