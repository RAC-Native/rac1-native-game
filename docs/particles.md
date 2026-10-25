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
| Kerwan | motes (light dust) | brighter where the view looks toward the sun: the sunbeams through the streets |
| Umbris | embers | rising from the lava (Anya-Thoughtmane's suggestion for the Snagglebeast's arena) |
| Batalia | rain, a little ash | the rain slanted by the wind, splashes on the ground; it replaces the game's own rain (below) |
| Gaspar | embers, ash | the volcano's |
| Orxon | dust | the toxic air's, a little green |
| Pokitaru | fireflies | brighter in the shade and at night |
| Hoven | snow | drifting flakes; it replaces the game's own snow (below) |

The other levels have none: a line in their file adds some (the [editor](#the-editor)).

## The kinds

| Kind | Looks | Moves | Light |
|---|---|---|---|
| `dust` | small soft specks | drifting slowly, sinking a little | the zone's light |
| `ash` | grey flakes | falling slowly, drifting | the zone's light; none inside interiors |
| `embers` | orange glows, flickering, each with a short life | rising, wandering | their own (added to the picture) |
| `fireflies` | yellow-green glows, blinking | wandering | their own, brighter where the zone is dark |
| `motes` | tiny bright specks | almost still | the zone's light, brighter toward the sun |
| `rain` | thin streaks along their fall, splashes on the ground near the camera | falling fast | the zone's light; none inside interiors |
| `snow` | white flakes | falling slowly, drifting | the zone's light; none inside interiors |

Low draws half the particles of High. A line's density scales the count again.

## How they are drawn

Each line of a level's file is a volume around the camera (a box some 30 to 40 units across). Its particles have fixed places
in the world and move with their kind's motion (the fall or the rise, the line's wind with a slow gust, a drift); a particle
that leaves the box on one side comes back on the other, so the box always holds the same count however the camera moves, and
the particles do not move with the camera. Toward the box's faces they fade out, so the wrap is not seen.

The GPU makes every particle from its number alone (no buffers, nothing on the CPU per particle): one draw per line, after
the MRP's world, mobys and ambient occlusion, before the game's effects and the 2D. OpenGL and Vulkan draw them the same
(`src/game/render/mrp_particles.cpp`; the Vulkan shaders in `tools/gen_vk_shaders.py`, `MRP_PART_*`).

- **Against the scene**: each particle compares its depth with the scene's (a copy of the depth buffer on OpenGL, the depth
  buffer itself on Vulkan): hidden behind walls, and faded where it nears the surface behind it (soft particles: no hard cut
  where a flake meets the ground).
- **Light**: the zone's own (the two lights of the light sets the game lights Ratchet with, as the [modern lighting's sun](lighting.md#the-sun)
  reads them), so they are dim in a dark zone and at night. Embers and fireflies are their own light.
- **Interiors**: the level's interior boxes (`interior` lines, [Interiors](lighting.md#interiors)) hold no rain, snow or ash:
  the particles inside the boxes nearest the camera (four at most) are not drawn, and the rain's splashes stop there.
- **Rain**: each drop a streak along its motion as the camera sees it (the camera's own motion taken off), thin (at least a
  pixel wide, lighter instead of thinner). Its splashes are a pass over the scene's upward surfaces within 18 units of the
  camera: in each cell of a grid on the ground (0.55 units) a ring now and then.
- **Snow**: round soft flakes, drifting in the wind.
- **Time**: the game frame shown, so they stand still while the game does, and are the same in every run of a test.

## Rain and snow in place of the game's

Batalia's rain is the game's own particles (types 0 and 1 of its particle pool, drawn by its particle program); Hoven has its
own snow. Where a level's `rain` or `snow` line is on (and not marked `keep`) and the option is on, the game's effect is
**not drawn**: the MRP's effects pass (`mrp_fx_draw`) skips its primitives. Only the drawing: the game still makes and updates
its particles, so its state is the same (`RAC1_RAMHASH`). `RAC1_PARTICLES_GAME=1`, or the debug menu's checkbox, draws the
game's as well (for comparing). With the MRP's effects off (`RAC1_MRP_FX=0`: the GS replica draws them) the game's are drawn.

## A level's lines

In the level's light file, `data/lights/levelNN.txt` (see [The light editor](lighting.md#the-light-editor)), one line a volume:

```
particles <kind> <density> [r g b] [wind <x> <y>] [box x0 y0 z0 x1 y1 z1] [keep]
```

- `kind`: `dust`, `ash`, `embers`, `fireflies`, `motes`, `rain`, `snow`.
- `density`: 1 the kind's own count, 0 to 4.
- `r g b`: a colour of its own (0 to 1), else the kind's.
- `wind x y`: units a second along the world's x and y.
- `box`: the particles only inside this world box (a lava field, an arena), faded over a unit at its faces.
- `keep`: for rain and snow, the game's own drawn as well.

The file is read when the level starts (also with the modern lighting off). Saving keeps the lines where they were, and a
line not edited is written back exactly as it was read.

## The editor

The debug menu's Sun tab (F1, with the debug tools) has the level's particle lines under the interiors: each line's kind,
density, colour, wind, box ("around the camera" makes one 40 x 40 x 12 units), `keep`, an on switch and remove; "Add a particles
line"; Save (the whole level file, as the Sun tab's other Save). Every change shows at once. The tab also sets the option for
the run (Off, Low, High), draws the game's rain and snow as well, and shows the particles drawn and the CPU time; the GPU's is
the Profiler tab's "Ambient particles" stage.

## Testing

- `RAC1_PARTICLES=0|1|2`: the option (Off, Low, High) whatever the preferences.
- `RAC1_PARTICLES_GAME=1`: the game's own rain and snow drawn as well.
- `RAC1_PARTICLES_TRACE=1`: every 120 frames what is drawn (draws, particles, the light, the game's rain or snow hidden).
- `RAC1_PARTICLES_FXTRACE=1`: the game's effect primitives by texture, and its particle pool by type (how the game's rain was
  found).
- The cost: `RAC1_PROFILER_DUMP` (the "Ambient particles" stage's CPU and GPU time).

## Cost

At High, 960 x 720, on an integrated Radeon 890M (the debug menu's Profiler stage "Ambient particles", averaged over 300 frames
of a walk from each level's start; `RAC1_PROFILER_DUMP`):

| Level | OpenGL CPU | OpenGL GPU | Vulkan CPU | Vulkan GPU |
|---|---|---|---|---|
| Kerwan (motes) | 0.03 ms | 0.13 ms | 0.01 ms | 0.03 ms |
| Umbris (embers) | 0.03 ms | 0.15 ms | 0.01 ms | 0.03 ms |
| Batalia (rain, splashes, ash) | 0.04 ms | 0.37 ms | 0.01 ms | 0.20 ms |
| Gaspar (embers, ash) | 0.04 ms | 0.16 ms | 0.01 ms | 0.04 ms |
| Orxon (dust) | 0.07 ms | 0.14 ms | 0.01 ms | 0.03 ms |
| Pokitaru (fireflies) | 0.04 ms | 0.14 ms | 0.01 ms | 0.02 ms |
| Hoven (snow) | 0.05 ms | 0.16 ms | 0.01 ms | 0.03 ms |

OpenGL's GPU time includes the copy of the depth buffer (about 0.1 ms); Batalia's the splashes' screen pass.

## Limits

- Rain and snow fall through roofs that are not in an interior box (under a ship's hull, a balcony): only the level's
  interior boxes keep them out. A box per covered place can be added in the Sun tab.
- The game's rain and snow are told from its other particles by their particle types (0 and 1 on every level but Hoven,
  0x49 on Hoven) and the textures those had this frame: another particle sharing one of their textures in the same frame
  would be hidden too.
- Umbris's embers fill the whole level; a `box` around the Snagglebeast's arena would keep them there (its coordinates were
  not reached in the headless runs).
