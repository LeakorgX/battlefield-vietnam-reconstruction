# Renderer upgrade plan

2026-10-24. A staged plan for the owner's renderer request. It builds on the
existing client mods `world_render` (material, terrain and water effects in
`mods/shaders/world/`, packed into `effects_003.rfa`) and `renderer_upgrade`
(FP16 target and post chain, `mods/shaders/world_post.hlsl`). See
[WORLD-RENDER.md](WORLD-RENDER.md) for the hooks already in use.

Ground rules for every stage:

- Engine-integrated D3D9 only: hooks in `mods/client/`, effects in
  `effects_003.rfa`, no proxy DLL, no ReShade. Original EXEs and archives
  stay untouched. Build with `--mods-only` (frame routine `004a8af0` must
  stay native; the hooks check its return addresses).
- Each feature has its own switch (F8 stays the whole-upgrade A/B), so every
  stage can be compared with the one before it at the same camera position.
- Gate per stage: every effect compiles and begins a pass with the game's
  D3DX 9.0b (`package_world_shaders.py`); device reset (alt-tab, resolution
  change) recreates everything; `BFV_WORLD_CAPTURE=1` before/after captures
  on three reference views (a jungle map, a water map, a night/fog map);
  frame time and draw-call count logged against the previous stage. A stage
  that fails its gate is not stacked on.

## Implementation status (2026-10-24)

Stages 2–9 are implemented and built into `BfVietnam-WorldRender.exe`. All
effects pass the D3DX 9.0b gate and the post shaders compile. **None of it
has been tested in game yet**: reset, the captures, frame time and visual
quality are all still pending.

| Stage | State | Where | Deviation from the plan below |
|---|---|---|---|
| 2 Planar water reflection | built | `world_render.c`, `WorldWater.fx` | Still replayed per draw in the main pass (the stage-1 mechanism). The target is now FP16, with an R8G8B8A8 fallback. |
| 3 HDR | built | `WorldSurface.fx`, `renderer_upgrade.c` | The target still holds display-encoded colour; the post chain decodes it to linear. The sun is scaled by `WorldHdrScale` (1.1 while the post chain is on). No MSAA: the world target is single-sample and TAA replaces MSAA. |
| 4 Exposure and tone mapping | built | `adapt`, `finish` | Exposure comes from the 1/64 bloom level, not a separate luminance chain. Hue-preserving shoulder: identity below 0.8, so the native palette matches the plain resolve. Exposure is clamped to 1.0–1.1. |
| 5 Bloom and height fog | built | `bloom_down`/`bloom_up`, `composite` | Six levels, mixed before tone mapping. Height fog and sun in-scatter use the map's fog colour, starting at 30 m. |
| 6 Lightmaps as occlusion | built | `WorldSurface.fx` | Direct light is `min(lightmap visibility, shadow)`. Ambient and environment light are reduced in baked shadow. A lightmapped surface is never darker than its native shading with the sun shadow applied. For `PVLS1DifLmp` that is `albedo × saturate(lightmap × vertex light + textureFactor)`, and the engine's per-material `textureFactor` is now bound. |
| 7 PBR where data exists | from stage 1 | `WorldSurface.fx` | Relief from diffuse brightness only under `WR_RELIEF`. There is no material table yet. |
| 8 Sun shadows | built | `world_render.c`, `WorldSurface.fx`, `WorldTerrain.fx` | Two cascades (30 m and 150 m) in a 4096×2048 D24X8 atlas with hardware PCF. Casters are this frame's main-view mesh and terrain draws, replayed per draw, so there is **one frame of latency**. Fixed-function foliage and objects outside the main view do not cast. The retail projected shadows are not disabled. |
| 8 SSAO | built | `depth_lin`, `ssao`, `composite` | 12-tap Alchemy SSAO from INTZ depth only (no normal MRT), at half resolution. The sample rotation repeats over 4×4 pixels and composite blurs that tile with depth weights. It multiplies all scene light, not only ambient. |
| 9 TAA | removed | | Camera-only reprojection (no per-object motion vectors) left see-through trails on moving and alpha-tested geometry, and jitter plus history softened the image (third game test). Replaced by `aa`: single-frame FXAA 3.11 quality, no jitter, no history. |
| 9 SSR | removed | | The 10-step water march produced stair-stepped reflections in the first game test and was removed. Water uses the full-resolution planar reflection, then the HD sky cube (`texture_003.rfa`), turned with the sky. |

Keys: **F7** sun shadows, **F8** the whole material upgrade, **F9** capture,
**F10** post chain (off = plain decode), **F11** edge AA.

## Current state and the defects stage 1 fixes

Before stage 1, `renderer_upgrade.c` switched on HDR, sun shadows, bloom, fog, SSAO, SSR and
temporal blending at once. Read from the code and shaders:

1. **Gamma.** Upgraded materials end in `pow(lit, 1/2.2)`, so the FP16
   target holds display-encoded values. Fixed-function draws (foliage, sky,
   particles, legacy materials) are also display-encoded. The finish pass
   then applies ACES to them, which is a second curve: the HDR target holds
   LDR data and highlights are not brighter than 1.
2. **Fog is applied twice.** Each world shader fogs with `sqrt` of a linear
   ramp towards a hard-coded `WorldFogColor` (0.79, 0.80, 0.84), and the
   finish pass adds `exp(-z²)` fog towards `D3DRS_FOGCOLOR`. Fixed-function
   draws still use native fog. The three disagree in colour and curve.
3. **Depth.** Without INTZ, the aux MRT holds depth only where an upgraded
   shader drew (normalised by fog end); other draws keep the clear value, so
   post fog and AO are wrong there. With INTZ the depth is hyperbolic and the
   two paths feed the same post code.
4. **Texture binding and state.** Shadow map, sky cube and reflection are
   set on shared effects by name every draw; post passes change sampler and
   render states behind the engine's state cache and rely on one state
   block. The shadow map is A8R8G8B8 (8-bit depth) and lags one frame.
5. **Placeholder effects.** SSR marches a fixed screen direction, SSAO is a
   4-tap depth difference, and the temporal blend has no jitter and no
   reprojection (it smears in motion). Shadow and reflection passes replay
   each draw inside the main pass with two render-target switches per draw.

## Stage 1: Material, fog, gamma and texture-binding correctness

Status 2026-10-24: implemented and built; effects pass the D3DX 9.0b gate;
not yet tested in game (reset, captures, frame time pending).

As implemented (differences from the first draft are noted):

- One colour space: the scene target holds display-encoded colour, the
  same encoding native draws produce, so native blending and fog stay
  exact. Upgraded shaders output `pow(lit, 1/2.2)` unclamped; the finish
  pass decodes with `pow(2.2)` once, where later stages work in linear
  light. This replaces the drafted sRGB sampling of native draws and the
  "native draw" MRT flag, which would have changed native output.
- One fog: the world shaders no longer fog towards a hard-coded colour and
  the post fog is gone. Upgraded shaders emit the retail vertex fog factor
  `(FogData.y - w) / (FogData.y - FogData.x)` and leave `FOGENABLE` to the
  engine, so the device applies the map's fog to every draw once (fixed-
  function fog still applies after ps_2_x shaders). Native fog is never
  turned off.
- Depth: INTZ depth texture, or a plain D24S8 surface with the depth
  stages off. No partial depth MRT. Linearisation comes with the first
  stage that reads depth.
- Bindings: fog, sun, sky and the toggle are set once per effect per frame
  (effect + frame id); per-object parameters stay per draw. The finish
  pass captures and reapplies a D3DSBT_ALL state block, binds nothing that
  is also a render target, and unbinds its texture. The shadow map and its
  per-draw replay are removed (stage 8). A debug-runtime validation run is
  still to do.
- Material table: deferred to stage 7, where metallic/roughness use it.
- Output: LDR clamp, so F8 and `BFV_WORLD_CAPTURE` compare against native.

## Stage 2: Planar water reflections

- Reflections are rendered once per frame in a dedicated pass, not per draw
  inside the main pass. Record (shader, render item) pairs during the main
  pass's submission through `00a4b490`; at the start of the next frame's
  world (`00a55f50`, return `004a8b71`), replay them into the reflection
  target with the mirrored view (plane `y = water height`), a user clip plane
  at the water surface and flipped culling. Mirrored view, not mirrored
  object matrices, so skinned meshes and terrain reflect correctly.
- Include the sky (its own draw with the mirrored view) and terrain;
  exclude particles and first-person. Half resolution, FP16 once stage 3
  exists, R8G8B8A8 before that.
- Visibility: replaying main-view items misses objects only visible in the
  mirror (above the camera's view behind hills). Accept that first; if it
  shows, add a second cull pass through the engine's own scene culling.
- Water shader: sample the reflection with the per-pixel normal offset,
  Fresnel from the linear-space view angle, refraction tint by depth
  difference (INTZ) for shoreline softness.

## Stage 3: FP16 HDR scene target

- The world draws into an A16B16G16R16F target with the INTZ depth, from
  the world begin (`00a55f50` after-hook) to `renderList_end` (`00a401b0`,
  return `004a8ca2`); HUD and menus draw after, unchanged, into the back
  buffer. This part exists; stage 1 makes its contents linear.
- Light units: sun and sky are scaled so that the retail image, after
  stage 4's tone curve at the default exposure, matches native brightness.
  The sky mesh outputs its texture times a sky intensity, so the sun area
  can exceed 1.
- MSAA: FP16 MSAA is supported under DXVK; resolve with `StretchRect` from
  an MSAA surface. Keep the RPG mod's MSAA handling in one place.

## Stage 4: Filmic exposure and tone mapping

- Average log luminance from a downsampled chain (1/4 res → 1×1, R16F),
  adapted over time (separate up/down rates), clamped to a map-specific
  min/max so night maps stay dark.
- Tone curve: ACES fitted (or Hable) applied to linear radiance, then
  display gamma. Remove the current grade/vignette/grain from the default;
  keep them as an optional look.

## Stage 5: Bloom and height-based atmospheric fog

- Bloom: threshold-free energy-conserving mip chain (downsample 13-tap,
  upsample tent) at 1/2 → 1/32, mixed at a low weight before tone mapping.
- Fog: the stage-1 engine fog stays the base. Add an exponential height
  term (density falling with world height, from the reconstructed world
  position: INTZ depth + inverse view-projection) and sun in-scatter
  (Henyey-Greenstein towards the map's sun direction). The fog colour
  follows the map's fog colour; distances stay inside the map's fog end so
  gameplay visibility does not change.

## Stage 6: Lightmaps as indirect lighting and occlusion

- Retail lightmaps bake sun and sky together, including the sun's shadow.
  On lightmapped meshes and terrain: indirect = lightmap × albedo (scaled
  so the result matches native at default exposure); direct sun specular
  and any extra direct light are multiplied by the lightmap's sun
  visibility (its luminance relative to the map's fully lit level), so a
  surface shadowed in the bake never gets a sun highlight.
- When stage 7's shadow map exists, the direct visibility is
  min(lightmap visibility, shadow map), so dynamic objects shadow
  lightmapped ground without double-darkening already baked shadows.

## Stage 7: PBR only where the data exists

- Use real channels only: the RPG's authored DXT5nm/ORM; the retail RGB
  tangent normal maps with gloss in alpha; diffuse-alpha gloss masks.
  Roughness from gloss through one documented mapping
  (`roughness = sqrt(2/(specPower+2))`, the Blinn-Phong to GGX fit).
- Metallic only for materials listed in a material table (texture name -> roughness source, gloss
  channel, metallic; unknown materials keep the retail look) (authored or
  owner-approved); everything else dielectric with F0 0.04.
- Surfaces without such data keep the retail diffuse plus the existing
  specular, in linear space. The derived "relief from diffuse brightness"
  in the current shaders is removed by default (it is not measured data).

## Stage 8: Sun shadow maps, then SSAO

- Shadows: two or three cascades of a D24 depth texture (hardware PCF in
  D3D9/DXVK) fitted to the view frustum, texel-snapped to stop shimmering.
  Rendered in a dedicated pass before the world each frame from the
  recorded casters (the stage-2 mechanism) with the sun direction from
  `sky+0x3c`; this replaces the per-draw replay and removes the one-frame
  lag for static geometry. Casters: meshes, vehicles, soldiers; terrain
  receives only (lightmap covers terrain self-shadowing).
- Engine shadows: the retail projected shadows are disabled only where the
  shadow map covers them, so nothing is shadowed twice.
- SSAO: after shadows, a normal-aware horizon/GTAO-style pass at half
  resolution with a bilateral blur, applied only to ambient/indirect light
  (not to direct sun, not to the sky). Needs normals: the upgraded shaders
  write a view-space normal MRT; native draws fall back to normals from
  depth derivatives.

## Stage 9 (consider): SSR and temporal antialiasing

Start only once depth, normals and motion data exist:

- Motion vectors: previous-frame world-view-projection per object (the
  engine's per-item matrix at `item+8`, kept per item between frames) for
  rigid meshes; skinned meshes and particles first use camera-only motion.
- TAA: sub-pixel projection jitter (Halton), reprojection with motion
  vectors, neighbourhood colour clamping, history rejection on disocclusion.
  Without these the current history blend only smears, so it is removed in
  stage 1.
- SSR: hierarchical depth march along the reflected view ray using the
  normal MRT, roughness-limited, falling back to the planar water reflection
  and the sky cube. Mainly for wet/metal surfaces; water keeps stage 2.

## Order of work

Stages 1 → 9 in order; stage 2 can run beside 3–5 because it only adds a
pass. Stage 1 lands as its own build and A/B check before anything else,
since every later stage assumes linear colour, one fog and trustworthy depth.
