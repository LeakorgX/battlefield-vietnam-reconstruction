# Vulkan renderer

Target design: the client renders through Vulkan. The engine code and its
statically linked D3DX effect framework stay byte-identical. Underneath them,
the build puts a native implementation of the Direct3D 9 interfaces they call
(`bfv-native-code/vk/`). The engine draws exactly as before. Neither d3d9.dll
nor DXVK is used.

**Status 2026-10-25 (second bring-up, RTX 4060 Laptop): the device is complete
and every harness gate passes; game integration is untested.** All 119
`IDirect3DDevice9` slots are filled. The GPU harness draws the same scenes
through system d3d9 and `vk/` and compares the pixels (61 passing checks):
fixed-function geometry with
all four draw calls, shader models 1-3 and a D3DX effect, copies, render to
texture, lighting, MSAA 0-8, blending, a sixteen-draw state-change pass, clears,
state blocks, Present, gamma and Reset. x86 and x64 pass under the Khronos
validation layer with zero errors and warnings, x86 also with the
fixed-function fallback paths (`vulkan.eds3 = 0`, `vulkan.vertexInput = 0`) and
with the production `platform.c`. The `vulkan` mod and the build wiring exist,
but no client has been built or run with them: that needs the supported client
EXE. The game still renders through system d3d9.

This backend is a translation layer. [VULKAN-NATIVE-PLAN.md](VULKAN-NATIVE-PLAN.md)
plans the move to native Vulkan rendering, one subsystem at a time on top of it.

**First in-game run (2026-10-25): the backend loads, then the engine hangs in
its Reset retry loop.** See "First in-game run" under Remaining work.

## How it is wired

- The game's only Direct3D import is `Direct3DCreate9`: thunk `0086791a`,
  IAT slot `00b42444`. Four functions call the thunk: `Setup_Profiles_00439260`
  (at `00439ee8`), `PCDX8Renderer::PCDX8RendererInit` `00a5f700` (at
  `00a5f70e`), `Setup_Profiles_helper_00427c00` and the driver check
  `sub_BAD_DRIVERS_POST_00491e10`.
- The `vulkan` mod (`mods/client/vulkan.c`) hooks `00439260` and `00a5f700`.
  Before either runs, it writes `vk_Direct3DCreate9` into the IAT slot, which
  stays patched. The other two callers get the Vulkan IDirect3D9 only if they
  run after one of those; check the order in `vulkan.log` during step 5.
- Lifted code leaves C for any address outside its catalogue. A call through
  a COM vtable therefore jumps straight into the payload's `__stdcall`
  methods, as it did into d3d9.dll. Mods that call the device
  (`GAME_STDCALL(GAME_VIRTUAL(dev, n), ...)`) reach the same methods.
- The backend needs no import table. It finds `vulkan-1.dll`, `user32.dll`
  and kernel32 functions with the game's own `LoadLibraryA` /
  `GetProcAddress` IAT entries.

## Layers

| File | Role |
|---|---|
| `vk/vkd3d9.h` | Internal header: objects, state, platform functions |
| `vk/platform.c` | Heap, logging, Win32 and Vulkan entry points, `dxvk.conf` options |
| `vk/spirv.c` | SPIR-V module builder (types, constants, instructions) |
| `vk/dxso.c` | D3D9 shader bytecode (vs 1.1–3.0, ps 1.0–3.0) to SPIR-V |
| `vk/ff.c` | Fixed-function vertex and pixel state to generated vs_3_0 / ps_3_0 bytecode |
| `vk/vk_core.c` | Instance, device, memory, command buffers, staging, submission |
| `vk/d3d9_main.c` | `IDirect3D9`: adapter, modes, caps, format checks, `CreateDevice` |
| `vk/d3d9_device.c` | `IDirect3DDevice9`: state, draws, clears, copies, present, reset |
| `vk/d3d9_resource.c` | Textures, surfaces, cube and volume textures, vertex and index buffers |
| `vk/d3d9_shader.c` | Vertex declarations, shaders, state blocks, queries, swap chain |

## Rendering model

- **Vulkan 1.3** with dynamic rendering, synchronization2, extended dynamic
  state 1–3, vertex input dynamic state, push descriptors and custom border
  colours. A pipeline depends only on its two shader modules and the
  attachment formats. Blend, depth, stencil, cull, fill mode, topology,
  vertex layout, viewport and scissor are all set dynamically.
- **Images** stay in `GENERAL` layout. Write-to-read hazards are covered by
  a barrier when a rendering pass ends and around copies.
- **Uploads** (Lock/Unlock, UpdateSurface, UpdateTexture) go through staging
  memory. A resource the GPU is not using is filled by an upload command
  buffer submitted ahead of the frame. A whole-resource write to a busy
  resource gets a fresh backing ("renaming"), so the frame never stalls.
  Partial writes to busy resources are copied in-stream.
- **Dynamic buffers** live in host-visible memory. `D3DLOCK_DISCARD` renames,
  `D3DLOCK_NOOVERWRITE` writes in place.
- **Per draw**, command buffer state (pipeline, vertex input and buffers,
  dynamic state, push descriptors, push constants) is recorded only when it
  differs from what the previous draw of the same submission set. Constant
  blocks are written to the upload ring only when their inputs changed, and
  only registers `[0, floats_read)` that `dxso_scan` finds the shader reading
  (all with relative addressing), plus the integer/Boolean/bump tail when read.
  The descriptor still covers the whole block; its unread rest belongs to later
  ring allocations. The fixed-function register layout (`ff.h`) puts the small
  common registers first, so XYZRHW draws upload two registers and unlit
  transforms twenty. This keeps the ring small in a 32-bit address space.
- **Readback** (GetRenderTargetData, GetFrontBufferData, queries) submits and
  waits only when the result is needed.
- **Present** draws the back buffer into the swap chain image, scaled to the
  window and through the gamma ramp. Vsync, frame latency and anisotropy
  come from the game's existing `dxvk.conf` keys: `d3d9.presentInterval`,
  `d3d9.maxFrameLatency`, `d3d9.samplerAnisotropy`.

## D3D9 semantics kept

- Half-pixel offset: the Vulkan viewport is shifted by +0.5 pixel and
  flipped (negative height). `vPos` is `FragCoord - 0.5`.
- Shader float rules: `0 * x = 0` in mul, mad, dp2–4, the matrix macros and
  nrm (`d3d9.floatEmulation = strict`). `rsq`, `log` and `pow` use |x|.
  min/max return the non-NaN operand.
- ps 1.x: constants clamped to [-1, 1], `texcoord` saturated, `cnd` on r0.a
  (1.0–1.3), co-issue run in order.
- Colour outputs of vs 1.x/2.x are saturated.
- The post-shader stages run inside the generated fragment shader: alpha
  test, fixed-function fog (vertex or table, z or w, never for ps 3.0) and
  point sprites.
- Texture types for ps 1.x and depth comparison (D24/D16 textures sampled
  with hardware shadow comparison; INTZ and DF16/DF24 return depth) are
  shader variants chosen from the bound textures.
- Unbound samplers read (0, 0, 0, 1). An unbound fixed-function stage reads
  white.
- `D3DCREATE_FPU_PRESERVE` is honoured. Without it, the device sets x87 to
  24-bit precision, as d3d9 does.
- Fullscreen changes the display mode. In fullscreen, losing focus loses the
  device (`D3DERR_DEVICELOST`, then `D3DERR_DEVICENOTRESET`) until `Reset`.

## Testing

`vk/test/run_gpu.ps1` builds the standalone GPU harness with warnings treated
as errors (unused COM parameters excluded). Validation is required by default;
use `-NoValidation` explicitly when the matching validation layer is absent.
`-Architecture x64` runs the same sources as a 64-bit validation check. This
does not replace the required 32-bit validation gate for the game.

Scenes compared with system d3d9 (per-channel limit 2 unless noted): 40
fixed-function geometry scenes (DrawPrimitive, DrawIndexedPrimitive and both
`*UP` calls x modulate/add/subtract, blending, alpha test, fog, vertex colour,
depth, stencil, occlusion and event queries); vs/ps 1.1, 2.0 and 3.0 pairs and a
D3DX effect; UpdateSurface/ColorFill/StretchRect (exact); render to texture;
directional lighting; an X8R8G8B8 target with destination-alpha and
BOTH(INV)SRCALPHA blends; sixteen draws in one pass, each after a different
state change (`churn_scene`: it catches stale constant blocks, images, dynamic
state and fixed-function constants); render targets with MSAA 0/2/4/8; clears
with viewport and rectangle clipping (exact); state blocks. Then: 4096 exact
upload/readback pixels, 12 textured presents with pipeline-cache reuse, gamma,
implicit back-buffer references, Reset rejection and Reset, Reset to another
window, an additional swap chain on the device window, and resource lifetime.
Present's displayed pixels are not compared. The last IDirect3D9 release
destroys the Vulkan device and instance, so the validation layer also reports
leaked objects.

Switches: `-Fullscreen` runs the fullscreen scene (display mode change, focus
loss, DEVICELOST/NOTRESET, Reset), which is skipped by default. `-Bench` prints
the per-draw CPU cost of a fixed-function loop on both devices
(`BFV_VK_BENCH=d3d9|vulkan` for one). `vulkan.eds3 = 0` and
`vulkan.vertexInput = 0` (environment or `dxvk.conf`) force the pipeline-state
fallbacks of devices without those extensions. `run_gpu.ps1` works in Windows
PowerShell 5.1 and PowerShell 7.

`vk/test/run_dxso.sh` passes 61 shader translations and `spirv-val` reports zero
invalid modules. Logs/binaries are under `bfv-native-code/build/vulkan-test-*`.

Validation layers: x64 from MSYS2 (`mingw-w64-x86_64-vulkan-validation-layers`,
`-ValidationLayerPath C:\msys64\mingw64\bin`). x86 from the LunarG SDK
1.3.296.0, the last one with 32-bit layers (1.4.304.0 removed them), installed
with its "SDK 32-bit Core Components" (`-ValidationLayerPath
C:\VulkanSDK\1.3.296.0\Bin32`). The first machine used a private x64 layer in
`../bfv-reference-local/vulkan-validation/layers64`. Elevated Windows processes
ignore `VK_LAYER_PATH`: the script then adds a temporary explicit-layer registry
entry and restores it. It disables all implicit layers while validating; the
loader's notices about that are logged but not counted as warnings.

Machine notes: the MSYS2 layer (winpthreads) waits forever in a static
destructor at process exit if the Vulkan instance is still alive, which is why
the harness ends a failed run with TerminateProcess and the backend destroys
the instance on the last release. RivaTuner Statistics Server injects
RTSSHooks.dll into the harness; after the Vulkan benchmark its window hook threw
a C++ exception out of a later CreateWindow (STATUS_FATAL_USER_CALLBACK_EXCEPTION),
so the Vulkan benchmark runs after the last window is created. On a hybrid
laptop the backend picks the discrete GPU; AMD's switchable-graphics implicit
layer can hide it, so non-validated runs there should also set
`VK_LOADER_LAYERS_DISABLE=~implicit~`.

Per-draw CPU cost (`-Bench`, 2000 fixed-function draws per frame with a texture
and world matrix change each, x86, RTX 4060 Laptop for Vulkan and the Radeon
890M for system d3d9): d3d9 1.70 us/draw, Vulkan 2.32 us/draw (4.86 before the
state tracking above). A 32-bit harness without large-address awareness ran out
of address space with full 8.5 KB constant uploads per draw.

## Remaining work

State on 2026-10-25, second bring-up:

| Piece | State |
|---|---|
| `spirv.c`, `dxso.c`, `ff.c` | Written. `vk/test/run_dxso.sh` translates a corpus and runs `spirv-val`. |
| `vk_core.c` | Runs: context/device, allocations, upload ring, readback, submissions, validation callback, teardown on the last IDirect3D9 release. |
| `d3d9_main.c`, `d3d9_resource.c`, `d3d9_shader.c` | Exercised by the GPU harness. |
| `d3d9_device.c` (+ `device_methods.inc`, `device_draw.inc`, `device_window.inc`) | **Complete vtable.** Draw path with per-submission state tracking. Patches, ProcessVertices, palettes, hardware cursor and dialog-box mode return D3DERR_INVALIDCALL and log. |
| `platform.c` | Written. The harness passes linked against it (`-ProductionPlatform`). Its two bootstrap IAT slots match `imp_GetProcAddress`/`imp_LoadLibraryA` in `symbols_client.h`. |
| `mods/client/vulkan.c` | Written: hooks `00439260` and `00a5f700` and writes `vk_Direct3DCreate9` into IAT slot `00b42444` (`imp_Direct3DCreate9`). **Never built or run.** |
| Build wiring | Written: `--mods vulkan` compiles `vk/*.c` into the client and requires `--output`. `vk/*.c` compiles with the mod flags without warnings; the only external symbols are `memcpy`/`memset`/`memcmp` (mod runtime) and libgcc helpers. MinGW needs `mingw-w64-i686-vulkan-headers`. **Not built against the client.** |
| Comparison harness | 61 scenes and checks pass on x86 and x64 under validation (see Testing). |

The next step is step 4's gate on a machine with the supported client
(`79655e9c...`, see SETUP.md), then step 5.

### Plan

Each step has a gate. A step that fails its gate is not built on. Steps 1-3
passed their gates on 2026-10-25; step 4 is written but its gate needs the
supported client; steps 5 and 6 remain.

1. **`d3d9_device.c`** (largest piece, about 2,500–3,500 lines). All 119
   `IDirect3DDevice9` methods, in vtable order: reference counting,
   `TestCooperativeLevel` and `Reset` (lost device), render/sampler/texture
   stage state and transforms, lights, materials, clip planes, shader
   constants, render target and depth binding, viewport and scissor,
   `BeginScene`/`EndScene`, `Clear`, the four draw calls (including `*UP`
   through a streaming buffer), `StretchRect`, `ColorFill`, `UpdateSurface`,
   `UpdateTexture`, `GetRenderTargetData`, `GetFrontBufferData`, `Present`,
   cursor and gamma. The draw path builds the dxso key from the bound
   textures and state, picks or creates the pipeline, writes push
   descriptors and dynamic state, and draws. Also the helpers already
   declared in `vkd3d9.h`. Gate: compiles with `-Wall -Wextra`; every
   vtable slot is filled; links with the other `vk/` files in the harness.
2. **Comparison harness** (`vk/test/`). A 32-bit program that creates a
   window, draws the same scenes through the system d3d9 and through `vk/`
   (validation layers on), reads both back and compares them. Scenes:
   clear, fixed-function textured quad, the fixed-function combiner ops,
   fog, alpha test and blend, a vs/ps 1.x and 2.0 pair, a D3DX effect,
   render-to-texture, depth and stencil, lock/discard streaming, reset.
   Gate: no validation errors; each scene within a small per-channel
   tolerance.
3. **`platform.c`**. The `vkp.h` functions on top of the game's own
   `LoadLibraryA` / `GetProcAddress` IAT entries (heap, `vulkan.log`,
   `dxvk.conf`, time, locks, file I/O); `vkd_cosf` and `vkd_powf` without
   the CRT. Gate: the harness also passes when linked against this file
   through a small shim.
4. **Build wiring and the `vulkan` mod**. Teach `tools/mods.py` /
   `tools/build.py` to compile `vk/*.c` (except the harness) into the client
   when the `vulkan` mod is selected, with the MinGW Vulkan headers on the
   include path. `mods/client/vulkan.c` hooks `00439260` and `00a5f700` and writes
   `vk_Direct3DCreate9` into IAT slot `00b42444` before it runs. Gate: a
   `--mods-only` client builds; without the `vulkan` mod it is still
   byte-identical to the current mods-only build.
5. **In-game bring-up**. Main menu, then a map, windowed first, then
   fullscreen and alt-tab (device lost and reset), resolution change, and
   exit. Check `vulkan.log` and the validation layers. Then the existing
   client mods (`world_render`, `renderer_upgrade`, `hd_sky`, `cinematic`)
   on top, since they call the device directly. Gate: same frames as the
   d3d9 build at the same camera positions (`BFV_WORLD_CAPTURE=1`), no
   validation errors, frame time logged against d3d9.
6. **Docs**. Update this file's status, `HANDOFF.md` and `MODDING.md`.

### Rules `d3d9_device.c` must follow

Read from the other `vk/` files on 2026-10-25. They apply to every change to
the device methods.

- A method that changes state a draw reads must set the matching `DIRTY_*` bit
  (`vkd3d9.h`) when it is not recording. Within a submission, draws record the
  pipeline, vertex input, dynamic state, descriptors, push constants and
  constant blocks again only when their inputs are dirty; SetRenderState sets
  `DIRTY_ALL`. Resource contents need no bit: vertex buffer handles (renamed on
  DISCARD) are compared per draw and autogen mipmaps are checked per draw.
  `churn_scene` in the harness must keep passing.
- Fixed-function registers are named only through the `FF_VS_*`/`FF_PS_*`
  macros, and a shader's constant reads must stay visible to `dxso_scan`
  (`floats_read`, `tail_read`): the upload is cut to what it reports.

- `vkd_create_surface/texture/buffer`, `vkd_create_vdecl`, `vkd_create_shader`
  and `vkd_create_query` do not AddRef the device, but their destroy paths
  Release it. Every device `Create*` method must AddRef the device on success.
  The same goes for `CreateStateBlock` and `EndStateBlock`: `sb_Release`
  Releases the device.
- `vkd_light_slot` is defined in `d3d9_shader.c`, so don't define it again.
  `vkd_present_free(d)` is not in `vkd3d9.h`; declare it in the device file.
- `vkd_state_defaults` reads `d->auto_ds`, so set `auto_ds` before calling it.
  It does not set the viewport or the scissor rectangle.
- While `d->recording` is set, every `Set*` method writes into
  `recording->s` and sets the matching bit in `recording->m`, using the same
  bit layout that `mask_for_type` and `vkd_stateblock_capture` use in
  `d3d9_shader.c`.
- `present_pipeline` in `d3d9_shader.c` expects `d->layout` to be a push-descriptor
  set layout with 22 bindings: bindings 0 and 1 are uniform buffers, bindings
  2–21 are combined image samplers, all for VS and FS. It also expects one
  push-constant range of `sizeof(struct dxso_push)`, and `d->zero_vb` to be a
  zeroed host buffer of at least `sizeof(struct dxso_vs_consts)`.
- `vkd_map` has no removal, and its values are `void*`. A `VkPipeline` or
  `VkSampler` is 64-bit on 32-bit builds, so store map values on the heap.
  `vkd_pipelines_forget` can only mark entries dead.
- `vkd_create_query` only supports EVENT and OCCLUSION. Occlusion restart
  needs `d->rendering`.

### Fixed during first GPU bring-up (2026-10-25)

- Present's pipeline map now stores heap-allocated 64-bit handles, not truncated
  `void*` values. Allocation/map-insertion failure destroys the uncached pipeline.
- Texture-stage capture masks are 64-bit. Native comparison showed that
  PIXELSTATE excludes D3DTSS_CONSTANT; custom recordings retain bit 32. ALL
  retains bit 32 too, but its full Apply path is not tested yet.
- Implicit back-buffer AddRef from zero holds the device until the final Release.
- Present writes a valid fallback descriptor at binding 3 when gamma is disabled.
- Reset saves the old HWND before replacing it, so changing windows actually
  destroys the old Vulkan surface. The two-window reset test covers this.

### Fixed during the second bring-up (2026-10-25, RTX 4060 Laptop + Radeon 890M)

- CreateAdditionalSwapChain on the device window failed with
  VK_ERROR_NATIVE_WINDOW_IN_USE_KHR (Vulkan allows one swap chain per window).
  Additional swap chains create their Vulkan swap chain at the first Present,
  and the swap chain that presents takes the window over from another one of
  the device (the last Present wins, as on D3D9).
- The process-wide Vulkan device and instance were never destroyed; with the
  MSYS2 validation layer every harness process then hung forever in
  LdrShutdownProcess. They are destroyed when the last IDirect3D9 is released
  and created again by the next Direct3DCreate9.
- X8R8G8B8 targets blended with the stored alpha instead of 1 (`no_alpha` was
  unused); D3DBLEND_BOTH(INV)SRCALPHA did not set the destination factor; blend
  ops were not range-checked. `blend_scene` covers them.
- Every draw uploaded both full constant blocks (8.5 KB) and recorded all
  command buffer state: 4.86 us/draw, and the 32-bit harness ran out of address
  space for the upload ring. See "Per draw" above.
- UpdateSurface's invalid calls returned through the "unsupported DeletePatch"
  log line; they now return D3DERR_INVALIDCALL.
- The loader's notices about layers disabled by VK_LOADER_LAYERS_DISABLE
  counted as validation warnings; run_gpu.ps1 failed in Windows PowerShell 5.1
  on the harness's first stderr line.

### First in-game run (2026-10-25)

Client `32aec85d...` (the newer patched build, branch `client-exe-port`) built
with `--mods-only --mods vulkan`, so only the backend, the two hooked
functions and the hand-written C are new; fullscreen, validation on,
implicit layers disabled, launched from a shell.

- Both hooks install `vk_Direct3DCreate9`. The backend picks the RTX 4060
  Laptop (Vulkan 1.4), creates the profile-probe device (800x600 swap chain)
  and then the renderer's IDirect3D9. No validation errors.
- The game's setup rewrote `Mods/BfVietnam/settings/VideoDefault.con`
  (`renderer.FullScreen 1` became `0`) and restarted itself.
- In the restarted process the menu music plays, but the window stops
  responding. The main thread sleeps in `PCDX8Renderer_00a5fbca`, called
  from `PCDX8Renderer::vf33`: its `IDirect3DDevice9::Reset` failed, and
  after any failed Reset (except one `D3DERR_NOTAVAILABLE` case) the engine
  loops `SleepEx(300)` and `TestCooperativeLevel` until
  `D3DERR_DEVICENOTRESET`, without pumping messages. The device was not lost,
  so `TestCooperativeLevel` returned `D3D_OK` and the loop never ends.
- `Reset` does not log why it fails, so the failing check is not known yet:
  a still-referenced default-pool resource or back buffer
  (`D3DERR_INVALIDCALL`), or the fullscreen mode test (`D3DERR_NOTAVAILABLE`;
  the profile asks for 3840x2400 at 60 Hz).
- `BFVWeather.dll`, which the client's entry stub loads, stands down when the
  EXE is not named `BfVietnam.exe`, so it did not interfere.

Next: log every failing `Reset`/`CreateDevice` check with its parameters;
make `TestCooperativeLevel` follow the window's actual foreground state as
D3D9 does (lost and not-reset are now driven only by focus messages, which
this loop never pumps); run with `vulkan.windowed=1` first, launched from the
desktop rather than a background shell.

### Known issues and unimplemented paths

- Not run in the game yet (steps 4-5). The fullscreen scene (display mode
  change, focus loss, Reset) was not run on the second machine.
- Patches (DrawRectPatch/DrawTriPatch), ProcessVertices, palettes (P8 is not
  offered), the hardware cursor (CursorCaps 0) and SetDialogBoxMode fail with
  D3DERR_INVALIDCALL and a log line.
- D3DBLEND_SRCCOLOR2/INVSRCCOLOR2 need dual-source blending, which is not
  enabled; SRCALPHASAT on an X8 target uses the stored alpha.
- A resource released while still bound keeps a device reference until
  it is unbound.
- `vkd_submit` ends the pass, then `vkd_queries_pass_end` ends it again. This
  is harmless.
- Planned device behaviour that is assumed or unsupported: the
  `MultiplyTransform` order is assumed; D3DRS_WRAPn cylindrical wrapping is not
  supported; without `independentBlend`, per-target write masks are invalid;
  spot lights with theta == phi give an infinite constant; unbound vertex
  inputs read zero; XYZRHW with MinZ == MaxZ cannot be represented; the
  half-pixel offset is exactly 0.5.
