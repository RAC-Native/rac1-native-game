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
material <tfrag|tie|shrub|moby|surface> <class> tex <n> <type> [a] [b] [c] [tint r g b] [waves <scale> <speed>] [foam f] [depth d] [glow <strength> [threshold]]
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
| `grass` | height 0.05 (0.01..0.3) | density 18 (2..40) | bend 0.6 (0..2) | Options > Graphics > Grass, tfrag textures only: the blades ([below](#water-and-grass)) |

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
  programs; Vulkan: the lighting's uniform buffer (11776 bytes: the Water option's `uMat2` and the lights' aim `uPtAim` since 0.6.x; 8192 before, 5120 before that) and its descriptor set's bindings 2 (the
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

Their normal is the face's (the seas are mostly flat: the ripples and the glint do the rest). Rilgar, Pokitaru and Hoven ship such lines
for the Water option ([below](#water-and-grass)).

In the editor (since #102's batch 1: "couldn't target water ... as it is a particle") the seas are found along the view, not by the
depth the pick reads: they are drawn after it, so a click on the water read the bottom under it. The menu sends the ray from the
eye toward the picked point (`mrp_fx_surface_pick`); the next frames test the seas' triangles as they are built (`build_vu7`) and
keep the nearest hit, with its class and texture. The Textures tab lists every sea the level has drawn (`mrp_fx_surfaces_seen`).

## Water and grass

Two more options of Options > Graphics, each Off (the default), Low or High, each on the textures a level's file tags, each with
Lighting not Original only (`water`, `grass` in the preferences; items 52, 54 of a save's block; `RAC1_WATER`, `RAC1_GRASS` for
testing). They start from the game's own textures - their colours stay the texture's - and add
relief and motion; a texture pack replacing those textures later gets the same treatment. Nothing of the game's state changes (proved
with `RAC1_RAMHASH`: Novalis and Pokitaru with all of them on, OpenGL and Vulkan, the same hashes as with them off).

### Water

The `water` material (and `lava`, `quicksand`) with Water on, Surface Detail whatever it is:

- **Two procedural normal maps** (value noise with its gradient, two octaves each) scroll across each other over the texture, the game's
  own water animation kept under them. `waves <scale> <speed>` (1 1) sets their size and speed; `b` (ripples) their height.
- **The depth under the surface**: the 3D under the water is copied (colour and depth, as Surface Detail High's reflections do) right
  before the seas are drawn - with Water on the program-7 seas that write depth are drawn just after that copy, as the ambient particles
  already asked for - so a sea knows, per pixel, how much water lies under it. Shallow water is lighter and more transparent, deep
  water darker (toward `tint r g b` when given, else a deeper shade of the texture's own colour) and as opaque as the game draws it; how
  deep is "deep" is `depth d` (water 2.5 units). The water is never made more opaque than the game's own alpha: where the game drew it
  translucent it stays at most that.
- **Foam**: a thin, broken band where the water meets the shore (the depth near 0), `foam f` (water 0.6), white as bright as the place.
- **Fresnel and the sky's reflection**; the **sun's and the lamps' glints** (a sharp specular on the waves); with **High** the scene's
  reflection from the copy (the same ray march as the puddles') and a slight **refraction** of what is under the water (the copy read
  bent by the waves, only where what it shows lies behind the water).
- How it reaches the picture: a sea whose blending is the GS's alpha blend (0x44) or none is composited by our shader over the copy (its
  colour, the water's, over the bent 3D under it, with the game's alpha scaled by the depth); a sea with another blend keeps its own,
  its colour shaded and, when drawn before the copy, its alpha scaled. The world's water (tfrag, tie) draws the waves, the glints and the
  reflections only: it is in the copy itself, so it has no depth under it.
- **Lava** with Water on: the crust's relief lit by the sun from slow normal maps, a hotter seam where it meets the shore (`foam`), the
  tint; on High the heat shimmer. **Quicksand**: the slow normal maps under a dull sheen, a darker wet band at its edge (`foam`).

The additive overlays some seas draw over their water (Pokitaru's 13155 and 13223, Hoven's 13391: ALPHA 0x48 / 0x68, highlights) are
best left untagged: `RAC1_LIGHTS_TRACE=1` prints each sea's blend (`ABE 1 ALPHA 44`) and a point of it (`near x y z`).

### Grass

The `grass` material on a tfrag texture with Grass on. Kept small, as the tester Anya-Thoughtmane and Daino asked: closer to what a
detail or parallax map does to give the ground depth than to a field.

- **Low: no geometry.** The ground's texel is shaded by a field of blades (one per cell, `density` a unit across, a random offset and
  height): darker between them, lighter on their tips, the tufts turned by the sun - a detail normal map computed in the shader. Its mean
  brightness is the texture's.
- **High: a few very low shells** besides: the tagged tfrags drawn again 4 times (`RAC1_GRASS_SHELLS`, the Enhancements tab), each lifted
  k / 4 of `height` (0.05 units: a few centimetres), only the blades' pixels kept. The blades are Low's own (its first grid: the same
  cells, offsets, directions and heights, 80 % of them), so a shell grows out of a tuft the relief already shows and where the shells
  fade only the relief is left: no seam between the two. Each blade is narrow and tapers to its tip, its root the ground's colour and its
  tip lighter. The blades lean with the wind and away from Ratchet's feet (`bend`). Daino's playtest asked for something between the
  first High and Low: High was made subtler (the first one drew round, darker dots, 0.45 of a cell wide at the root), not a Medium added -
  with the shells sharing Low's tufts, one step between Low and High reads as a single option growing, and a fourth value would have
  needed a new item in the saves and the preferences for little difference.
- **Only where the grass is the ground seen.** A level paints its paths over the meadow: on Novalis the dirt path is another tfrag
  texture laid on the grass's own triangles (the same depth, drawn after, blended at its edges by the vertices' alpha). Lifted, the
  shells came through it (green dots on the dirt). Now, before the shells, the tfrags are drawn again in the world's order into the
  stencil alone (no colour, no depth, a little toward the camera to pass on their own depth): the grass's pixels set it, every other
  texture's pixels clear it - a blended one (the GS's usual alpha blend, ALPHA 0x44) with the probability of its alpha, dithered on the
  screen, so a path's soft edge thins the blades out instead of cutting them; an additive one hides nothing and leaves it. The shells
  are drawn only where it is set. The grass painted into the path's own texture is not the tagged texture: it gets neither the relief
  nor the shells.
- **Fading, not culling.** Both fade per pixel with the distance (`RAC1_GRASS_DIST`, 40 units; the shells from 30 % of it: shorter and
  fewer until none) and where a blade would be under a pixel or so (no shimmering; at 2560 x 1440 that is before the distance). The
  vertex shader drops only the vertices 16 units past the distance: a triangle reaching inside it is drawn whole (before, a vertex past
  the distance + 6 dropped its triangles: a straight edge could fall where blades still stood). The straight cut Daino saw on the right
  of the dirt path was the path texture's own edge: the dots showed only through the path (on the grass they have the grass's colour)
  and stopped where its triangles end - the same with the distance at 200. The stencil removed it with the dots. The pop-in while
  walking was the old fade, from 14 to 24 units, close enough to see.
- The shells are drawn after the world and before the occlusion: the sun's shadows, the point lights and the occlusion fall on them.
  The game's drop shadows (the volumes cut to a slab around the ground) do not reach the shells' tops. A shell's pixel without a blade
  is not lit (its lighting skipped before it is discarded), nor are the stencil's.

### Tagging (Anya)

As any material: the Lights tab's surface in hand (the crosshair, a click) or the Textures tab, the type in the combo (`grass`,
`water`), its sliders live; for water, lava and quicksand also the **Tint**, **Waves**, **Foam** and **Depth** of the Water
option. A warning says when the option that draws the type is Off. The debug menu's **Enhancements** tab switches every enhancement
live, with its profiler stage's time (Water: "Water (seas, depth copy)"; Grass: "Grass shells").

Seeded: Veldin (0) the plain's dry grass (tfrag texture 3); Novalis (1) the meadow's grass (tfrag 8); Kerwan (3) the lawn of the start's
garden (tfrag 23); Rilgar (5) the harbour (surface 879 tex 13040) and the
canals by the start (surface 831, 13052 and 13120); Pokitaru (11) the sea (surface 1111 tex 13135); Hoven (12) the water (surface 1901
tex 13323). Novalis's sea is not tagged yet: its program-7 surfaces near the start (13347, 13351) are the landing area's, not water.

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

### Water and grass

GPU time per frame from the profiler's stages (`RAC1_PROFILER_DUMP=120`, the last 120 frames; integrated Radeon 890M, Lighting Medium,
the widened view filling the window). Water: Pokitaru, Ratchet swimming among the deck's posts (the sea fills two thirds of the picture;
`RAC1_GOTO=11@60 RAC1_SIM_INPUT=shipskip RAC1_HERO_AT=-1@2400,495,430,231,1.98`, frame 2640): with Water Off the seas are drawn in
"Effects", with it on in "Water" (the seas and the copy of the 3D under them). Grass: Kerwan's garden at the start
(`RAC1_GOTO=3@60 RAC1_SIM_INPUT=shipskip`, frame 2640; the lawn fills half of the picture).

| | Vulkan 2560 x 1440 | Vulkan 5120 x 1440 | OpenGL 2560 x 1440 | OpenGL 5120 x 1440 |
|---|---|---|---|---|
| Seas, Water Off (Effects) | 2.76 | 4.40 | 3.96 | 4.02 |
| Water Low (Water + Effects) | 4.03 | 7.11 | 5.43 | 6.81 |
| Water High | 6.02 | 11.13 | 7.70 | 12.31 |
| Grass Low (World, against Off) | about +0.4 | within the noise | within the noise | within the noise |
| Grass High (Grass shells, 4), first version | 2.65 | 3.86 | 2.60 | 3.82 |
| Grass High, the stencil and 40 units (Grass shells) | 2.43 (2.13 before this change, measured alongside) | 4.30 (3.62) | | |
| Grass High on Novalis's start meadow (Grass shells) | 2.25 (2.28 before) | 3.20 (3.16) | 2.20 (2.22) | 3.65 (3.25) |

The machine was shared while measuring (the OpenGL numbers moved by a millisecond between runs). Water Low costs about 1.3 ms at
2560 x 1440 and 2.7 at 5120 x 1440 (the normal maps, the depth, the copy); High twice that more (the reflection's ray march over the
whole sea). The grass shells cost by the pixels of grass near the camera (each shell is shaded where it covers, its cut-outs discarded) and by
the tfrags' vertices, run again per shell (4 shells cost little less than 6 did: 3.2 ms). The stencil's two passes over the tfrags
near the camera and the range pushed from 24 to 40 units cost about as much as skipping the lighting of the shells' empty pixels saves:
on Novalis (`RAC1_GOTO=1@60 RAC1_SIM_INPUT=shipskip`, frame 2640, the meadow) the same as before within the noise, on Kerwan's lawn
+0.3 ms at 2560 x 1440 and +0.7 at 5120 x 1440 (Vulkan; the OpenGL numbers of the second measure moved by more than that).
