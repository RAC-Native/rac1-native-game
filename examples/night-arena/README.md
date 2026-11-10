# Daino's Arena

(Its folder and level id: `night-arena`.) A level mod made in Blender: an arena in a lava lake ringed by mountains, on Kalebo III's code (base level 16), with a
plugin that runs challenges against Kalebo III's enemies. It is the larger example next to
[the example level](../custom-level/README.md): the scripts here write the whole mod (geometry, collision, textures,
sounds, lights, objects and the plugin's source) into a mod folder, and the setup composes it with your disc. See
[docs/mods.md](../../docs/mods.md) ("Level mods") and [docs/sdk.md](../../docs/sdk.md); everything here is our own,
nothing of the game's (the enemies, the crates and the hero are the base level's, by their class numbers).

## What is in it

- **The arena:** a 96 x 96 floor inside a wall, three gates with drop zones in front, a hub with ramps, four towers,
  cover, four flame vents, floodlight masts, a corridor to the ship's plaza; the lava (surface type 1, the game's lava) all round.
- **The menu:** on the hub's top, Triangle opens the challenges (Cross starts one, Square opens the vendor).
- **The challenges:** waves of Kalebo III's robots, bomb bots, Gadgetron robots (bomb launcher, club, flamethrower),
  jetpack troopers and dropships; some with bombs from the sky, the vents burning faster and the three drop zones'
  circles electrified in turn (they hurt Ratchet and the enemies standing in them), a time limit, no hit allowed,
  the wrench only, a random weapon each wave, a new one every 20 seconds; two championships (15 and 50 rounds) mixing
  those modes. The won ones, and their best times, are kept with the save; a win saves the game (into the player's slot,
  as leaving a planet does) once Ratchet is back in the arena.
- **Grind rails**, for players who have the Grind Boots (Grindscarponi), from a station outside the north-west wall:
  - "Grindscarponi: Discesa": a course of three lanes along the north wall (a side jump changes lane), our mines to
    jump, Kalebo III's own rail mines (class 933, started on the lanes by the plugin, on their own code: broken by the
    wrench (Square on the rail) or a weapon from afar, they blow up as on Kalebo III) to break or to pass on another lane, two of them closing two lanes at a time, the side lanes ending before the
    north-east corner; then one rail round the corner, down the east side and over the wall into the arena, where a
    last wave of robots waits. A hit costs one health point and slows Ratchet (the game's own hop on the rail); a fall
    puts him back on the station, the mines back too;
  - "La grande fuga" (The Great Escape), the longest but Impossibile: the same course, two rounds in the arena, then
    the lava floods the arena and an escape rail from the hub's top leads west over the wall, round the south-west
    corner over the lava lake (our mines and Kalebo III's again) and through a portal's ring, grinding: a white flash,
    and Ratchet is in other planets' worlds (sections of the session, each with its own music), one after the other:
    - **Quartu**, down our rail from high south of its Giant Clank's pad (its model drawn in the world), then on Kalebo
      III's hoverboard (free: no race; a fall back by the pad) east across the Giant Clank's round arena to the ring in its
      middle, ramming the robots and bomb bots in the way (the board knocks down what it touches);
    - **Batalia**, its long rail from its start (its own: Ratchet comes in just over it and lands on it), with its spiked
      rail mines (jumped, or broken with the wrench), to our portal's ring just before its end, where the rail leaps:
      another flash;
    - **Rilgar**, its own hoverboard race (its hero code, its board, its HUD: lap, time, place) from its finish line
      round to the big ramp, whose jump lands on the small platform of bolt crates: the ring there;
    - **Umbris**, the Snagglebeast's cave (its objects, its boss line): the challenge is won when the Snagglebeast is
      beaten. The challenge's box and its messages go up over its health bar.

    A death anywhere is the challenge lost (it is meant to be hard); won or lost, Ratchet is taken back to the arena.
  - The session's other section, "prova" (`build_prova.py`: a round platform under Quartu's sky with its own rail in),
    where the escape went before Quartu's world could take a mod's rails, is still there, used by no challenge.

  The rails are level.txt's `rail` lines (the composer puts them in the game's rail table). Under each runs a thin
  collision strip (the game lets go of a rail with nothing under Ratchet's feet): near the ground Ratchet walks on (by the
  station, over the hub) it is an object of its own (`class ... collision`), solid only while its rail is on, so he does
  not walk on it in the air; the rest is the level's collision, too high to be walked onto, which Kalebo III's rail mines
  need to float over (their floor probe does not see objects). The plugin turns the rails all off at the level's start
  (`rail_enable`) and a challenge's own on only while it runs. The station is by one of Kalebo III's own rail
  spots, (115, 264, 133) in the game's coordinates: its hero code took a rail only near them while the plugin made the
  rails itself. With rails of the level's own it takes one anywhere.

- **Its planet:** a picture of its own on the loading screen and the ship's map's planet page (`make_textures.py` paints
  it), and under the galaxy a view of the arena rendered from the scene when it is exported; its name in five languages
  (Arena di Daino, Dainos Arena, Arena de Daino, Arène de Daino).

- **Its HUD** is at the bottom of the screen: the challenge's box at the left (under the quick select's ring), its banners
  and messages in the middle, at the same height: moved right just past the box when they would touch it, and made
  smaller when they do not fit there (the SDK's `hud_width`, ABI 14).

- **Its texts** (the menu, the challenges, the HUD, the banners) follow the game's text language: English, French,
  German, Spanish or Italian (plain ASCII, as the HUD's font takes it). The table is in the plugin (`TEXTS`); the
  weapons', the wrench's and the Grind Boots' names are read from the game's own texts, as it shows them.

- **A session** (`level.format` 3, the open engine's): the level is loaded once at the arrival. A death is a soft
  reset at a checkpoint (south of the hub), and a challenge he dies in is lost there, with no level load. Main's objects
  are frozen while he is in another section.

- **The game's own lava**: Ratchet runs on the generic hero code (level.txt's `hero generic`: the one Veldin, Gaspar and
  Umbris run, not Kalebo III's own), and the lake is surface type 1, the one Gaspar's and Umbris's lava is made of: as there,
  touching it he jumps out burnt, a health point lost each time, and with none left he sinks. (Type 3 is Aridia's
  quicksand: there he would sink and be bounced out with Cross.)

It needs a game with the mod SDK's ABI 16 or later (the hoverboard outside a race, Rilgar's race in its world; ABI 15 a
win's save, `game_save`; ABI 13 the mod's rail in a planet's world, the boss; `hero generic` ABI 12; the rails, the
sessions and sections ABI 11; the rest of the plugin ABI 5).

## Making it

You need Python 3 with Pillow and Blender (made with 5.2).

1. The textures (`textures/`):

   ```
   python3 make_textures.py
   ```

2. The scene and the mod folder (`out/night-arena`; `--install` also copies it to the game's `mods/night-arena`):

   ```
   blender -b --factory-startup --python build_arena.py --python build_rails.py --python build_prova.py --python export_arena.py -- --install
   ```

   Or in Blender's GUI, run `build_arena.py`, `build_rails.py` and `build_prova.py` (each rebuilds only its own collections), edit the
   scene, then run `export_arena.py` and call `export(None, True)` (the scripts' headers show how). The game's checkout
   is the repository these scripts are in; a `local.json` beside them (`{"game": "/path/to/rac1-native-game"}`) names
   another one.

3. Compose the level and build its plugin, from the game's checkout:

   ```
   ./build/rac1-native --setup --level-mod night-arena
   ./build/rac1-native --setup --mod-plugin night-arena
   ```

   Again after any change to `mods/night-arena`. In the game, Options > Mods: turn **Daino's Arena** on and confirm its
   code; the level is then a planet on the ship's map.

## Files

| File | What it does |
|---|---|
| `make_textures.py` | the arena's textures (PIL): floor, walls, grating, hazard stripes, lava, rock, the flames' sprites..., and the planet's picture |
| `build_arena.py` | the arena in Blender: the collections `arena_visual`, `arena_collision`, `arena_markers`, `arena_lights` |
| `build_rails.py` | the rails (`arena_rails`: a curve each; the course's side lanes made from its centre rail), their station, the course's and the escape's obstacles (markers), the portal's marker |
| `build_prova.py` | the section `prova` beyond the portal (`prova_visual`, `prova_collision`, `prova_markers`: its entry) |
| `export_arena.py` | the mod folder (and the map's picture, rendered): `level.txt`, `collision.obj`, `meshes.obj`, `lights.txt`, the classes' models (the rails' and the mines' too), the rails (level.txt's `rail` lines), the session's lines and its section's block (`prova_collision.obj`, `prova_meshes.obj`), the sounds (synthesized), `mod.json`, the plugin's source |
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
