# Cinematic renderer finish

2026-10-24. `mods/client/cinematic.c` adds an engine-integrated Direct3D 9
pixel shader to the current RPG/sky mod. Built client:
`X:\Battlefield Vietnam - Copy\BfVietnam-Cinematic.exe`.

The first look is restrained: saturation 0.82, stronger midtone contrast,
cool steel-blue shadows, slightly warm highlights, highlight neutralisation,
and a 5.5% corner vignette. The source is `mods/shaders/cinematic.hlsl`;
`cinematic_shader.h` contains only this authored shader's compiled ps_2_0
bytecode. There is no proxy DLL, ReShade, runtime compiler or new archive.
The RPG and HD sky still use their existing `_002` archives.

This grades the existing LDR world. It does not make the retail terrain,
vehicles or soldiers physically based, add missing material maps, or recover
HDR highlights that the renderer has already clipped. The RPG keeps its own
PBR shader. Battlefield 1 is a visual direction, not a claim of renderer parity.

## Engine evidence and integration

In the supported original client (`79655e9c...17a06a5`), frame routine
`004a8af0` calls `renderList_end` (`00a401b0`) at `004a8c9d`, after the world
and first-person drawing path. It then calls `004a04d0` (HUD icons) and
`00490e10` (menu drawing). See the lifted/readable
`client/unsorted/unsorted_0048e0f0.c`. The after-hook grades only when its
native return address is `004a8ca2` and the engine's scene flag is set.
Other uses of renderList_end, including shadow passes, are ignored.
Menus/loading paths that skip this world branch are also left alone.

Use **--mods-only**. The enclosing frame routine must remain native; a full
readable build changes its return address and is already known to crash at
startup independently of this mod (MODDING.md).

The pass checks that the target is the back buffer, captures D3DSBT_ALL,
ends the scene, resolves/copies to a same-size non-MSAA render-target texture,
reopens the scene and draws a fullscreen XYZRHW quad. Pixel centres use the
D3D9 half-pixel offset. It reapplies and releases the state block immediately,
keeping actual device state consistent with the engine's state cache. It
does not rebind or replace the depth surface. Resources are released before
the same reset/release/create routines the RPG MSAA mod hooks, and recreated
when the device, dimensions or format change. Errors append to `cinematic.log`.

## Research

- [Moving Frostbite to Physically Based Rendering, by its authors](https://seblagarde.wordpress.com/2015/07/14/siggraph-2014-moving-frostbite-to-physically-based-rendering/):
  material response, lighting and the camera/image pipeline are distinct
  parts of a cinematic renderer. This implementation addresses the image
  finish; the existing RPG material addresses one asset's lighting response.
- [Microsoft: StretchRect](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-stretchrect):
  resolving a multisampled render target into a non-multisampled surface.
- [Microsoft: state blocks](https://learn.microsoft.com/en-us/windows/win32/direct3d9/state-blocks-save-and-restore-state):
  capture and reapply device state around the extra pass.
- [Microsoft: lost devices](https://learn.microsoft.com/en-us/windows/win32/direct3d9/lost-devices):
  resource lifetime and reset handling.

## Build and verify

From the repository root, with MinGW32 on PATH:

```powershell
$env:PATH = 'C:/msys64/mingw32/bin;' + $env:PATH
# Recompile the embedded shader after editing the HLSL:
gcc -O2 bfv-native-code/tools/compile_cinematic_shader.c -o bfv-native-code/build/compile_cinematic_shader.exe
./bfv-native-code/build/compile_cinematic_shader.exe bfv-native-code/mods/shaders/cinematic.hlsl bfv-native-code/mods/shaders/cinematic_shader.h

uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/build.py --game-dir 'X:/Battlefield Vietnam - Copy' --tree readable --mods-only --mods ai_commander,ai_aggressive,rpg7_hd,hd_sky,cinematic --target client --output 'X:/Battlefield Vietnam - Copy/BfVietnam-Cinematic.exe'

# Hidden-window real D3D9 HAL integration test of the same C and shader:
gcc -O2 -DBFV_CINEMATIC_TEST -DBFV_MOD_CLIENT -msse2 -mfpmath=sse -I bfv-native-code/mods/include -I bfv-native-code/readable/runtime -I bfv-native-code/build/client bfv-native-code/mods/client/cinematic.c bfv-native-code/tools/test_cinematic.c -ld3d9 -o bfv-native-code/build/test_cinematic.exe
./bfv-native-code/build/test_cinematic.exe
```

Validation on 2026-10-24:

- Shader compilation: ps_2_0, 700 bytes.
- GPU test: MSAA 0/2/4/8, three cycles each; changed colour output,
  preserved black, viewport/blend/fill restoration, resource reuse,
  device Reset and zero leaked device references all pass. This uses a
  direct stdcall adapter for COM calls, not the live game's mod runtime.
- Build: 33 hooked functions, including existing RPG, sky and AI hooks;
  13,878,784-byte EXE. SHA-256:
  `cb94a1a9fefdf95f3bcb5ba3cc2e968fbb982d348effe7917d2e56ff1f48d568`.
- PE audit: original section bytes differ only at declared entry/vtable
  patches (21,816 changed bytes, zero unexplained); frame caller stays native
  and its world-end call is unchanged. PE headers/new payload are separate
  from this section comparison. Original EXE SHA matches the supported hash.
- Private manifest, audit and GPU log:
  `bfv-reference-local/cinematic/` outside the repository.

**Gameplay and visual tuning remain untested.** The owner's existing game
process was left running. Close it normally and launch BfVietnam-Cinematic.exe
from the game folder, load a map, and check `cinematic.log` for
`world graded before HUD 00000000`. Check the sky horizon, weapon highlights,
dark vegetation, HUD, zoom, pause menus and Alt+Tab. This build has not been
measured for frame-time cost or tested in multiplayer.
