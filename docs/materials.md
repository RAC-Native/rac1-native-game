# Materials

With the [modern lighting](lighting.md) on, a level's light file can give a texture a **material**: the
texture as the PS2 painted it is kept, and the material lights and animates it. Metal catches the sun,
the lamps and the sky at grazing angles; wet ground darkens and shines; puddles mirror the scene and
ripple in the rain; water gets a sun glint, Fresnel and moving ripples over the game's own water
animation; lava glows, pulses and flows slowly; mud and quicksand drift. It is our art, data only: the
game's state is never touched, and with Surface Detail Off (the default) nothing of it is drawn.

The code: the materials' list and the shaders' uniforms in `src/game/render/mrp_lighting.cpp`, the
GLSL in `src/game/render/mrp_materials_glsl.inc` (one text for OpenGL and, read by
`tools/gen_vk_shaders.py`, for Vulkan), the slots in the world's and the mobys' draws in
`mrp_world_renderer.cpp` and `mrp_moby_renderer.cpp`.

## Surface Detail

Options > Graphics > Surface Detail (`surface_detail` in the preferences, item 51 of a save's block):

| Value | What is drawn |
|---|---|
| Off (default) | nothing: the tagged textures are drawn as before (emissive surfaces still glow) |
| Low | every material: specular, wetness, puddles, ripples, rain, the lava's pulse, the slow flow |
| High | Low, plus the reflections of puddles and water taken from the last frame's picture, and the lava's heat shimmer |

It works only with Lighting not Original, like the rest of the modern lighting. `RAC1_SURFACE_DETAIL=0|1|2`
(testing) overrides the row.

## Tagging a texture

In the level's light file (`data/lights/level<N>.txt`):

```
material <tfrag|tie|shrub|moby> <class> tex <n> <type> [a] [b] [c]
rain on
sky <r> <g> <b>
```

- `<kind> <class> tex <n>` names the texture exactly as an `emissive` line does (see
  [Emissive surfaces](lighting.md#emissive-surfaces)): a tie's texture prim, a shrub's or a moby's local
  texture, a tfrag's level texture (`tfrag 0 tex <n>`). A tie's or a shrub's texture is the level's,
  so every class that uses it gets the material; a moby's is its class's alone.
- `<type>` and its parameters (left out: the default; each is clamped to its range):

| Type | a | b | c | What it does |
|---|---|---|---|---|
| `emissive` | strength 1 (0..4) | threshold 0 (0..1) | | the same as an `emissive` line (Save writes it as one) |
| `metal` | roughness 0.35 (0.03..1) | strength 1 (0..2) | | Blinn-Phong specular of the sun (where it is not in shadow) and of the point lights, energy-normalised, with an exponent from a GGX-like roughness; Schlick Fresnel with the texture as the metal's colour; the level's sky in the sheen at grazing angles, as bright as the place is lit |
| `wet` | wetness 0.6 (0..1) | roughness 0.45 (0.05..1) | | darker (up to 40 %), a broad glossy specular, a little sky |
| `puddle` | coverage 0.4 (0..1) | scale 0.3 (0.02..4) | wetness 0.6 (0..1) | `wet`, and on the flat ground facing up (normal within some 15 degrees of up) puddles where a world-space noise (its frequency `scale` per unit) is above 1 - coverage: they stay in place whatever the camera does. In a puddle: dark, a near mirror (Fresnel from 15 % looking down to 90 % at grazing), small waves; rain rings when the level rains |
| `lava` | strength 1.3 (0..4) | pulse 0.15 (0..1) | threshold 0 (0..1) | emissive (the original's colour x strength, as an emissive surface; texels whose brightest channel is under the threshold lit as before), a slow brightness pulse whose phase changes across the world, the texture pushed slowly to and fro; High: the heat shimmer |
| `water` | strength 1 (0..2) | ripples 1 (0..3) | | a sharp sun / lamp glint, Fresnel, procedural ripples (three crossing wave trains and a fine noise, animated) and, when it rains, rain rings; the reflection is the sky (Low) or the last frame's picture (High). Added on top of the game's water: its scrolling, its waves and its tints are its own |
| `mud` | wetness 0.5 (0..1) | drift 1 (0..3) | | a wet glossy sheen and a very slow, viscous drift of the texture |
| `quicksand` | drift 1 (0..3) | gloss 0.3 (0..1) | | the drift, a dull sheen |

- `rain on` (default off): the puddles' and the water's rain rings on this level, only under the open sky (none under a roof:
  the ambient particles' sky grid, [Under a roof](particles.md#under-a-roof)). Batalia's is on.
- `sky r g b` (0..2; default 0.52 0.58 0.68): the colour a reflection shows where it finds nothing, and the
  metals' sheen. The ground's side of it is the same colour at 30 %.

A texture is either emissive or one material: one line per texture. Emissive surfaces and materials share
the level's 30 slots, in the file's order.

### How to find a texture (for Anya)

In the debug menu's Lights tab, "Emissive surfaces and materials": aim the crosshair at the surface
(the line under it says its kind, class and texture, the level's texture too); tick Preview to see which
texture pulses magenta and step with the arrows if the class has several; pick a type in the combo and
press "Give the material"; then the type and its parameters act live, Surface Detail and "Rain on this
level" are there too. Save writes `material` lines with the rest of the file, in place. A headless run
finds them too: `RAC1_PICK_SCREEN=u,v;u,v;...` prints the surface under each point of the picture, and
`RAC1_LIGHTS_TRACE=1` lists every texture of the level with its mean colour and the classes using it (the
red ones for lava, and so on).

## How it is drawn

- **The slot.** `emissive_refresh` / `moby_emissive_refresh` give each listed texture a slot (1 to 30),
  carried in the draw's texture word (the world's bits 25..29, the mobys' 27..31) as for the emissive
  surfaces. With Surface Detail Off a material gets no slot: those draws are the original's.
- **The uniforms.** Per slot `uEmis[s]` (strength, threshold, the type in z) and `uMat[s]` (a, b, c with the
  defaults filled in); `uMat[0]` holds Surface Detail, the clock (frames / 60, so a headless run is
  deterministic), the rain and whether the last frame's picture is ready. OpenGL: uniforms of the lighting's
  programs; Vulkan: the lighting's uniform buffer (5120 bytes) and its descriptor set's bindings 2 (the
  picture) and 5 (its depth). The rain rings' sky (`sm_sky`): the ambient particles' sky grid window, an R32F
  texture whose last row holds its place (OpenGL unit 30, Vulkan binding 6; none: open sky everywhere).
- **The shading** (`sh_material`, after `sh_emissive`, in the world's `SH_POINTS` and the mobys' `SH_SPLIT`
  variants). The normal is the face normal the lighting already has (flat on each triangle); ripples and rain
  bend it. The texel (x 255 / 128) is the albedo; the lit vertex colour is how much light the place has, and
  scales every reflection (a puddle in a dark room stays dark). The flow of lava, mud and quicksand moves the
  texture coordinates (`sh_mat_uv`) by two broad waves across the world, bounded (a few texels): a
  texture's clamping is never reached.
- **High: the last frame's picture.** After the world, the mobys, the ambient occlusion and the sun rays
  (`mrp_lighting_material_capture`, at the end of `mrp_lighting_ao_pass`), the 3D's colour and depth are copied
  (OpenGL: a blit; Vulkan: an image copy), with the view they were drawn with, only when the level has a
  puddle, water or lava and Surface Detail is High. The next frame's puddles and water march their
  reflected ray through that depth in world space (20 steps, growing), reprojected with the old view; a hit
  shows the old picture there (faded near its edges), a miss the sky colour. The lava's shimmer reads the old
  picture around the pixel, displaced by a rising noise. The copy is used only the frame right after it.

## Shipped

- **Batalia (8)**, the pilot, `rain on`, a stormy sky: the landing platform's steel deck (tie 391 `tex 2`, the
  level's texture 108), the railings' posts (tie 338 `tex 2`) and the military buildings' walls (tie 297
  `tex 5`) as metal; the paved ground of the base below the platform (tfrag texture 11) as puddles.
- **Umbris (7)**: the swampy ground where the walk starts (tfrag texture 9) as mud.

## What is not drawn by the world

The materials apply to what the MRP's world (tfrag, tie, shrub) and mobys draw. The levels' large seas
and pools are drawn otherwise: Novalis's water and Aridia's sand sea are the game's VU1 program 7 prim
lists, drawn by the MRP's effects (`mrp_fx.h`); Gaspar's lava sea, Batalia's sea and its waterfall are
not in the world's depth either (drawn after it, by the effects or the GS replica). A material line on
their textures does nothing, as none of their textures is in the world's tables. The `lava` and `water`
types work on any world or moby texture, but no level is tagged with them yet: Gaspar's lava, Novalis's
and Rilgar's water need the effects' and the replica's 3D drawn with the lighting's variant first (a
separate project).

## Limits

- Normals are the faces' (flat): a highlight is one facet's, coarse on round low-polygon objects.
- The reflections are of the last frame (one frame late when the camera turns fast) and of what is on
  screen; off it, the sky colour.
- No reflections of the effects and particles drawn after the world (the copy is taken before them).
- At most 30 emissive surfaces and materials a level.
- The heat shimmer moves the picture on the lava itself (and a few pixels around its edge), not the air
  above it.

## Performance

GPU time per frame (`RAC1_FRAME_PROF=2`, the profiler's "gpu ms/frame", the mean of its last four reports;
960 x 720, Lighting on, integrated Radeon 890M), standing at the level's start:

| Place | Off | Low | High |
|---|---|---|---|
| Batalia, the start (the platform's metal, the ground's puddles far below) | 5.62 | 5.61 | 6.58 |
| Batalia, the base's ground (puddles over a fifth of the picture; `RAC1_VIEW_AT=196,186,42,214,193,36`) | 4.64 | 4.08 | 5.01 |
| Umbris, the start (the mud) | 6.56 | 6.51 | 6.79 |

The machine was shared with other GPU work while measuring: the numbers move by about half a millisecond between
runs (Low below Off is that noise). Low costs nothing measurable; High about 0.4 to 1 ms, most of it the copies of
the picture (colour and depth at the 3D's resolution) and the reflections' 20 steps where puddles fill the screen.
