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
material <tfrag|tie|shrub|moby|surface> <class> tex <n> <type> [a] [b] [c] [glow <strength> [threshold]]
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
| `metal` | roughness 0.35 (0.03..1) | strength 1 (0..2) | | roughness 0 is a polished mirror, 1 brushed. Blinn-Phong specular of the sun (where it is not in shadow) and of the point lights, energy-normalised, with an exponent from a GGX-like roughness (a small, very bright highlight when polished, a broad dull one when rough); Schlick Fresnel with the texture as the metal's colour; a mirror of the level's sky over the ground's shade, its horizon sharper (up to four times) and its share larger the lower the roughness (up to 1.5 times the Fresnel), as bright as the place is lit and dimmed to 35 % where the sun does not reach (its shadow, the faces turned from it); on High, below roughness 0.45 and not on a moby, the last frame's picture along the reflected ray, blurred above roughness 0.05 and fully mixed in only toward it; the painted colour dimmed by 25 % (rough) to 55 % (polished) |
| `wet` | wetness 0.6 (0..1) | roughness 0.45 (0.05..1) | | darker (up to 40 %), a broad glossy specular, a little sky |
| `puddle` | coverage 0.4 (0..1) | scale 0.3 (0.02..4) | wetness 0.6 (0..1) | `wet`, and on the flat ground facing up (normal within some 15 degrees of up) puddles where a world-space noise (its frequency `scale` per unit) is above 1 - coverage: they stay in place whatever the camera does, the same with OpenGL and Vulkan on any GPU (an integer noise). In a puddle: dark, a near mirror (Fresnel from 15 % looking down to 90 % at grazing), small waves; rain rings when the level rains |
| `lava` | strength 1.3 (0..4) | pulse 0.15 (0..1) | threshold 0 (0..1) | emissive (the original's colour x strength, as an emissive surface; texels whose brightest channel is under the threshold lit as before), a slow brightness pulse whose phase changes across the world, the texture pushed slowly to and fro; High: the heat shimmer |
| `water` | strength 1 (0..2) | ripples 1 (0..3) | | a sharp sun / lamp glint, Fresnel, procedural ripples (three crossing wave trains and a fine noise, animated) and, when it rains, rain rings; the reflection is the sky (Low) or the last frame's picture (High). Added on top of the game's water: its scrolling, its waves and its tints are its own |
| `mud` | wetness 0.5 (0..1) | drift 1 (0..3) | | a wet glossy sheen and a very slow, viscous drift of the texture |
| `quicksand` | drift 1 (0..3) | gloss 0.3 (0..1) | | the drift, a dull sheen |

- `rain on` (default off): the puddles' and the water's rain rings on this level, only under the open sky (none under a roof:
  the ambient particles' sky grid, [Under a roof](particles.md#under-a-roof)). Batalia's is on.
- `sky r g b` (0..2; default 0.52 0.58 0.68): the colour a reflection shows where it finds nothing, and the
  metals' sheen. The ground's side of it is the same colour at 30 %.

A texture is either emissive or one material: one line per texture. A material (not lava, emissive by itself) can **glow** besides:
`glow <strength> [threshold]` at the end of its line is an [emissive](lighting.md#emissive-surfaces) strength and threshold on the same
texture, applied after the material (a metal lamp whose bulb is painted in the same texture; Anya-Thoughtmane's "metal objects can't
also be emissive", #102 batch 1). A level's file or `classes.txt` with an `emissive` line and a `material` line for the same texture
is read as one entry, the glowing material (before, the later line's slot won and the other was not drawn); the editor's Save
writes it as one line. The glow is drawn with Surface Detail Off too (the glow alone, as an emissive surface). Emissive surfaces and
materials share the level's 126 slots, in the file's order, the level's own lines first and then the `classes.txt` ones (30 slots
until #102's batch 1: with batch 1's lines for every level the slots were full on every level and the lines past the 30th were
dropped - "metal rendering just gives up after a set number of marked metals"; Novalis had 71 lines, Rilgar 73).

### How to find a texture (for Anya)

In the debug menu's Lights tab, "Emissive surfaces and materials": aim the crosshair at the surface
(the line under it says its kind, class and texture, the level's texture too); tick Preview to see which
texture pulses magenta and step with the arrows if the class has several; pick a type in the combo and
press "Give the material"; then the type and its parameters act live, and **Glow** / **Glow threshold** give the
material an emissive glow besides; Surface Detail and "Rain on this level" are there too. A sea (water, lava, sand: below) is
found along the view: aimed or clicked at, the tab says "a sea: surface <class> tex <n>" and follows it (or **Take the sea**); the
Textures tab lists the seas the level has drawn ("Seas drawn") above its grid. Save writes `material` lines with the rest of the file, in place. A headless run
finds them too: `RAC1_PICK_SCREEN=u,v;u,v;...` prints the surface under each point of the picture, and
`RAC1_LIGHTS_TRACE=1` lists every texture of the level with its mean colour and the classes using it (the
red ones for lava, and so on).

## How it is drawn

- **The slot.** `emissive_refresh` / `moby_emissive_refresh` give each listed texture a slot (1 to 126),
  carried in the world draw's texture word (bits 25..31), in the mobys' draw command's fourth word (until #102's batch 1 their
  texture word's bits 27..31: five bits, 30 slots) and in the seas' vertices, as for the emissive surfaces. With Surface Detail
  Off a material gets no slot (those draws are the original's), unless it glows.
- **The uniforms.** Per slot `uEmis[s]` (strength, threshold - a material's glow -, the type in z) and `uMat[s]` (a, b, c with
  the defaults filled in), 128 of each (127 the editor's preview); `uMat[0]` holds Surface Detail, the clock (frames / 60, so a headless run is
  deterministic), the rain and whether the last frame's picture is ready. OpenGL: uniforms of the lighting's
  programs; Vulkan: the lighting's uniform buffer (8192 bytes; 5120 before) and its descriptor set's bindings 2 (the
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
  puddle, water, lava or a polished metal (roughness under 0.45; 0.6 before #102's batch 1) and Surface Detail is High. The next frame's puddles and water march their
  reflected ray through that depth in world space (20 steps, growing), reprojected with the old view; a hit
  shows the old picture there (faded near its edges), a miss the sky colour. The lava's shimmer reads the old
  picture around the pixel, displaced by a rising noise. The copy is used only the frame right after it.

### Metal's roughness

Before 0.6.0 a metal's roughness seemed reversed (#101): low values gave a highlight so narrow that, on the flat faces of the PS2's
models, it was hardly ever seen, and the sky's sheen was the same whatever the roughness, so a "polished" metal looked no shinier
than a rough one (Veldin's start with its ground as metal at 0.05, 0.5 and 0.9: three nearly equal pictures, 0.9 a little brighter).
The sheen is now a mirror whose sharpness and strength follow the roughness: at 0.05 the ground mirrors the sky and, on High, the
cliffs and the ship; at 0.5 a soft sheen; at 0.9 a dull, brushed look.

In #102's batch 1 Anya-Thoughtmane found that "metal rendering freaks out on reflecting lower than 0.40 roughness in some levels",
and that metals "stay reflective in the shadow". Below 0.4 the last frame's picture came in almost whole (from 0.6 down, all of it
from 0.25), sharp, with a loose test for a hit (a ray passing up to three times 0.3 + 5 % of its length behind an object counted):
on Veldin's ground made metal 0.30 the cliffs' reflection was crisp and changed shape from frame to frame as the camera moved, and
on Aridia the landing pad (0.34 and 0.43) showed its surroundings as a lake. On a moby the old picture holds the moby itself a frame
earlier (a spinning bolt, metal 0.35 in `classes.txt`, would mirror its own last pose). Now: the picture only below roughness 0.45,
mixed in from nothing at 0.45 to all of it at 0.05 and blurred above 0.05 (four more taps, up to about 1 % of the picture apart); a
hit only within 0.15 + 2 % of the ray's length of the old surface, faded out by twice that (the puddles and the water keep the old
test); never on a moby (its sky and ground mirror alone); the sky's horizon at most four times sharper (six before: whole flat
faces flipped between sky and ground). Where the sun does not reach - its shadow, and the faces turned from it - the mirror (the
sky's and the picture's) is dimmed to 35 % (the sun's highlight was already hidden there; the ambient occlusion, drawn after,
darkens all of it).

## Shipped

- **Batalia (8)**, the pilot, `rain on`, a stormy sky: the landing platform's steel deck (tie 391 `tex 2`, the
  level's texture 108), the railings' posts (tie 338 `tex 2`) and the military buildings' walls (tie 297
  `tex 5`) as metal; the paved ground of the base below the platform (tfrag texture 11) as puddles.
- **Umbris (7)**: the swampy ground where the walk starts (tfrag texture 9) as mud.

## The levels' seas and pools

The levels' large water, lava and sand-sea surfaces are VU1 program 7's (Novalis's and Rilgar's water, Aridia's sand sea, Gaspar's
lava sea, Hoven's water...), drawn by the MRP from the program's input ([Rendering](rendering.md#vu1-program-7s-surfaces)). They
take a material as a `surface` line, named as an `emissive surface` line names them (see
[Emissive surfaces](lighting.md#emissive-surfaces)): `RAC1_LIGHTS_TRACE=1` prints each one with its class and texture, e.g.

```
material surface 879 tex 13040 water    # Rilgar's harbour
material surface 317 tex 12811 lava     # Gaspar's lava sea
```

Their normal is the face's (the seas are mostly flat: the ripples and the glint do the rest). No level ships such a line yet.

In the editor (since #102's batch 1: "couldn't target water ... as it is a particle") the seas are found along the view, not by the
depth the pick reads: they are drawn after it, so a click on the water read the bottom under it. The menu sends the ray from the
eye toward the picked point (`mrp_fx_surface_pick`); the next frames test the seas' triangles as they are built (`build_vu7`) and
keep the nearest hit, with its class and texture. The Textures tab lists every sea the level has drawn (`mrp_fx_surfaces_seen`).

## Limits

- Normals are the faces' (flat): a highlight is one facet's, coarse on round low-polygon objects.
- The reflections are of the last frame (one frame late when the camera turns fast) and of what is on
  screen; off it, the sky colour.
- No reflections of the effects and particles drawn after the world (the copy is taken before them).
- At most 126 emissive surfaces and materials a level, the `classes.txt` lines included (the log says so when there are more).
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
