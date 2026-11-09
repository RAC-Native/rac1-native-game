# Daino's Arena

(Its folder and level id: `night-arena`.) A level mod made in Blender: an arena in a lava lake ringed by mountains, on Kalebo III's code (base level 16), with a
plugin that runs challenges against Kalebo III's enemies. It is the larger example next to
[the example level](../custom-level/README.md): the scripts here write the whole mod (geometry, collision, textures,
sounds, lights, objects and the plugin's source) into a mod folder, and the setup composes it with your disc. See
[docs/mods.md](../../docs/mods.md) ("Level mods") and [docs/sdk.md](../../docs/sdk.md); everything here is our own,
nothing of the game's (the enemies, the crates and the hero are the base level's, by their class numbers).

## What is in it

- **The arena:** a 96 x 96 floor inside a wall, three gates with drop zones in front, a hub with ramps, four towers,
  cover, four flame vents, floodlight masts, a corridor to the ship's plaza; the lava (surface type 3) all round.
- **The menu:** on the hub's top, Triangle opens the challenges (Cross starts one, Square opens the vendor).
- **The challenges:** waves of Kalebo III's robots, bomb bots, Gadgetron robots (bomb launcher, club, flamethrower),
  jetpack troopers and dropships; some with bombs from the sky, the vents burning faster, a time limit, no hit allowed,
  the wrench only, a random weapon each wave, a new one every 20 seconds; two championships (15 and 50 rounds) mixing
  those modes. The won ones, and their best times, are kept with the save.
- **Grind rails**, for players who have the Grind Boots (Grind Scarponi), from a station outside the north-west wall:
  - "Test Grind Scarponi": one rail down into the arena (our first try);
  - "Grind Scarponi: Discesa": a course of three lanes along the north wall (a side jump changes lane), mines to jump,
    barriers that close two lanes at a time, drones to break with the wrench (Square on the rail) or they zap you, the
    side lanes ending before the north-east corner; then one rail round the corner, down the east side and over the
    wall into the arena, where a last wave of robots waits. A hit costs one health point and slows Ratchet (the game's
    own hop on the rail); a fall puts him back on the station, the obstacles back too.

  Kalebo III's hero code takes a rail only within 12 units of two points of its own level, (115, 264, 133) and
  (257, 301, 128) in the game's coordinates, so the station is by the first one; once on a rail, its jumps and the side
  jumps to another rail work anywhere. The plugin puts the challenge's rails in the game's rail table only while it
  runs, and a thin collision strip runs under each rail (the game lets go of a rail with nothing under Ratchet's
  feet).

- **Its planet:** a picture of its own on the loading screen and the ship's map's planet page (`make_textures.py` paints
  it), and under the galaxy a view of the arena rendered from the scene when it is exported; its name in five languages
  (Arena di Daino, Dainos Arena, Arena de Daino, Arène de Daino).

It needs a game with the mod SDK's ABI 5 or later.

## Making it

You need Python 3 with Pillow and Blender (made with 5.2).

1. The textures (`textures/`):

   ```
   python3 make_textures.py
   ```

2. The scene and the mod folder (`out/night-arena`; `--install` also copies it to the game's `mods/night-arena`):

   ```
   blender -b --factory-startup --python build_arena.py --python build_rails.py --python export_arena.py -- --install
   ```

   Or in Blender's GUI, run `build_arena.py` and `build_rails.py` (each rebuilds only its own collections), edit the
   scene, then run `export_arena.py` and call `export(None, True)` (the scripts' headers show how). The game's checkout
   is the repository these scripts are in; a `local.json` beside them (`{"game": "/path/to/rac1-native-game"}`) names
   another one.

3. Compose the level and build its plugin, from the game's checkout:

   ```
   ./build/rac1-native --setup --level-mod night-arena
   ./build/rac1-native --setup --mod-plugin night-arena
   ```

   Again after any change to `mods/night-arena`. In the game, Options > Mods: turn **Night Arena** on and confirm its
   code; the level is then a planet on the ship's map.

## Files

| File | What it does |
|---|---|
| `make_textures.py` | the arena's textures (PIL): floor, walls, grating, hazard stripes, lava, rock, the flames' sprites..., and the planet's picture |
| `build_arena.py` | the arena in Blender: the collections `arena_visual`, `arena_collision`, `arena_markers`, `arena_lights` |
| `build_rails.py` | the rails (`arena_rails`: a curve each; the course's side lanes made from its centre rail), their station, the course's obstacles (markers) |
| `export_arena.py` | the mod folder (and the map's picture, rendered): `level.txt`, `collision.obj`, `meshes.obj`, `lights.txt`, the classes' models (the rails', the mines', barriers' and drones' too), the rails (`rails.txt`), the sounds (synthesized), `mod.json`, the plugin's source |
| `plugin/arena_plugin.c` | the menu, the challenges, the enemies' paths (a navigation grid and a flow field), the flame vents, the bombs, the supplies, the rails |

## Testing

Headless runs with the mod's own test hooks (`RAC1_MOD_TEST`, listed before `test()` in the plugin), for example the
rail challenge with Ratchet dropped onto the rail (`arena_course`: the course, riding its centre lane through its
obstacles to the last wave):

```
RAC1_HEADLESS=1 RAC1_MODS=1 RAC1_MOD_CODE=1 RAC1_GOTO=night-arena@60 RAC1_MOD_TEST=arena_rail RAC1_FRAMES=2900 ./build/rac1-native
```

A `RAC1_SIM_INPUT=seq:` script's frames are the run's, about 1472 more than the test's frames (the level's loading).

## Music

The level plays "Overworld" by Daino Selvatico (the example level's, `../custom-level/music/overworld.ogg`), licensed
under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
