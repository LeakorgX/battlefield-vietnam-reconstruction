# Vulkan renderer

Target design: the client renders through Vulkan. The engine code and its
statically linked D3DX effect framework stay byte-identical. Underneath them,
the build puts a native implementation of the Direct3D 9 interfaces they call
(`bfv-native-code/vk/`). The engine draws exactly as before. Neither d3d9.dll
nor DXVK is used.

**Status 2026-10-25: GPU bring-up works; game integration is unfinished.**
The standalone harness runs on the RTX 3060: exact native-D3D9 clear/readback
parity, state-block comparisons, textured Present, gamma and windowed Reset.
The x64 run has zero validation errors/warnings; the x86 run passes without
validation (no x86 validation layer installed). Device draw calls are still
missing. The game still renders through system d3d9; no Vulkan mod is installed.
The sections below describe the target design where not explicitly tested.

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

`vk/test/run_gpu.ps1` builds the standalone GPU harness with warnings treated
as errors (unused COM parameters excluded). Validation is required by default;
use `-NoValidation` explicitly when the matching validation layer is absent.
`-Architecture x64` runs the same sources as a 64-bit validation check. This
does not replace the required 32-bit validation gate for the game.

Current GPU tests: 4096 exact upload/readback pixels; native-D3D9 versus Vulkan
clear pixels with viewport and rectangle clipping; custom/pixel state blocks,
including recording isolation and constant bounds; 12 textured presents with
pipeline-cache reuse; gamma pipeline; implicit back-buffer references; Reset
rejection with a held back buffer; successful Reset and clear; Reset to another
window (surface replacement). Present's final displayed pixels are not compared
yet. No DrawPrimitive/DrawIndexedPrimitive or fixed-function scene is tested.

`vk/test/run_dxso.sh` passes 61 shader translations and `spirv-val` reports zero
invalid modules. Logs/binaries are under `bfv-native-code/build/vulkan-test-*`.
The current machine's private x64 validation layer is outside the repository at
`../bfv-reference-local/vulkan-validation/layers64`. Elevated Windows processes
ignore `VK_LAYER_PATH`: the test used a temporary explicit-layer registry entry,
restored afterward, and disabled the unrelated ReShade/RTSS implicit layers for
that process. No validation layer or registry entry was installed permanently.

## Remaining work

State on 2026-10-25:

| Piece | Lines | State |
|---|---|---|
| `spirv.c`, `dxso.c`, `ff.c` | 2442 | Written. `vk/test/run_dxso.sh` translates a corpus and runs `spirv-val`. |
| `vk_core.c` | | Runs: context/device, allocations, upload/readback, submissions; optional validation callback. |
| `d3d9_main.c`, `d3d9_resource.c`, `d3d9_shader.c` | | Exercised by the GPU harness; broader resource/shader coverage remains. |
| `d3d9_device.c` | | **Partial.** Creation, layout, lifetime, clear/readback/present, windowed reset, state setters/constants/blocks. Vtable is incomplete; no device draw path. |
| `platform.c` | 0 | **Missing.** Only the harness version (`test/harness_platform.c`) exists. |
| `mods/client/vulkan.c` | 0 | **Missing.** Nothing installs `vk_Direct3DCreate9`. |
| Build wiring | | **Missing.** `tools/mods.py` only compiles `mods/<target>/*.c`; nothing compiles `vk/` or puts the Vulkan headers on the include path. |
| Comparison harness | | **Partial.** Clear pixels and a state-block subset match native D3D9. Full scene suite remains. |

The next implementation target is the device draw path (shader/FF selection,
constants/descriptors, pipelines/dynamic state and vertex/index inputs) and the
remaining COM slots. Do not install the backend in the game while vtable slots
are empty. The first GPU execution gate is now covered; the complete device
and scene-comparison gates below have not passed.

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

Read from the other `vk/` files on 2026-10-25. Step 1 is partly implemented;
the following ownership/layout requirements still apply to new methods.

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

### Known issues and unimplemented paths

- The device vtable is incomplete and fullscreen creation/reset is explicitly
  rejected. Lost-focus behavior, complete default-pool reset validation, draws,
  resource creation/binding/copies, transforms/lights and remaining state need
  implementation. The GPU harness uses only populated methods.
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
