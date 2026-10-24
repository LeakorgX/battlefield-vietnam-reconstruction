# Vulkan renderer

Target design: the client renders through Vulkan. The engine code and its
statically linked D3DX effect framework stay byte-identical. Underneath them,
the build puts a native implementation of the Direct3D 9 interfaces they call
(`bfv-native-code/vk/`). The engine draws exactly as before. Neither d3d9.dll
nor DXVK is used.

**Status 2026-10-24: not finished, not in any build.** The game still renders
through the system d3d9. See [Remaining work](#remaining-work) below; the
sections in between describe the design, not a running renderer.

## How it is wired

- The game's only Direct3D import is `Direct3DCreate9`: thunk `0086791a`,
  IAT slot `00b42444`. It is called once, from `00439ee8` in the renderer
  setup.
- The `vulkan` mod (`mods/client/vulkan.c`) hooks that setup function. Before
  it runs, the mod writes `vk_Direct3DCreate9` into the IAT slot.
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

`vk/test/` holds a 32-bit harness. It renders the same scenes through the
system d3d9 and through this implementation, under the validation layers,
and compares the read-back pixels. Every generated SPIR-V module is checked
with `spirv-val`.

## Remaining work

State on 2026-10-24, read from the files:

| Piece | Lines | State |
|---|---|---|
| `spirv.c`, `dxso.c`, `ff.c` | 2442 | Written. `vk/test/run_dxso.sh` translates a corpus and runs `spirv-val`. |
| `vk_core.c` | 1040 | Written, compiles. Never run. |
| `d3d9_main.c`, `d3d9_resource.c`, `d3d9_shader.c` | 3595 | Written, compile. Never run. |
| `d3d9_device.c` | 0 | **Missing.** Declared in `vkd3d9.h` (`vkd_device_create`, `vkd_device_vtbl`, state and pipeline helpers). |
| `platform.c` | 0 | **Missing.** Only the harness version (`test/harness_platform.c`) exists. |
| `mods/client/vulkan.c` | 0 | **Missing.** Nothing installs `vk_Direct3DCreate9`. |
| Build wiring | | **Missing.** `tools/mods.py` only compiles `mods/<target>/*.c`; nothing compiles `vk/` or puts the Vulkan headers on the include path. |
| Comparison harness | | **Missing.** Only the shader translator is tested; no scene is drawn through both d3d9 and `vk/`. |

About 7,000 of an estimated 10,500–11,500 lines exist. Most of the remaining
code is `d3d9_device.c`. Most of the remaining effort is bring-up: none of the
Vulkan side has executed yet.

### Plan

Each step has a gate. A step that fails its gate is not built on.

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
   include path. `mods/client/vulkan.c` hooks `00439ee8` and writes
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

Read from the other `vk/` files on 2026-10-25. Step 1 had not started writing
the file when work stopped.

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

### Known issues in the written files (not fixed)

- `present_pipes` and `drop_pipe` (`d3d9_shader.c`) store a `VkPipeline` in a
  `void*`. On 32-bit builds the 64-bit handle is truncated.
- `mask_for_type` uses `0x1fffffffe` in a `uint32_t`, so stage state 32 is
  never captured in pixel state blocks.
- AddRef on an implicit back buffer does not AddRef the device.
- With gamma off, Present pushes 3 descriptors and skips binding 3.
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
