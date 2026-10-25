# Native Vulkan renderer plan

2026-10-25. A staged plan for moving the client's rendering from the D3D9
translation layer in `bfv-native-code/vk/` to Vulkan code driven by the engine
itself. Nothing here is built yet. [VULKAN.md](VULKAN.md) describes the
translation layer this plan starts from.

## Where things stand

`vk/` implements the Direct3D 9 interfaces on Vulkan 1.3. The engine and its
statically linked D3DX library are unchanged and still issue D3D9 calls; the
backend turns them into Vulkan work. That is a translation layer, the same
idea as DXVK, built into the payload. It is the right first step (every frame
renders through Vulkan with no engine changes), but it keeps D3D9's cost
model: per-call state changes, state blocks, lock/unlock and pipelines picked
at draw time. On the harness it costs 2.32 us of CPU per fixed-function draw
against system d3d9's 1.70 us (Vulkan on the RTX 4060 Laptop, d3d9 on the
Radeon 890M).

A native renderer instead records Vulkan work from the engine's own objects:
materials become prebuilt pipelines, vertex and index blocks become buffers
with known layouts, render passes are explicit, and draws can be sorted and
batched. The D3D9 layer stays as the fallback for everything not yet ported.

## What the engine gives us

The engine was written against its own renderer abstraction, not against D3D9
directly. In `readable/client/ref2/rend/` (80 files):

- **`PCDX8Renderer`** (126 functions): one implementation of a virtual
  renderer interface of about 123 slots (`vf6` to `vf122`). `NullDRenderer`
  (the dedicated server's renderer, 41 functions) implements the same
  interface. Device calls go through the `IDirect3DDevice9*` at
  `PCDX8Renderer+0x90` (for example `vf21`, end scene, calls COM slot 42).
- **Resources**: `PCDX8VertexBlock`, `PCDX8IndexBlock` and their mappers,
  `PCDX8Texture2D/3D/Cube/Depth2D`, `MemVertexBlock`, `MemIndexBlock`.
- **Shaders**: `ShaderManager` (59 functions), `DXEffectManager`, `RaShader`
  and 18 material classes (`RaShaderTerrain`, `RaShaderWater`,
  `RaShaderPVLS1Dif(Lmp)`, `RaShaderPPLS*`, `RaShaderSkn*`,
  `RaShaderEnvironment`, `RaShaderOverGrowth/UnderGrowth`, ...). Each material
  sets the parameters of a D3DX effect; the renderer then runs its passes.
- **Views**: `RenderView` (field of view, projection, frustum).

The scene code that feeds it lives in `ref2/geom` (`PatchTerrain` with 82
functions, `StandardMesh*` in eight classes, `AnimatedMesh`, `PatchWaterCell`,
`GrowthManager`, `OverGrowthManager`, `UnderGrowthManager`, `ParticleSystem`),
`ref2/world` (LOD selectors, `BinTreeCuller`, `DecalManager`, `EffectBundle`),
`ref2/fx` (rain, snow, `ShadowContext`, fonts) and the menu/HUD font renderers.
The statically linked D3DX 9.0b is in `lib/d3dx*.c` (1,292 functions).

The Linux dedicated server (`bfv_linded.static`, unstripped) names part of
this interface: `MemVertexBlock`/`MemIndexBlock` (`create`, `lock`, `unlock`,
`update`, `getData`, `getVertexStride`, `resizeDynamic`), `RenderView`
(`setFieldOfView`, `setProjectionType`, `setTransformation`, `getFrustum`,
...) and `BlendMode`. The D3D implementations fill the same slots, so those
names carry over slot for slot. It does not contain the full renderer.

What `vk/` already has that a native renderer reuses unchanged, about 4,100 of
its 11,700 lines: `vk_core.c` (device, memory, upload ring, submission),
`spirv.c` (SPIR-V builder), `dxso.c` (D3D9 shader bytecode to SPIR-V) and
`ff.c` (fixed-function state to shaders). The D3D9 state machine
(`d3d9_*.c`, `device_*.inc`, about 6,900 lines) is what the native paths
bypass; it stays as the fallback.

## Approach

Port one subsystem at a time underneath the existing renderer, on the same
Vulkan device as the translation layer. A ported subsystem records its draws
directly into the translation layer's current pass; everything else keeps
going through D3D9 calls. The game renders correctly at every stage, and each
stage can be compared pixel for pixel against the translated frame it
replaces.

When every per-frame draw is native, the end state is a Vulkan implementation
of the engine's renderer interface, beside `PCDX8Renderer` and
`NullDRenderer`. Replacing the renderer interface in one step instead would
mean writing all 123 slots, every resource class and an effect runtime before
anything draws, with nothing to compare against in between.

Shaders: keep compiling the game's `.fx` effects with its own D3DX 9.0b at load
(D3D9 bytecode, then `dxso.c` to SPIR-V, which the harness already exercises
with a D3DX effect). Build each material's pipelines at level load instead of
at first draw. Offline HLSL-to-SPIR-V for new shaders can come later.

## Stages

Each stage has a gate. A stage that fails it is not built on.

| # | Stage | Size | Gate |
|---|---|---|---|
| 0 | In-game bring-up of the translation layer (VULKAN.md step 5) | days to weeks | Same frames as d3d9 at fixed camera positions, no validation errors, device reset works |
| 1 | Coexistence API in `vk/` | about 1 week | A native draw recorded mid-frame leaves every other draw unchanged (new harness scene beside `churn_scene`) |
| 2 | Renderer interface map | 2 to 3 weeks | Every renderer slot named or marked unknown, with its callers and per-frame call counts |
| 3 | Terrain | 2 to 4 weeks | Pixel comparison against the translated frame on three reference views; frame time |
| 4 | Static meshes | 4 to 8 weeks | Same, plus draw count and CPU time per frame |
| 5 | Skinned meshes | 2 to 4 weeks | Same, with animated characters in view |
| 6 | Water and sky | 2 to 3 weeks | Same, on a water map |
| 7 | Foliage (over/undergrowth) | 2 to 3 weeks | Same, on a dense jungle map |
| 8 | Particles, decals, effect bundles, rain and snow | 2 to 4 weeks | Same, with explosions and weather |
| 9 | Post chain and sun shadows | 3 to 6 weeks | Same, against the `renderer_upgrade`/`world_render` build |
| 10 | UI, fonts, HUD, menus | 2 to 4 weeks, or never | Menus and HUD identical; this can stay on the translation layer |
| 11 | Native renderer interface | after 3 to 10 | The world renders with no D3D9 calls; D3D9 remains only for D3DX-only paths and mods |

Sizes are rough, for one person who knows the codebase. Together that is
roughly six months to a year.

### 1. Coexistence API

A small interface in `vk/` for native code inside a frame:

- `vkd_native_begin()` flushes pending D3D9 state, makes sure a rendering
  pass is active and returns the command buffer, the colour and depth
  formats, the current viewport and the upload ring.
- `vkd_native_end()` marks all D3D9 draw state dirty (`DIRTY_ALL`), so the
  next translated draw records everything again.
- Native code allocates buffers, images and pipelines through `vk_core.c` and
  shares the device's descriptor layout or creates its own.
- The x87 control word is left exactly as the device set it. Without
  `D3DCREATE_FPU_PRESERVE`, D3D9 (and `vk/`) put x87 in 24-bit precision, and
  the client's game logic runs under that setting.

### 2. Renderer interface map

Before replacing anything, record what each part does: the `PCDX8Renderer`
slots; the vertex/index block and texture interfaces (named in part from the
Linux server); `RenderView`; `ShaderManager`, `DXEffectManager` and the
`RaShader` base class. Count per-frame calls with a `--mods-only` logging
hook. Write it to `docs/RENDERER-INTERFACE.md`. Each later stage starts from
this map.

### 3. Terrain first

One shader family, large screen coverage, few state changes and an easy
visual check. `world_render` already documents the material setter
(`RaShaderTerrain::vf7`, `00a6cbb0`) and the parameters it binds. Terrain
patches live in `PCDX8VertexBlock`/`PCDX8IndexBlock` objects whose D3D9
buffers `vk/` backs with Vulkan buffers, so the native path should be able to
use those buffers directly: the terrain pipeline is built at load, and each
patch becomes one indexed draw. Stage 2 confirms the layouts first.

### 4 to 8. The world, by draw volume

Static meshes are the bulk of the draws and the main CPU cost. Each
`RaShader` material maps to prebuilt pipelines, and draws are sorted by
material and batched. Skinned meshes add the bone palettes. Water and sky
include the planar reflection pass that `world_render` adds. Foliage is
where instancing helps most. Particles, decals and weather come last of the
world, because they mix many small draws with fixed-function state.

### 9. Post and shadows

`renderer_upgrade`'s FP16 target, SSAO, bloom, exposure and FXAA, and
`world_render`'s shadow cascades, become native passes; the full-screen
passes are natural compute work.

## Risks

- **Mods that call the device.** `world_render`, `renderer_upgrade` and
  `cinematic` make 69 direct device calls. They keep working while their
  subsystems are on the translation path; each stage that ports a subsystem
  must carry their effects over (for example the terrain stage includes
  `WorldTerrain.fx`'s parameters) or switch their hooks off for it.
- **The newer patched client.** `BFVWeather.dll`, loaded by the client's
  entry stub, hooks the import slot of `Direct3DCreate9` (`00b42444`), then
  `CreateDevice` and device methods. The `vulkan` mod writes its own pointer
  into the same slot, so BFVWeather never sees a device under Vulkan. It needs
  a chaining fix (let its hook wrap `vk_Direct3DCreate9`) before weather
  effects work on any Vulkan build.
- **D3DX effect semantics.** Techniques, passes and state assignments live in
  D3DX, so native materials must reproduce what each pass sets. The
  interface map (stage 2) records it.
- **Floating-point state.** See the coexistence rules above.
- **Device loss and reset.** Native resources must be recreated on the same
  events as the translation layer's (alt-tab, resolution change).
- **Single-threaded engine.** Command recording stays on the game thread at
  first. Recording native passes on worker threads is a later optimization,
  not a premise.
- **Expected gains.** The GPU load of a 2004 game is small; the win is CPU time
  per frame (batching, prebuilt pipelines, no D3D9 emulation) and room for
  modern rendering. Frame time is measured at every gate, so a stage that
  doesn't pay off is visible.

## First step

Stage 0: run the translation layer in the game. A test client with only the
`vulkan` mod (`--mods-only --mods vulkan`) exercises it on the real engine
with everything else native. Its `vulkan.log` and validation output decide
what stage 1 can rely on.
