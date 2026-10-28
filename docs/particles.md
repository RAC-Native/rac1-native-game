# Ambient particles

Our own particles in the air: dust, ash, embers, fireflies, light dust in the sunlight, rain and snow. They are an
enhancement, purely visual, and **off by default** (Options > Graphics > Ambient Particles: Off, Low, High). Nothing of them
comes from the disc: the kinds, their looks and where they are are our art choices, written in the levels' light files.

> **Off is the original.** With the row on Off (the default) nothing is read, drawn or changed: the picture is the same pixel
> for pixel (the reference screenshots). On, they only change the picture, never the game: they read the camera, the hero's
> light set and the level's file, and write nothing the game reads, so the game's state is the same with them on or off
> (checked with `RAC1_RAMHASH`, below).

## What a level has

| Level | Particles | Notes |
|---|---|---|
| Rilgar | rain | a very light rain (Anya-Thoughtmane's experiment, #102 batch 1, after Ratchet & Clank 2016's Rilgar) |
| Umbris | embers | rising from the lava itself, only near it: the Snagglebeast's lava cave (Anya-Thoughtmane's suggestion; denser and slower since #102 batch 1) |
| Batalia | rain | slanted by the wind (with its gusts), splashes on the ground; drawn with the game's own rain (`keep`, since #102 batch 1) |
| Gaspar | embers, ash | rising from the lava sea itself, higher than Umbris's (it lies deep below the platforms); none indoors or away from the lava; a second, orange line of embers since #102 batch 1 |
| Orxon | ash, embers, snow | a dark green ash, green embers rising from the toxic pools, a little dark snow (Anya-Thoughtmane's, #102 batch 1; a green dust before) |
| Pokitaru | fireflies | brighter in the shade and at night |
| Hoven | snow, motes | a calm, light snowfall, a gentle steady drift; it replaces the game's own snow (below); faint motes beside it, ice glittering in the air (Anya-Thoughtmane's, #102 batch 1) |

The other levels have none: a line in their file adds some (the [editor](#the-editor)).

## The kinds

| Kind | Looks | Moves | Light |
|---|---|---|---|
| `dust` | small soft specks | drifting slowly, sinking a little | the zone's light |
| `ash` | grey flakes | falling slowly, drifting | the zone's light; none under a roof |
| `embers` | orange glows, flickering, each with a short life | rising, wandering | their own (added to the picture) |
| `fireflies` | yellow-green glows, blinking | wandering | their own, brighter where the zone is dark |
| `motes` | tiny bright specks | almost still | the zone's light, brighter toward the sun |
| `rain` | thin streaks along their fall, splashes on the ground near the camera | falling fast | the zone's light; none under a roof |
| `snow` | white flakes | falling slowly, drifting | the zone's light; none under a roof |

Low draws half the particles of High. A line's density scales the count again.

## How they are drawn

Each line of a level's file is a volume around the camera (a box some 30 to 40 units across). Its particles have fixed places
in the world and move with their kind's motion (the fall or the rise, the line's steady wind - gusts only if the line asks -, a
drift); a particle that leaves the box on one side comes back on the other, so the box always holds the same count however the
camera moves, and the particles do not move with the camera. Toward the box's faces they fade out, so the wrap is not seen.

The GPU makes every particle from its number alone (no buffers, nothing on the CPU per particle): one draw per line, after
the MRP's world, mobys and ambient occlusion, before the game's effects and the 2D. OpenGL and Vulkan draw them the same
(`src/game/render/mrp_particles.cpp`; the Vulkan shaders in `tools/gen_vk_shaders.py`, `MRP_PART_*`).

- **Against the scene**: each particle compares its depth with the scene's (a copy of the depth buffer on OpenGL, the depth
  buffer itself on Vulkan): hidden behind walls, and faded where it nears the surface behind it (soft particles: no hard cut
  where a flake meets the ground).
- **Light**: the zone's own (the two lights of the light sets the game lights Ratchet with, as the [modern lighting's sun](lighting.md#the-sun)
  reads them), so they are dim in a dark zone and at night. Embers and fireflies are their own light.
- **Under a roof**: rain, snow and ash fall only from the open sky ([below](#under-a-roof)). The level's interior boxes
  (`interior` lines, [Interiors](lighting.md#interiors)) keep them out as well: the particles inside the boxes nearest the
  camera (four at most) are not drawn, and the rain's splashes stop there.
- **Rain**: each drop a streak along its motion as the camera sees it (the camera's own motion taken off), thin (at least a
  pixel wide, lighter instead of thinner). Its splashes are a pass over the scene's upward surfaces within 18 units of the
  camera: in each cell of a grid on the ground (0.55 units) a ring now and then.
- **Snow**: round soft flakes, falling slowly, drifting gently in the wind.
- **From the lava** (`source lava`): see [below](#from-the-lava).
- **Time**: the game frame shown, so they stand still while the game does, and are the same in every run of a test.

## Under a roof

Rain, snow and ash (not ash rising from the lava) are not drawn under a roof, a tunnel's ceiling, a room's, a bridge or a
ledge, and the rain's splashes only where the ground itself is under the sky; with Surface Detail on, the
[materials'](materials.md) rain rings in puddles and water stop there too. No box needs placing: the "roof" is the level's
collision seen from above. (Before, Batalia's covered hall by the tracked robot had rain and splashes inside, and Hoven's rooms
snow.)

- **The sky grid**: once a level has run 30 frames with its collision unchanged (as for the lava), the collision tree the game
  loaded (`*(0x173e40)`, read only) is read on a thread of its own: every face that is not a wall (its normal at least a tenth
  up or down) is drawn into a grid of 0.5-unit squares over the whole level, each square keeping the height of the topmost face
  over it (Batalia about 35 ms, Hoven about 75 ms, 1.5 MB). Only levels whose file has a rain, snow or ash line are read
  (Rilgar, Batalia, Orxon, Hoven), or `rain on` while Surface Detail and the modern lighting are on (Batalia); a grid read from another level's
  collision (while a level loads) is never used.
- **The test**: a window of the grid around the camera (256 squares a side, 128 units) is a texture the particles read, taken
  again when the camera has moved 24 units. A particle below the topmost face at its place (blended over the grid's four
  nearest squares, softened half a unit up and down) is under cover and not drawn, so flakes fade out at a roof's edge, a door
  or a hole in a ceiling rather than stopping on a line. A splash is drawn where its ground is the topmost face (within 0.75
  units). The materials read the same window (the shadow pass takes it again when needed): their rain rings fade out where a
  splash would.
- Open ground is not changed: there the topmost face is the ground itself, below every particle.
- `RAC1_PARTICLES_SKY=0` (or the Sun tab's "Keep rain / snow / ash out from under roofs") turns the test off, for comparing.

## From the lava

A line with `source lava` does not fill the camera's volume: its particles rise from the level's lava itself, so a level's lava
fields and arenas have them and its other places (indoors, away from the lava) none.

- **Where the lava is**: the level's collision, the faces the game kills the hero in (surface kind 1, its deadly liquid,
  `hero_surface_update`'s +0x12e5): their tops, and only where no floor is above them (on Gaspar the lava is one plane under the
  whole level, its hills and platforms included; under a cave's roof, as Umbris's arena, it stays). Once a level has run 30
  frames with its collision unchanged, the collision tree the game loaded (`*(0x173e40)`, read only) is read on a thread of its
  own (Umbris about 20 ms, Gaspar about 60 ms) and its lava gets a point every 1.5 square units, spread by area with a fixed seed:
  the same points in every run. Only levels whose file has a `source lava` line are read.
- **Which lava**: the levels' kind 1 faces, in the NTSC game: Umbris (the Snagglebeast's arena and its cave, 10 700 square units),
  Gaspar (the lava sea under the level, 168 000), and the green liquids of Blarg, Orxon and Drek's fleet, which are not lava (Orxon's
  green embers rise from its toxic pools this way; Blarg and Drek's fleet have no line). The other levels have none (Aridia's quicksand is kind 3).
- **Near the camera**: each frame up to 512 of the points within 60 units are the draw's slots, the nearer the more (a point is
  taken with a chance falling with the square of its distance, about as many on screen near as far; behind the camera a point
  counts as twice as far). A slot keeps its point while it stays taken; a point no longer taken lets its particles fade out over
  1.5 seconds before the slot takes another, whose particles fade in: nothing jumps or pops.
- **The motion**: each particle's life (8 to 14 seconds) starts at one of the slots' points, a little around it, and rises slowly
  (embers 0.9 units a second, ash 0.5, or the line's `rise`), carried by the air's slow currents (a calm field that changes over
  the level and over minutes, the same for the particles of one place: they drift together, like wisps) and the line's wind,
  wandering a little; born bright, it fades in the second half of its life, and toward 60 units from the camera.
- The particles drawn follow the slots in use: with little lava near, few of the line's count are drawn.

## Rain and snow in place of the game's

Batalia's rain is the game's own particles (types 0 and 1 of its particle pool, drawn by its particle program); Hoven has its
own snow. Where a level's `rain` or `snow` line is on (and not marked `keep`) and the option is on, the game's effect is
**not drawn**: the MRP's effects pass (`mrp_fx_draw`) skips its primitives. Only the drawing: the game still makes and updates
its particles, so its state is the same (`RAC1_RAMHASH`). `RAC1_PARTICLES_GAME=1`, or the debug menu's checkbox, draws the
game's as well (for comparing). With the MRP's effects off (`RAC1_MRP_FX=0`: the GS replica draws them) the game's are drawn.

## A level's lines

In the level's light file, `data/lights/levelNN.txt` (see [The light editor](lighting.md#the-light-editor)), one line a volume:

```
particles <kind> <density> [r g b] [wind <x> <y>] [gust <g>] [box x0 y0 z0 x1 y1 z1] [source lava] [rise <r>] [keep]
```

- `kind`: `dust`, `ash`, `embers`, `fireflies`, `motes`, `rain`, `snow`.
- `density`: 1 the kind's own count, 0 to 4.
- `r g b`: a colour of its own (0 to 1), else the kind's.
- `wind x y`: units a second along the world's x and y, steady.
- `gust g`: the wind's gusts, its speed varying slowly by a quarter times `g` (0, the default: none; Batalia's rain has 1).
- `box`: the particles only inside this world box (a lava field, an arena), faded over a unit at its faces.
- `source lava`: rising from the level's lava ([above](#from-the-lava)), instead of filling the camera's volume.
- `rise r`: from the lava, the rise in units a second (else the kind's: embers 0.9, ash 0.5).
- `keep`: for rain and snow, the game's own drawn as well.

The file is read the first time the level's particles, lighting or the debug menu need it (also with the modern lighting off). Saving keeps the lines where they were, and a
line not edited is written back exactly as it was read.

## The editor

The debug menu's Particles tab (F1, with the debug tools; until #102's batch 1 at the bottom of the Sun tab) has the level's particle
lines: each line's kind, density, colour, wind, gusts, "from the lava" and its rise, box ("around the camera" makes one 40 x 40 x 12
units), `keep`, an on switch and remove; "Add a particles line"; Save (the whole level file, as the other tabs' Save). **Solo** (a
button per line, by its number and kind) draws that line alone while it is on, to tell the lines apart on screen; "All" (or Save,
which ends the solo first and writes every line as it was) gives the others back. Every change shows at once. The seas (water,
lava, sand) are surfaces, not particles: their materials are given in the Lights tab or the Textures tab ([Materials](materials.md#the-levels-seas-and-pools)). The tab also sets the option for
the run (Off, Low, High), draws the game's rain and snow as well, and shows the particles drawn and the CPU time; the GPU's is
the Profiler tab's "Ambient particles" stage.

## Testing

- `RAC1_PARTICLES=0|1|2`: the option (Off, Low, High) whatever the preferences.
- `RAC1_PARTICLES_GAME=1`: the game's own rain and snow drawn as well.
- `RAC1_PARTICLES_TRACE=1`: every 120 frames what is drawn (draws, particles, the light, the game's rain or snow hidden, the
  camera under a roof or the sky); each level's lava as read (faces, square units, points, the time; the collision's faces by
  surface kind; the lava's heights) and its sky grid (squares, faces, the time).
- `RAC1_PARTICLES_SKY=0`: no test against the roofs ([Under a roof](#under-a-roof)), as before.
- `RAC1_PARTICLES_FXTRACE=1`: the game's effect primitives by texture, and its particle pool by type (how the game's rain was
  found).
- The cost: `RAC1_PROFILER_DUMP` (the "Ambient particles" stage's CPU and GPU time).

## Cost

At High, 960 x 720, on an integrated Radeon 890M (the debug menu's Profiler stage "Ambient particles", averaged over 300 frames
of a walk from each level's start; `RAC1_PROFILER_DUMP`):

| Level | OpenGL CPU | OpenGL GPU | Vulkan CPU | Vulkan GPU |
|---|---|---|---|---|
| Kerwan (motes) | 0.03 ms | 0.13 ms | 0.01 ms | 0.03 ms |
| Umbris (embers from the lava, in the Snagglebeast's arena) | 0.04 ms | 0.14 ms | 0.02 ms | 0.02 ms |
| Batalia (rain, splashes) | 0.02 ms | 0.22 ms | 0.01 ms | 0.15 ms |
| Gaspar (embers, ash from the lava) | 0.07 ms | 0.12 ms | 0.04 ms | 0.03 ms |
| Orxon (dust) | 0.07 ms | 0.14 ms | 0.01 ms | 0.03 ms |
| Pokitaru (fireflies) | 0.04 ms | 0.14 ms | 0.01 ms | 0.02 ms |
| Hoven (snow) | 0.02 ms | 0.12 ms | 0.01 ms | 0.02 ms |

OpenGL's GPU time includes the copy of the depth buffer (about 0.1 ms); Batalia's the splashes' screen pass.

The test against the roofs costs nothing measurable (300 frames in Batalia's covered hall and Hoven's room, test off / on):
OpenGL CPU 0.06 / 0.05 ms and 0.05 / 0.06 ms, GPU 0.25 / 0.25 ms and 0.17 / 0.15 ms; Vulkan CPU 0.02 / 0.04 ms and 0.01 / 0.03 ms,
GPU 0.09 / 0.07 ms and 0.02 / 0.02 ms. The grid is read once per level on its own thread; the window (256 KB) is uploaded again
every 24 units the camera moves. The lava's CPU time
is mostly the choice of its slots (Gaspar: some thousand points within 60 units); Umbris, Batalia, Gaspar and Hoven measured
after the playtest's changes, from the level's start (Umbris: placed in the arena).

## Limits

- Under a roof is what the collision says: a roof the game gives no collision (out of reach, a tree's leaves) lets rain
  through, and an invisible ceiling of collision would keep it out (none seen on Batalia or Hoven). Mobys (a ship, a moving
  platform) are not in it. An `interior` box can still keep the particles out of a place the collision misses.
- The game's rain and snow are told from its other particles by their particle types (0 and 1 on every level but Hoven,
  0x49 on Hoven) and the textures those had this frame: another particle sharing one of their textures in the same frame
  would be hidden too.
- From the lava: lava the game draws but its collision does not mark as kind 1 (a lava fall, lava out of reach) has none, and the
  particles rise from the collision's surface, which may lie a little off the lava drawn. They are not hidden by what stands
  between them and the camera beyond the scene's depth (a platform's edge hides those below it, as it should).
