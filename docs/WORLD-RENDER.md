# World material and lighting upgrade

2026-10-24. This replaces the earlier colour-grading-only experiment for the
owner's cinematic rendering request. Play with **BfVietnam-WorldRender.exe**.
The `cinematic` screen filter is excluded from this build.

## Implemented

- Ordinary lit static meshes and lightmapped meshes: programmable vertex
  shaders and per-pixel GGX direct lighting replace fixed-function lighting.
- Normal-mapped meshes: retain the retail RGB tangent normal/gloss encoding,
  normalise per pixel, and use roughness-dependent direct and image-based
  specular. The RPG has a different DXT5nm/ORM material and stays on its own
  existing PBR shader.
- Gloss-mask materials (including retail M16/AK textures): alpha controls
  roughness, not transparency. The mesh's native opacity is retained.
- Skinned PVLS and tangent-normal characters: four-bone position/normal
  skinning and the new per-pixel shading. Bone palettes and scrolling UVs
  still come from the engine.
- Reflections: the HD sky's own prefiltered cube (`texture_003.rfa`), turned
  with the sky, analytic split-sum
  environment BRDF, and the material's original gloss exponent. Legacy
  environment materials retain their reflectivity parameter.
- Detail on surfaces without normal maps: a small surface-gradient relief
  inferred from their diffuse detail. This is an approximation; painted
  texture brightness is not measured height.
- Terrain: keep the map's baked macro lighting and shadows; add
  distance-faded detail relief lit by the map's sun, relative to the original
  geometric slope. Detail textures use anisotropic filtering.
- Water: two animated normal scales, per-pixel reflection vectors,
  angle-dependent Fresnel reflection and a directional sun highlight;
  retain native shoreline coverage, water scale, speed and opacity inputs.

Original geometry, sky, particles, UI, and gameplay archives are not rewritten.
Unlit materials and fixed-function foliage remain on their original paths.
The unusual legacy object-space normal shaders (PPLSWs1Dif and its skinned
version) are not converted by this version: their mirrored normal encoding
needs a separate implementation.

## Scope and limits

This is a renderer upgrade over the retail assets, not a port of a modern
engine. Retail assets have no authored roughness or metalness set.
Roughness is estimated from the original specular exponent and gloss masks.
Ordinary materials use dielectric F0, so painted vehicles, cloth and
vegetation are not made metallic. The RPG's authored metalness is preserved.

Added on top of the materials (stages 2–9 of
[RENDERER-PLAN.md](RENDERER-PLAN.md); its status table lists every
deviation):

- Sun shadow maps.
- SSAO.
- HDR bloom.
- Auto exposure and a hue-preserving highlight shoulder.
- Height fog with sun in-scatter.
- Edge antialiasing (FXAA 3.11 quality) on the current frame.

Not added:

- Global illumination.
- Volumetric lighting.
- Displacement.
- Per-object motion vectors.
- Shadows from foliage.

The retail projected shadows still draw as before.

## FP16 scene target (renderer plan stage 1)

`mods/client/renderer_upgrade.c` redirects the world to an FP16 target
with an INTZ depth texture (plain D24S8 when INTZ is missing; depth-based
stages then stay off) and resolves it into the back buffer at
`renderList_end`, before the HUD. The target holds display-encoded colour,
as native draws write it, so their blending and device fog are unchanged.
Upgraded effects end in `pow(lit, 1/2.2)` unclamped and use the retail
per-vertex fog factor (`FOG` output from `FogData`), so every draw is fogged
once, by the device. The finish pass (`mods/shaders/world_post.hlsl`,
compiled to `world_post_finish.h` by `tools/compile_post_shaders.py`)
decodes once and, in stage 1, clamps and re-encodes. The earlier sun-shadow
replay, bloom, post fog, 4-tap AO, screen reflection and history blend
were removed; each returns as a stage of [RENDERER-PLAN.md](RENDERER-PLAN.md).
With a multisampled back buffer (`rpg7_hd`), the world target is an FP16
MSAA surface with MSAA depth, resolved with `StretchRect`. The engine's
`setRenderTarget` (`00a563e0`) rebinds its back buffer after render-to-texture
passes; while the world target is active it is bound again instead. Menu
frames (`[0xd444c4]+0x10c` set) skip `renderList_end` and stay on the back
buffer; end scene (`00a55fa0`) resolves any frame that still has the target.
Shared effect parameters (fog, sun, sky, toggle) are set once per effect per
frame; the environment material still rebinds the sky every draw because
its native setter binds its own cube.

## Stages 2–9: shadows, post chain, antialiasing

**Sun shadows** (`world_render.c`):

- The first world draw of a frame captures the camera into `world_shared`
  (`world_shared.h`). It reads the view from `mgr+0x14` and the projection
  from `mgr+0x94`.
- Every main-view mesh and terrain draw is then also replayed into two
  orthographic cascades. The pass uses the light's view-projection
  (`ReflectionWVP`, `ShadowPass=1`, colour writes off, depth bias).
- Two atlases alternate. Draws sample the one written in the previous frame,
  with that frame's matrices.
- The receivers are `WorldSurface.fx` and `WorldTerrain.fx`. Their vertex
  shaders compute the cascade coordinates, so `ps_2_a` stays under 32
  constants.

**Post chain** (`renderer_upgrade.c`, `world_post.hlsl`, all `ps_3_0`):

1. Linear depth (INTZ).
2. Half-resolution SSAO.
3. Composite: blurred AO and a light height fog with sun in-scatter, from
   the map's device fog colour. The sky is not fogged again.
4. Edge antialiasing (FXAA 3.11 quality, subpixel 0.5).
5. Six-level bloom downsample.
6. Exposure adaptation.
7. Bloom upsample.
8. Finish: exposure, highlight shoulder, saturation and range-clamped
   sharpening, then encode.

There is no temporal antialiasing and the projection is never changed.

**Depth snapshot:** a mid-frame Z clear (`00a55e30`, the first-person pass)
takes a copy of the depth first, so fog and AO still see the world's
depth.

**Water:** the planar reflection is full resolution. Where it holds no
geometry, the water reflects the HD sky cube `texture/W_hd_sky_env.rcm`
(`texture_003.rfa`). `skybake --cube` bakes it from the same HDRI, shading
and tone curve as the sky faces, and `envfilter` GGX-prefilters it (9 mips,
roughness = mip/7). The sky mesh draws turned by `sky+0xb4 + sky+0x38`
degrees, so `SkyYaw` (cos, sin) turns each direction back before the
lookup. Metals and glossy surfaces use the same cube. The screen-space
water reflection was removed after the first game test (stair-stepped).
The earlier copy of the frame's sky was also removed.

Everything is created once per resolution and released on device reset.
Without INTZ, or if a post shader fails to create, the finish pass falls
back to decode-only.

## Files and engine integration

`mods/client/world_render.c` adds native engine hooks. The authored effects
are in `mods/shaders/world/`. `tools/package_world_shaders.py` writes ten
retail-name effect entry points and three shared files into a new
`effects_003.rfa` (13 entries, all authored here). It does not package the
retail RaCommon.fx: the engine resolves that include from its own archive.

The file-manager hook mounts `_003` immediately before the RPG's
`effects_002.rfa`; first-mounted wins. It activates only in EXEs that include
`world_render`, so the older RPG and colour-grade builds do not mount it.
No original EXE/archive or RPG/sky archive is overwritten.

The same replacement effects serve the opaque/blended shader instances.
The upgraded instances are identified by their original engine globals,
not by vtable alone: the RPG's separate PBR1Dif instance shares a retail
vtable and must be excluded.

Evidence in readable/lifted client sources under `ref2/rend/`:

- PVLS `00a6e1a0` and PVLS-lightmap `00a6e0e0` originally feed fixed-function
  material state. After them, call the native programmable object setter
  `00a4b650` to populate the replacement effect's existing base-class handles.
- `00a4b650` supplies object-space lights, eye and WVP. Its after-hook adds
  World, surface parameters and the sky cube.
- Base material setter `00a4bf90` retains diffuse binding and render states;
  the hook adds the existing specular/ambient helper `00a4b5a0`.
- Environment `00a6c450` binds its old cube after the base setter. A final
  hook reapplies the prefiltered sky while leaving native reflectivity.
- Terrain `00a6cbb0` binds the terrain colour texture and object transform.
  The hook derives WVP from that object matrix and the current native view
  and projection matrices, and supplies sun/fog data.
- Water `00a6c6b0` retains native WVP/eye updates; the hook supplies the map's
  sun and prefiltered sky.

## Comparison and diagnostics

**F7** toggles sun shadows, **F10** the post chain (exposure, tone
mapping, bloom, fog, AO), **F11** edge AA. **F8** toggles the new lighting model. OFF selects a retail-style lighting
approximation inside the replacement shaders; it does not restore the
original shader bytecode. Use the same camera position to compare.
`world_render.log` records the mounted archive, selected `world_*_v2`
techniques, first material bindings and toggles.

For local framebuffer diagnostics only, launch with environment variable
`BFV_WORLD_CAPTURE=1`. That test process uses windowed presentation and saves
`world_render_before.bmp` / `world_render_after.bmp` on successive gameplay
frames after a short delay, with the comparison model OFF/ON. Normal launches
do not change presentation settings or write captures. Capture call-site
guards assume **--mods-only**, with frame routine `004a8af0` staying native.

## Build

From the repository root (existing private tools are local prerequisites):

```powershell
$env:PATH = 'X:/Battlefield Vietnam - Copy;C:/msys64/mingw32/bin;' + $env:PATH
C:/Python314/python.exe bfv-native-code/tools/package_world_shaders.py --game-dir 'X:/Battlefield Vietnam - Copy' --rftool X:/temp/rpgwork/rftool/bin/Release/net9.0/rftool.exe --gamefx X:/temp/rpgwork/tools/gamefx.exe --retail-common X:/temp/rpgwork/fx/effects/RaCommon.fx --sky-env X:/temp/rpgwork/env_hdsky --install
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/build.py --game-dir 'X:/Battlefield Vietnam - Copy' --tree readable --mods-only --mods ai_commander,ai_aggressive,rpg7_hd,hd_sky,world_render,renderer_upgrade,fast_start --target client --output 'X:/Battlefield Vietnam - Copy/BfVietnam-WorldRender.exe'
```

The package command requires every effect to compile, validate, select its
upgrade technique and begin a pass with the game's actual statically linked
D3DX 9.0b on a D3D9 HAL device. The archive writer reads back every member.
Replacing a previous `_003` saves its content-addressed backup under the
private `bfv-reference-local/world-render/` folder first.

## Research references

- [Lagarde/de Rousiers: Moving Frostbite to Physically Based Rendering](https://seblagarde.wordpress.com/2015/07/14/siggraph-2014-moving-frostbite-to-physically-based-rendering/)
  separates material response, environment lighting and image processing.
- [Mikkelsen: Surface Gradient–Based Bump Mapping Framework](https://jcgt.org/published/0009/03/04/)
  describes derivative-based surface relief without requiring a new tangent
  stream for every existing mesh.
- [NVIDIA: High-Quality Software Rendering Effects in Real-Time Applications](https://developer.nvidia.com/gpugems/gpugems/part-v-performance-and-practicalities/chapter-35-leveraging-high-quality-software)
  discusses material attribute maps and environmental lighting inputs.

## Verification status

All ten effects compile, validate, select the new technique and begin a pass
with the game's actual compiler. Initial live runs loaded and bound mesh,
terrain and water upgrades and the prefiltered sky; device reset completed
without a logged exception. Direct Windows capture failed with
`FrameArrived timed out` / `window capture timed out`, so engine framebuffer
diagnostics were added. Early captures showed the loading overlay and do
not establish visual quality. Final gameplay visual inspection and broader
map/vehicle/character/performance/multiplayer testing remain to be recorded.
Stages 2–9 had a first game test (2026-10-24). It showed no visible shadows,
a dark, hazy image and stair-stepped water. Fixes:

- The water reflection replay ran inside the shadow replay and overwrote it.
- Caster render states are now set after the effect re-applies its pass.
- Exposure no longer darkens below native.
- Fog and AO are lighter.
- SSR is removed.

Second game test (2026-10-24): the image only looked right with F11 and F10
off. Three problems were reported: lines, washed-out textures and
overlapping, plus water without the sky and a bridge that went dark up
close. The root causes and fixes, with F10 and F11 still on by default:

- **Lines.** The SSAO rotation came from a per-pixel hash, was never
  blurred and changed every frame. It now uses a fixed 4×4 rotation tile
  and a depth-aware 4×4 blur. Sharpening is clamped to the neighbour range,
  so it adds no edge halos.
- **Overlapping.** TAA reprojected the first-person weapon as world
  geometry, which dragged a second copy behind it. Weapon-depth pixels now
  keep their screen position. History is read with Catmull-Rom instead of
  bilinear, which re-blurred it every frame. Feedback drops with motion.
  View rays come from the world camera's projection instead of the last
  one set.
- **Washed out.** The Neutral tone map desaturated highlights toward white.
  Exposure rose to 1.35 and the sun to 1.3×. A second height fog covered
  the sky. Now: a hue-preserving shoulder above 0.8, exposure 1.0–1.1, sun
  1.1×, a lighter fog that skips the sky, and a gentler S-curve and
  saturation.
- **Bridge dark up close.** The near LOD uses the lightmapped fixed-function
  `PVLS1DifLmp`, while the far LOD does not. The replacement dropped the
  engine's `textureFactor` floor and used only the lightmap's blue channel.
  Lightmapped surfaces now never fall below their native shading.
- **Water sky.** See the Water section above.

Third game test (2026-10-24): water still showed no sky, the bridge still
went dark, moving showed see-through lines and the image was soft. Fixes:

- **Water sky.** The water used the RPG studio cube and the frame copy.
  It now uses the HD sky cube, turned with the sky (Water section).
- **Bridge.** `o_riverbridge_m1` and the other Oblivion bridge objects have
  no object lightmap in `ObjectLightMaps/`. The texture manager then returns
  null and `PVLS1DifLmp` binds it. Fixed function reads an unbound stage as
  white; a pixel shader reads black. `LightMapBound` (0 when `render+0x98` is
  null) restores the white.
- **Lines when moving, softness.** TAA had no per-object motion vectors, so
  moving and alpha-tested geometry left see-through trails. Its jitter and
  history also blurred every frame. TAA, the jitter and the projection hooks
  (`00a42070`, re-jitter in `00a48a50`) are removed. A single-frame FXAA 3.11
  pass replaces them. Sharpening is raised from 0.25 to 0.35, still clamped
  to the neighbour range.

Not yet re-tested.

Stage 1 (2026-10-24): the three effects compile, validate and select
`world_*_v2` with the game's D3DX 9.0b; the archive and EXE build. Not yet
tested in game: the log line `world: FP16 scene target ready`, F8 A/B,
alt-tab reset, frame time.

Private build audit, manifest, effect compiler output and live logs belong
in `bfv-reference-local/world-render/`, outside the repository. Nothing was
committed or pushed.
