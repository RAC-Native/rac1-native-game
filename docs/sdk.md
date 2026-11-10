# The mod SDK: reference

The SDK is what a mod's code is written against: one C header, [`sdk/rac1_mod.h`](../sdk/rac1_mod.h), and one CMake
file, [`sdk/rac1_mod.cmake`](../sdk/rac1_mod.cmake). This page is its reference: every struct, service and callback,
when each may be used, the order things happen in, the limits and how versions work. For a first plugin (the folder,
building, testing) read [Custom levels: code](custom-levels.md) first; for the level folder itself (level.txt, the
meshes, the music) see [Mods, "Level mods"](mods.md#level-mods). The example level, `examples/custom-level/`, uses
nearly everything here.

Contents:

1. [Versions](#versions)
2. [Lifecycle](#lifecycle)
3. [Rules: threads, replays, memory, floats](#rules)
4. [The level's own content (level.txt)](#the-levels-own-content-leveltxt)
5. [Callbacks (`rac1_mod`)](#callbacks-rac1_mod)
6. [Services (`rac1_api`)](#services-rac1_api)
7. [Objects](#objects)
8. [Hits and damage](#hits-and-damage)
9. [Sounds](#sounds)
10. [Structs](#structs)
11. [Limits](#limits)
12. [The game's memory](#the-games-memory)

## Versions

`RAC1_MOD_ABI` is the version of the interface: a whole number that grows by one each time something is added.

| ABI | Game | Added |
|---|---|---|
| 1 | the first with code | the callbacks level_start .. shutdown; the services log .. call_port |
| 2 | the first with level objects | level objects of the level's own classes (`class_define` .. `object_at`, `rac1_class_def`, `rac1_spawn`), `collide_ray`, `objects_near`, `mobys_near`, `hero_damage`, `hit_send`, `hit_take`, the level's own sounds (`sfx_*`), triggers (`trigger_find`, `trigger_inside`, the `trigger` callback), `frame_count` |
| 3 | the first with base levels | the base level's objects at run time: `base_spawn`, `base_moby`, `base_free` (level.txt's `spawn` lines) |
| 4 | the first with the vendor | `vendor_open`, `input_hold`, the HUD's panels, anchors and sizes (`hud_rect`, `hud_text_ex`, `hud_text_width`, `RAC1_HUD_*`), `base_hunt`, `light_add`; level.txt's `update_all` line |
| 5 | the first with Kalebo III's enemies | Kalebo III's enemies: `base_option` (`RAC1_BASE_WEAPON`, `RAC1_BASE_TROOPS`), `base_handle`, `base_hunt` and `base_spawn` for the bomb bots 52, the Gadgetron robots 541, the jetpack troopers 638 and the dropships 1356; the weapons: `weapons_owned`, `weapon_equip`, `weapon_allow`, `ammo_fill` |
| 6 | the first with imports | classes of other levels (level.txt's `import` lines): `import_class`, `import_spawn`, `import_load`, `import_release` |
| 7 | (reserved) | the rails and the set pieces, still to come: their room in `rac1_api` (`reserved_abi7`, 16 entries) is kept, so ABI 8's services do not move when they come; never call them |
| 8 | this one | the level data of imported classes: `path_find`, `path_create`, `object_bind_path`, `object_bind`, `group_create`, `object_slots`; level.txt's `path` lines; imported enemies that read their planet's paths and areas |

How compatibility works:

- **Both tables only grow at their end.** Nothing is removed, moved or changed in meaning; a new version appends
  services to `rac1_api`, callbacks to `rac1_mod`, and fields at the end of the structs.
- **A plugin built for an older ABI runs on a newer game.** The game reads the plugin's `mod->size` (set it to
  `sizeof(rac1_mod)`): callbacks past it are none.
- **A plugin built for a newer ABI than the game's is refused** ("needs a newer game" in Options > Mods and the log).
  To run on older games too, build against the older header, or test each newer service before using it:
  `RAC1_API_HAS(api, service)` is true when the game's table has it (`api->size` is the game's `sizeof(rac1_api)`).
  `api->abi` is the game's ABI.
- **Structs carry their size.** Each struct the SDK passes starts with `uint32_t size`. In the ones you fill
  (`rac1_class_def`) set it to `sizeof`; in the ones the game fills for you (`rac1_ray_hit`, `rac1_hit` given to
  `hit_take`) set it to `sizeof` and the game fills only what fits (0 counts as the whole struct of the game's
  version); the ones the game gives you (`rac1_spawn`, `rac1_hit` in the `hit` callback) carry the game's size.

The level folder's own version is `level.format`: 1 runs on Veldin's code (level 0), 2 names another base level
(`level.base`, [Base levels](mods.md#base-levels)). The lines below are additions both formats read; a game without them
refuses the level with the line's number.

## Lifecycle

```
game start ──> plugins loaded (load order), rac1_mod_main(api, mod) each: register callbacks and classes
   │
   ├─ new game ──> new_game                          a save loaded ──> load (your bytes, or NULL)
   │
   ├─ a level loads ──> level_end (the level before) ──> [the level's load]
   │      └─ its first frame of play (with a ship: after the landing, Ratchet off the ship):
   │           sounds' bank loaded ─> placed objects spawned (each class's spawn) ─> level_start
   │
   ├─ each frame of play (not in menus, not while the renderer replays):
   │      objects' pass: for each object: hit (if it took one) ─> update
   │                     (and ABI 1's object_update for the level's placed mobys)
   │      triggers ─> frame ─> test ─> (the objects' models handed to the renderer)
   │      ... the hero, particles, the camera ─> camera
   │      HUD drawn ─> hud (may run twice: draw only)
   │
   ├─ a save is written ──> save
   └─ game end ──> shutdown
```

- `rac1_mod_main` runs once, at the game's start, before any level. Call `class_define` there.
- A level's objects are removed when the level is left: each class's `remove` runs (before `level_end`), their
  states are freed. Nothing of a level survives it: keep what must last in your own variables (and in `save`).
- Objects spawn at the level's first frame, **before** `level_start`: a class's `spawn` cannot count on what
  `level_start` sets up (the example looks its sounds up in both).
- The frame's order: the objects' pass runs inside the game's moby pass (where a moby's class code runs); the
  `trigger`, `frame` and `test` callbacks after it; `camera` after the game's camera; `hud` while the HUD is drawn.

## Rules

- **One thread.** Every callback runs on the game's thread, one at a time. Call the services only from inside a
  callback (or `rac1_mod_main`), never from a thread of your own, never after `shutdown`.
- **Replays.** The renderer runs some of the game's code a second time for a frame inside a journal of the game's
  memory, and puts the memory back after (`api->replaying()` is 1 then). No callback runs during a replay except
  `hud`; there, write nothing (neither the game's memory nor your variables): draw the text you made in `frame`.
  The objects' classes' callbacks never run in a replay; an object simply stays where it was for that pass.
- **The game's memory.** Read it inline through `api->ram` (`rac1_lw`, `rac1_lh`, `rac1_lb`, `rac1_lwf`). Write it
  only through `sw`, `sh`, `sb`, `swf` (and the services): the writes go through the journal. Never write through
  `api->ram`.
- **Your memory.** An object's state (`object_state`) is the host's memory, yours to read and write in your callbacks.
  It is not the game's memory: it is not journaled and not saved.
- **Floats.** Your code runs with the PS2's rounding (toward zero); `rac1_mod.cmake` compiles with the game's float
  rules (no FMA contraction, no fast math). Library functions (`sinf`, ...) may differ in the last bit between C
  libraries: avoid feeding them into the game's own state where a replay must be exact.
- **Handles.** Object handles are `uint32_t`, 0 none. A removed object's handle never names another object: every
  service given a stale handle does nothing and returns 0 / -1 / NULL. Mobys are the game's addresses
  (`uint32_t`): one may be freed and reused by the game; for your own objects prefer the object handle.

## The level's own content (level.txt)

A level mod's `level.txt` (format 1) may carry these lines besides those [Mods, "Level mods"](mods.md#level-mods)
lists. The composer checks them (an error names the file and line) and writes them into the build (`level.sdk`,
`sounds.bnk`, the models in `host_meshes.mrpc`).

```
class <name> <model.obj> [size <s>] [solid | ghost] [scale <k>]
object <class> <x> <y> <z> [<yaw>] [<key>=<value> ...]
sound <name> <file.wav | file.ogg | file.vag> [volume <v>] [range <near> <far>] [loop] [pitch <semitones>]
trigger <name> <x0> <y0> <z0> <x1> <y1> <z1>
```

| Line | What |
|---|---|
| `class` | a class of the level's own objects. `<name>`: letters, digits, `-`, `_`, `.` (31 at most); your plugin binds its behaviour to it with `class_define`. `<model.obj>`: its model, an OBJ in the level folder made **round its own origin, standing on it** (the object's position is the model's origin, and its hit box rises from there); level.txt's `up` and `scale` apply to it, its `offset` does not. The same rules as `meshes.obj` (its MTL, PNG textures or `Kd` colours, vertex colours, 16 materials, 64 colours); the whole file is one model. `size`: the hit box's edge in units (default: the model's largest extent times `scale`). `ghost`: no collision at all (nothing stands on it, bumps into it, hits it; rays miss it); `solid` (the default): a box. `scale`: the model's scale |
| `object` | an object of a level class, placed: at `<x> <y> <z>` (level.txt's offset applies) turned `<yaw>` radians about z. The `key=value` words (no spaces; any keys you like) are for your plugin: `object_param`, `object_named` (`name=`). Placed objects are there whether or not the mod's code runs: drawn and solid, without behaviour |
| `sound` | a sound of the level's own: a WAV (PCM, 8 or 16 bits, mono or stereo: mixed to mono), an Ogg Vorbis file (`.ogg`, mono or stereo: mixed to mono; decoded when the level is composed) or a VAG (the music's format), 8000 to 48000 Hz. `volume` 0 .. 1 (default 1); `range`: full volume within `near` units of the camera, none past `far` (default 10, 60); `loop`: it plays until stopped (the whole file loops); `pitch`: semitones up (or down, negative), -24 .. 24 |
| `trigger` | a box (its two corners; offset applies) Ratchet enters and leaves: the `trigger` callback, `trigger_inside` |

The build's files: `level.sdk` (the classes, placed objects, sounds and triggers, for the game), `sounds.bnk` (one
989snd sound bank, the format of the game's own banks), and the class models as more host meshes in
`host_meshes.mrpc` and the MRP world. `compose.log` lists each class, object, sound and trigger.

## Callbacks (`rac1_mod`)

Fill them in `rac1_mod_main`; all are optional (NULL: none). Each gets your `mod->user` back.

| Callback | ABI | When | Notes |
|---|---|---|---|
| `level_start(user, level)` | 1 | a level's first frame of play, its objects spawned (with a ship: after the landing) | `rac1_level_info`: `level` (the game's level number: a level mod's is its base level's), `slot` (19.. for a level mod), `level_id`, `mine` (1: your mod's level), `objects` (its placed mobys: uids 1..objects) |
| `level_end(user)` | 1 | the level is left (another loads, a save is loaded) | after the objects' `remove` |
| `frame(user)` | 1 | once a frame of play, after the objects' pass | not in menus, not in replays, not while the world is held (Advanced > pause in the quick select: the quick select open) |
| `hud(user)` | 1 | the HUD is drawn (not under the vendor's screens, ABI 4 games) | `hud_text` (and ABI 4's `hud_rect`, `hud_text_ex`) work only here; may run twice a frame: draw only |
| `camera(user)` | 1 | after the game's camera update | change the camera's memory here |
| `object_update(user, moby, uid, cls)` | 1 | before one of your level's placed mobys (level.txt's `moby` lines) is updated | return 1: its class's code skipped (yours instead), 0: it runs |
| `new_game(user)` | 1 | a new game starts | reset your progress |
| `save(user, buf, cap)` | 1 | a save is written | your bytes (at most `cap`, about 64 KB) into `buf`, their count returned (0: none); they go into `save<N>.pc` |
| `load(user, data, size)` | 1 | a save is loaded | your bytes as saved; NULL / 0 when it has none |
| `test(user, text, frame)` | 1 | `RAC1_MOD_TEST=<text>` given, once a frame of play | for scripted tests |
| `shutdown(user)` | 1 | the game ends | |
| `trigger(user, index, name, inside)` | 2 | Ratchet entered (`inside` 1) or left (0) one of level.txt's triggers | before `frame`; once per change |

A class's own callbacks are in its `rac1_class_def` ([Objects](#objects)).

## Services (`rac1_api`)

`api` is valid from `rac1_mod_main` until `shutdown`. "Any" in *When*: any callback; "play": any callback but `hud`.

### The log, files (ABI 1)

| Service | Does |
|---|---|
| `void log(const char* text)` | a line in the game's log, `[mod <folder>] text` |
| `int mod_path(const char* name, char* out, size_t cap)` | the full path of `name` in your mod's folder; 0 if it does not fit |

### Memory (ABI 1)

| Service | Does |
|---|---|
| `const uint8_t* ram` | the game's memory (32 MB), read-only: `rac1_lw`, `rac1_lh`, `rac1_lb`, `rac1_lwf` |
| `uint32_t ov(uint32_t veldin_addr)` | where Veldin's overlay address is on the current level |
| `void sw / sh / sb(uint32_t addr, uint32_t v)`, `void swf(uint32_t addr, float v)` | writes (journaled); an address outside the memory is ignored (logged) |
| `uint32_t ee_alloc(uint32_t size)` | `size` bytes of the game's memory of your own (16-byte aligned, zeroed), 0 when the block is full (53 KB shared by all plugins, for the whole run: take it once) |
| `int replaying(void)` | 1 while the renderer replays a frame |

### Mobys (ABI 1)

| Service | Does |
|---|---|
| `uint32_t moby_by_uid(int32_t uid)` | the live moby with that uid, 0 none |
| `uint32_t moby_spawn(int32_t cls)` | a new moby of a class the level has, 0 none; place it, then `moby_moved` |
| `void moby_remove(uint32_t moby)` | (do not use it on an object's moby: `object_remove`) |
| `void moby_moved(uint32_t moby)` | its matrix from its position, rotation, scale |
| `uint32_t moby_link(moby, pvar_offset)`, `void moby_set_link(moby, pvar_offset, other)` | a pvar word naming another moby |

### The base level's objects (ABI 3)

| Service | Does |
|---|---|
| `uint32_t base_spawn(int32_t cls, const float pos[3], float yaw)` | a new object of the base level's class `cls` at `pos`, turned `yaw`, as the base level's first placement of the class (its settings, its pvars, its bolts), its class's own behaviour: a handle, 0 none (no `spawn` line for the class, its room all in use) |
| `uint32_t base_moby(uint32_t handle)` | its moby while it lives; 0 once it is gone (killed, removed with `moby_remove`, the level left): how a plugin knows it died |
| `int32_t base_free(int32_t cls)` | how many more of class `cls` `base_spawn` can start now |

See [Base level objects](#base-level-objects) below.

### Collision and queries

| Service | ABI | Does |
|---|---|---|
| `int collide_line(from, to, hit)` | 1 | the segment against the level's collision and the mobys: 1 and the point |
| `int collide_ray(from, to, ignore, flags, out)` | 2 | the same, with what was hit: `rac1_ray_hit` (point, unit normal, moby or 0 for the level, object or 0). `ignore`: a moby skipped (your own object's, to look out of it). `flags`: `RAC1_RAY_NO_LEVEL` (mobys only), `RAC1_RAY_HITTABLE` (only mobys that take hits), `RAC1_RAY_BACKFACES` |
| `int32_t objects_near(pos, radius, out, max)` | 2 | the level objects whose positions are within `radius`, nearest first: at most `max` into `out`; the count (with `out` NULL: how many there are) |
| `int32_t mobys_near(pos, radius, cls, out, max)` | 2 | the same for live mobys (class `cls`, -1 any; Ratchet and the objects' mobys included) |

### The hero (ABI 1, 2)

| Service | ABI | Does |
|---|---|---|
| `uint32_t hero(void)` | 1 | Ratchet's moby |
| `void hero_place(pos, yaw)` | 1 | put him there standing (as the game's teleports: his state reset to standing, the camera behind him) |
| `int32_t bolts(void)`, `void bolts_add(int32_t n)`, `int32_t health(void)` | 1 | |
| `int hero_damage(int32_t amount, uint32_t from)` | 2 | hurt him by `amount` health points, as the game's enemies do: a hit record on him; the game's hero code then takes the health, plays the hurt, knocks him back from `from` (0: from where he faces), and ignores it while he cannot be hurt (just hurt, in some states). 1 when the hit was sent |

### Sound, music, effects (ABI 1, 2)

| Service | ABI | Does |
|---|---|---|
| `int32_t sound(int32_t index, uint32_t moby)` | 1 | the base level's sound `index` at `moby` (0: Ratchet): its slot, -1 |
| `void music(int32_t track)` | 1 | the level's music table's track |
| `int32_t sfx_find(const char* name)` | 2 | the current level's own sound of that name (level.txt's `sound` lines): its index, -1 |
| `int32_t sfx_play(int32_t sound, uint32_t moby, const float pos[3])` | 2 | play it: on `moby` (it follows it), else at `pos`, else (0, NULL) everywhere (no distance, no pan). Its slot, -1 when none ([Sounds](#sounds)) |
| `void sfx_stop(int32_t slot)` | 2 | stop it (a slot `sfx_play` gave that still plays one of the level's sounds) |
| `int sfx_active(int32_t slot, int32_t sound)` | 2 | 1 while `slot` plays the level's sound `sound` (-1: any of the level's own) |
| `void smoke(pos, vel, size)` | 1 | a puff of the base level's smoke |

### HUD, camera, pad, random, time, the game's code

| Service | ABI | Does |
|---|---|---|
| `void hud_text(x, y, abgr, text)` | 1 | in `hud` only: plain ASCII in the small font at (x, y) of 512 x 448, colour 0xAABBGGRR (alpha 0x80 opaque); 15 lines a frame at most |
| `void camera_position(float out[3])` | 1 | |
| `uint32_t pad_held(void)`, `uint32_t pad_pressed(void)` | 1 | pad 1's bits: 0x10 triangle, 0x20 circle, 0x40 cross, 0x80 square, 0x800 start, 0x1000 up, 0x2000 right, 0x4000 down, 0x8000 left, 1 L2, 2 R2, 4 L1, 8 R1 |
| `uint32_t random(void)` | 1 | your own numbers (the game's untouched), the same each run from a level's start |
| `uint64_t frame_count(void)` | 2 | the frames of play since the game started (the `test` callback's frame) |
| `int call_port(veldin_addr, arg)` | 1 | one of the game's functions it calls through a pointer (the objects' updates and the like), with one argument; 0 when the port has none. Not the vendor's screens (0x29a6b0: the game calls it directly, never through a pointer): `vendor_open` |

### The vendor, the pad, the HUD's panels, lights (ABI 4)

Test `RAC1_API_HAS(api, vendor_open)` before using them (a game older than ABI 4 has none of them).

| Service | Does |
|---|---|
| `int vendor_open(void)` | the vendor's screens (weapons and ammo to buy), as the game's own vendor item opens them (0x29a6b0 with no Vendor moby): 1 opened, 0 not (not in play - a menu, a cutscene, the landing, the vendor already open -, or called from `hud` or while replaying). Call it from `frame` (or an object's update, a trigger). The game stays paused under them: `frame` and `hud` are not called until the player leaves them (triangle), then play goes on where it was. Works on every base level (the vendor's code is in every level's overlay); tested on Kalebo III |
| `void input_hold(int on)` | on: from the next frame the game gets nothing from the pad in play (no walking, jumping, attacking, no camera stick) but **Start and Select**, so the pause menu (and quitting from it) and the map work as always; menus, the vendor and cutscenes get the pad as always. `pad_held` / `pad_pressed` still read the real pad, for your own menu. Off: the pad is the game's again, except the buttons still held at that moment, until released (the Cross that closed your menu does not make him jump). Off again at every level's start. Use it instead of writing the pad's fields at 0x13c940 (the game's own pad read writes them every frame, before your callbacks, and its moves read the presses' history too) |
| `void hud_rect(x, y, w, h, abgr, anchor)` | in `hud` only: a flat rectangle `w` x `h` from (`x`, `y`) of the 512 x 448 HUD, colour 0xAABBGGRR (alpha 0x80 opaque, 0x40 half transparent): a panel behind your texts (draw it first) |
| `void hud_text_ex(x, y, abgr, scale, anchor, text)` | in `hud` only: `hud_text` at `scale` times the font's size (1: `hud_text`'s, 2: twice; at most 8) |
| `int32_t hud_text_width(text, scale)` | `text`'s width in HUD units at `scale` (to centre or right-align it: `x = 256 - w / 2`) |
| `int base_hunt(uint32_t moby, int on)` | the base level's enemy `moby` (`base_moby`'s, or a `moby` line's) hunts Ratchet wherever he is instead of guarding where it started: [Base level objects](#base-level-objects). 1 done, 0 not (no such moby, a class not known: Kalebo III's robots 1445; from ABI 5 also `base_spawn`'s 52, 541, 638 and 1356, [Kalebo III's enemies](#kalebo-iiis-enemies-abi-5)) |
| `int light_add(pos, rgb, intensity, radius)` | a light for this frame only (call it every frame it shines, from `frame`): at `pos`, colour `rgb` (0..1) times `intensity` (0..8, 1 as a lamp of the debug menu's Lights tab), reaching `radius` units (at most 60). 1 taken, 0 not (31 a frame for all the plugins taken, bad values, from `hud`) |

**Anchors** (`hud_rect`, `hud_text_ex`): `RAC1_HUD_CENTRE` (0) is the 4:3 frame `hud_text` draws in, in the middle of the
screen; `RAC1_HUD_LEFT` (1) and `RAC1_HUD_RIGHT` (2) move the element with the game's own side elements (the health on the
left, the bolts on the right) toward that edge of the screen when the HUD is wider than 4:3 (Options > Display > HUD
16:9, 21:9, 32:9 or Fit, over a widened view). At 4:3 the three are the same. Coordinates stay the 512 x 448 HUD's: a
panel at x 10 anchored left is 10 units from the screen's left edge, one ending at x 502 anchored right ends 10 from its
right edge. Not on the vendor's screens (4:3 there, and the `hud` callback is not called under them).

**Lights** (`light_add`): drawn by the modern lighting's point lights per pixel, as the lights placed by hand in a
level's `lights.txt` and Veldin's plasma beams: on the world, the mobys and the level's meshes, with no shadow (no
shadow cube) and no light of their own on the faithful renderer. Only when the modern lighting draws point lights
(Options > Graphics: Lighting modern, Secondary Lights on; a headless test: `RAC1_SHADOWS=1`); off, or on the
faithful renderer, they draw nothing and cost nothing. A light more than the shadows' second split (about 60 units)
from the camera is left out. They are kept in the game's memory with the frame (the last 1 KB of the mods' block), so a
pipelined frame and the pause show the lights of their own frame. Cost: each one is a point light of the 96 the
lighting's shaders take a pixel, before the automatic emitters (which then get fewer); a few dozen lights near the
camera cost about as much as the same number of hand-placed lamps.

### Kalebo III's enemies, the weapons (ABI 5)

Test `RAC1_API_HAS(api, base_option)` before using them (a game older than ABI 5 has none of them).

| Service | Does |
|---|---|
| `int base_option(uint32_t handle, int32_t option, int32_t value)` | a choice of the class's for an object `base_spawn` has just started, in the same callback (before its first update). `RAC1_BASE_WEAPON` (1): a Gadgetron robot's (541) weapon, 0 the bomb launcher, 1 the club, 2 the flamethrower (the default, the pool's first placement's): 1 done. `RAC1_BASE_TROOPS` (2): a dropship's (1356) jetpack troopers, `value` (at most 4) started from class 638's spawn room, carried and dropped at its landing point: how many it took. 0: not (no such object, updated already, a class or a value the option has not) |
| `uint32_t base_handle(uint32_t moby)` | the handle of a moby `base_spawn` started, 0 none: the troopers a dropship brought (found with `mobys_near(pos, r, 638, ...)`) have handles too, for `base_moby` and `base_hunt` |
| `int32_t weapons_owned(int32_t* out, int32_t max)` | the weapons Ratchet owns: at most `max` item ids into `out` (may be NULL), in id order; the count. The weapons are the pause menu's Weapons page's items: 0x09 Suck Cannon, 0x0a Bomb Glove, 0x0b Devastator, 0x0d Visibomb Gun, 0x0e Taunter, 0x0f Blaster, 0x10 Pyrocitor, 0x11 Mine Glove, 0x12 Walloper, 0x13 Tesla Claw, 0x14 Glove of Doom, 0x15 Morph-o-Ray, 0x17 R.Y.N.O., 0x18 Drone Device, 0x19 Decoy Glove (the owned bytes 0x13d4c0) |
| `int weapon_equip(int32_t item)` | that weapon in his hand as the quick select puts it there (the request P+0x20b8 when Triangle is let go: the game's switch follows, its animation and sound, in the next frames; the ammo clamped to its most). 1 asked (or in hand already); 0 not: not owned, not allowed (`weapon_allow`), not in play (a menu, a cutscene, from `hud`), or where the quick select could not be opened now: swinging or throwing the wrench (state 0x13 and the like), an item being switched or used, in a vehicle, the ship or on a hoverboard. Standing, walking, running, crouching, strafing, jumping and falling it works. Call it again a frame later when it says 0 |
| `void weapon_allow(const int32_t* items, int32_t n)` | only those `n` weapons can be chosen: the others are not shown in the quick select's ring (empty slots), the PC's next / previous weapon skips them, Triangle's double press does not swap to one, and one the game asks for itself (its return to the weapon used before a gadget, a level's own build of the double press) is not taken; one that reached his hand anyway is put back for the last allowed item he held (else the wrench), unless a switch to an allowed weapon is under way (`weapon_equip`'s, the quick select's): that one is left to finish. So `weapon_allow` of a new weapon without the one in hand and `weapon_equip` of the new one, in either order, in one frame or across frames, end with the new one in hand (no need to keep the old one allowed until the switch is done). Only weapons are limited: the wrench and the gadgets never. NULL or 0: all again. Off at every level's start |
| `int ammo_fill(int32_t item)` | that item's ammo (0x13d428) to its most (the item table's, as the HUD and the pickups): 1; 0 not (not owned, an item with no ammo) |

### Classes of other levels (ABI 6)

Test `RAC1_API_HAS(api, import_class)` first. A level mod's `level.txt` brings classes of other planets with `import <name>`
or `import <level>:<class>` lines ([Level mods](mods.md#the-level-folder); the names are the game's
`data/pool/names.txt`), each with what it spawns (its shots, effects, debris: [The open engine](open-engine.md#dependencies)).
They are in the level from its load; these services spawn them and bring them in or out during play. `name` is a name or
`"<level>:<class>"`.

| Service | Does |
|---|---|
| `int32_t import_class(const char* name)` | the class id the import has in the level now (its own id, as on its planet, unless the base level has that id: then one of 2048 and up), for `import_spawn`, `mobys_near` and the moby's +0xa6; the base level's own class when `name` is one of its; -1 when it is not in the level |
| `uint32_t import_spawn(int32_t cls, const float pos[3], float yaw)` | an object of that class started at `pos`, turned `yaw` radians, as its planet first places one: its size, update distance, settings and private values (what names other objects of its planet, or its planet's shared data, is none); it drops its bolts when killed and is gone once killed. Its moby's address, 0 when it cannot be (no such class, no room in the moby array, not in play) |
| `int32_t import_load(const char* name)` | the class brought into the level now, with what it spawns (drawn from the next frame; the modern renderer loads the level's moby assets again, about 25 ms): its id, -1 when it cannot be |
| `int32_t import_release(const char* name)` | the class taken out of the level: its objects removed at once, its memory given back a few frames later, the classes only it needed with it; how many classes went (0: not one of the level's imports). A class `level.txt` imports is back at the level's next load |

Classes that read no level data of their own planet (`compose.log` says `reads -`) work as they are (Umbris's turret
`umbris.turret`, Orxon's spore plant `orxon.spore_plant`). From ABI 8's game on, classes that read their planet's paths,
areas or boxes work too: they find their planet's level data at their planet's indices, and a class with a **profile** (the
game's `data/pool/profiles`) is given level data of its own round where `import_spawn` starts it (an area round it, its
patrol ahead of it...), which the next section's services change. The mobys a placement names (an attached part, the
troopers a ship carries) are spawned with it, and `import_load` no longer reloads the level's moby assets (about 1 ms).

### The level data of imported classes (ABI 8)

Test `RAC1_API_HAS(api, object_bind_path)` first. An imported object's private values that name its planet's paths,
areas, groups or other objects are its class's **slots**, named by its profile ([The open engine](open-engine.md#class-profiles));
`object_slots` lists them. A class without a profile has none.

| Service | Does |
|---|---|
| `int32_t path_find(const char* name)` | level.txt's `path <name> x y z ...` line `name` (its points moved by the level's `offset`, as every coordinate): its handle, 0 when there is none |
| `int32_t path_create(const float* xyz, int32_t n)` | a path of the plugin's own: `n` points (1 to 64), `xyz[3 * n]` in the game's coordinates: its handle (until the level's next load), 0 when it cannot be |
| `int object_bind_path(uint32_t moby, const char* slot, int32_t path)` | the slot (a path or an area) of the imported object `moby` given the path's points: they are copied into the object's planet's level data (a path of the object's own there, freed when it is gone). 1 done, 0 not (no such object or slot, not a path's slot, no room) |
| `int object_bind(uint32_t moby, const char* slot, uint32_t value)` | the slot set to `value`: a `moby` slot takes another object's moby address (0: none), a `group` slot a `group_create` handle, a `word` slot the word. 1 done, 0 not |
| `int32_t group_create(uint32_t owner, const uint32_t* mobys, int32_t n)` | a group of `n` objects (moby addresses, 1 to 64) in the planet's level data of the imported object `owner`: its handle for `object_bind`, -1 when it cannot be |
| `int32_t object_slots(uint32_t moby, char* out, int32_t cap)` | the slots of the object's class, `"name:kind name:kind ..."` (kinds: path, area, group, moby, word, vec3), into `out`: their count, 0 when it has no profile |

The enemies with profiles (their names in the game's `data/pool/names.txt`; each fought in the arena example's test copy):

| Name | What | Slots (their defaults at `import_spawn`) |
|---|---|---|
| `fleet.blarg_trooper` (17:1382) | Drek's fleet's Blarg trooper | `area` (a square of 24 round it); `box_a`..`box_d` (none: always awake) |
| `gemlik.flying_robot` (13:63) | Gemlik's flying robot | `path_a`, `path_b` (none, as on its planet) |
| `gaspar.walking_robot` (9:193) | Gaspar's walking robot | `path` (none, as on its planet) |
| `blarg.serpent` (6:1048) | Blarg's serpent: kept inside its pen | `pen` (a square of 8 round it) |
| `pokitaru.amoeboid` (11:572) | Pokitaru's amoeboid | `area`, `home` (squares of 12 round it) |
| `hoven.flying_trooper` (12:294) | Hoven's flying trooper (it does not wake for Ratchet beside it, on its planet either) | `patrol`, `line_a`..`line_c` (none), `area` (a square of 24 round it) |

```c
/* level.txt: import blarg.serpent / path pen 2 -20 0  14 -20 0  14 -8 0  2 -8 0 */
uint32_t m = api->import_spawn(api->import_class("blarg.serpent"), at, yaw);
if (m && RAC1_API_HAS(api, object_bind_path)) api->object_bind_path(m, "pen", api->path_find("pen"));
```

### Classes and objects (ABI 2)

| Service | Does |
|---|---|
| `int class_define(const rac1_class_def* def)` | the behaviour of the level class `def->name` (copied; call again with the same name to replace it). Call in `rac1_mod_main`. 1 done, 0 not (no name, 64 classes already) |
| `int32_t class_find(const char* name)` | the current level's class of that name: its index (0.. in level.txt's order), -1 |
| `uint32_t object_spawn(int32_t cls, const float pos[3], float yaw)` | a new object: its handle, its class's `spawn` run first (with `placed` 0, `params` ""); 0 when none (no such class, 256 objects, the game's mobys all used) |
| `void object_remove(uint32_t obj)` | its class's `remove`, then it is gone (its moby freed, its state freed). May be called from its own callbacks |
| `uint32_t object_moby(uint32_t obj)` | its moby: position +0x10, rotation +0x40, scale +0x2c and the moby services (do not `moby_remove` it) |
| `uint32_t object_of_moby(uint32_t moby)` | the object a moby carries, 0 none (what `collide_ray` uses) |
| `int32_t object_class(uint32_t obj)` | its class's index, -1 no such object |
| `void* object_state(uint32_t obj)` | its state: its class's `state_size` bytes, zeroed at its spawn; NULL when the class has none |
| `int object_param(obj, key, out, cap)` | a placed object's `key=value` word: the value into `out` (cut to `cap`), 1 found, 0 not |
| `uint32_t object_named(const char* name)` | the placed object with `name=<name>`, 0 none |
| `void object_place(obj, pos, rot, scale)` | move it: `pos` (NULL kept), `rot` (x y z radians, NULL kept), `scale` (its model and box times this; <= 0 kept). Its matrix is made at once |
| `void object_show(obj, on)` | its model drawn or not (its collision stays) |
| `void object_solid(obj, on)` | its collision on or off (a ghost class's objects have none to turn on) |
| `int32_t object_count(void)`, `uint32_t object_at(int32_t i)` | the live objects, 0..count-1 (the order changes as objects come and go) |

### Hits (ABI 2)

| Service | Does |
|---|---|
| `void hit_send(uint32_t moby, uint32_t kinds, float strength, uint32_t from)` | a hit record on `moby`, as the game's attacks write them: what an enemy, a crate or one of your objects reads as a hit. `kinds`: `RAC1_HIT_*` (the game's classes look for their own bits: props `RAC1_HIT_PROP`, enemies `RAC1_HIT_ENEMY`) |
| `int hit_take(uint32_t moby, uint32_t kinds, rac1_hit* out)` | the hit `moby` took this frame with one of `kinds`, into `out`, consumed: 1; 0 none. (Your objects' hits come to their class's `hit` callback: no need to ask.) |

### Triggers (ABI 2)

| Service | Does |
|---|---|
| `int32_t trigger_find(const char* name)` | its index (level.txt's order), -1 |
| `int trigger_inside(int32_t index)` | 1 while Ratchet is inside (as of this frame's test, before `frame`) |

## Objects

A **level object** is an object of one of the level's own classes. What it is, in the engine:

- **A moby** of the base level's bolt crate class (every level a format 1 mod builds on has it), which the game sees
  as any of its objects: Ratchet stands on it and bumps into it, the game's rays hit it, his weapons and wrench hit it
  (hit records), sounds can follow it. Its crate code never runs, the game does not draw it, and it is updated at any
  distance. Its collision is the crate's box, scaled to the class's `size`: a cube, `size` on each side, centred on the
  object's x and y, rising from its origin. (A `ghost` class has none.)
- **A model**: the class's model, drawn by the modern renderer (the GL and Vulkan renderers) at the moby's position,
  rotation and scale, lit and fogged as the level's meshes. The faithful renderer (a debug choice) does not draw it.
- **Behaviour**: the class's callbacks in your plugin, and a state block per object.

```c
typedef struct turret { float home[3]; int cooldown; } turret;

static void turret_spawn(void* user, uint32_t obj, const rac1_spawn* sp) {
    turret* t = api->object_state(obj);
    memcpy(t->home, sp->pos, sizeof t->home);
}
static void turret_update(void* user, uint32_t obj) {
    turret* t = api->object_state(obj);
    if (t->cooldown > 0) --t->cooldown;
    /* ... turn to Ratchet, object_place(obj, NULL, rot, 0) ... */
}
static void turret_hit(void* user, uint32_t obj, const rac1_hit* hit) {
    api->object_remove(obj);
}

RAC1_MOD_EXPORT int rac1_mod_main(const rac1_api* game, rac1_mod* mod) {
    api = game;
    mod->size = sizeof(rac1_mod);
    rac1_class_def d = {sizeof d};
    d.name = "turret";              /* level.txt: class turret turret.obj size 1.5 */
    d.state_size = sizeof(turret);
    d.spawn = turret_spawn;
    d.update = turret_update;
    d.hit = turret_hit;
    api->class_define(&d);
    return 0;
}
```

- **Binding.** At a level's load each of its classes takes the behaviour your mod's plugin defined under its name;
  failing that, another plugin's (in the load order); failing that, none (the log says which). One plugin may define
  classes for several levels: unused definitions cost nothing.
- **Order.** In a frame, objects run in the moby pass's order (by the carrier's class: all objects together, in no
  order you should rely on). A callback may spawn and remove objects, its own too: an object removed in `hit` gets no
  `update`; one spawned during the pass first updates next frame.
- **Moving one.** `object_place`, or write its moby's position / rotation and call `moby_moved`. Its model follows
  each frame. Angles may grow as you like (a yaw you add to every frame): `object_place`, `object_spawn`, `hero_place`
  and `moby_moved` fold them into -pi..pi. (The game's rotation matrix folds its angles once only: past about 3 pi / 2
  its sine runs away and the model grows huge. If you write +0x40..+0x48 yourself, keep them in range or call
  `moby_moved`.)
- **What the game does not do for it.** It does not fall, collide with the level or animate by itself: your `update`
  moves it (use `collide_ray` to find the ground). It is not remembered in a save: placed objects are there again
  each time the level loads (keep what was collected or broken yourself, in `save` / `load`, and `object_remove` it
  in `level_start`).

## Base level objects

A level on a base level other than Veldin is there for the base level's objects, its enemies first. Two ways:

- **Placed**: level.txt's `moby <class> <x> <y> <z> [<yaw>]` (docs/mods.md, Objects): there at the level's start,
  uids 1.. in the lines' order; `moby_by_uid(uid)` is 0 once one is gone.
- **At run time** (ABI 3): level.txt's `spawn <class> <count>` gives room for `<count>` live objects of the class at once.
  The composer places that many copies of the base level's first placement of the class; when the level has loaded the
  game takes them out of play, keeping each one's moby and private values (pvars) as the load left them. `base_spawn`
  starts one of them again at the place given, exactly as loaded (its class runs from its start: it notices Ratchet,
  attacks, takes his hits, dies, drops its bolts); `base_moby(handle)` is its moby until it is gone; its room is free
  again then (`base_free`). They are never saved: a level loaded again starts with all of them free.

```c
static uint32_t wave[3];
static void frame(void* user) {
    for (int i = 0; i < 3; ++i) {
        if (wave[i] != 0 && api->base_moby(wave[i]) == 0) wave[i] = 0;   /* died */
        if (wave[i] == 0) {
            float p[3] = {200.0f + 8.0f * i, 200.0f, 100.5f};
            wave[i] = api->base_spawn(1445, p, 0.0f);                    /* Kalebo III's robot */
        }
    }
}
```

**Kalebo III's robots (1445) guard a place.** Their first update keeps where they stand as their guard point (pvars
+0x1d0..+0x1d8); each frame they pick a target (Ratchet: its moby at +0x1c0, its position at +0x180..+0x188) and give up
for good (+0x1c4 = 2, which nothing clears) when it is more than 16 units from the guard point on the ground or more than
3 units above or below the robot: they walk back to the guard point and stand there. `base_spawn` starts each one fresh,
so its guard point is where it was spawned (tested: +0x1d0 is the spawn position after its first update), not the pool's
placement; but a robot spawned 17 units from Ratchet gives up at once. `base_hunt(base_moby(h), 1)` makes it hunt him
anywhere: after each frame's updates the game puts its guard point on Ratchet and clears its giving up. Kept until the
moby is gone (the level left, killed, removed); `base_hunt(moby, 0)` stops it. The robots walk straight at Ratchet (no
paths): a wall between stops them. The game updates a moby only within its update distance of the camera (moby +0x30,
64 units for the robots; 0xff: always): level.txt's `update_all` line ([Base levels](mods.md#base-levels)) updates every
moby of a level mod's level wherever the camera is.

What works per class is the base level's class's own business: an enemy that walks and fights on flat ground works; one
that follows the base level's paths, waits for another object, a mission or a cutscene does not, unless the game gives
it what it waits for, as for Kalebo III's enemies below (docs/mods.md, [Base levels](mods.md#base-levels) lists what was
tried on Kalebo III). Test with `RAC1_MODS=1 RAC1_MOD_CODE=1` and
`RAC1_SIM_INPUT=attack` (Ratchet swinging the wrench) headless.

### Kalebo III's enemies (ABI 5)

(Since ABI 8's game what follows is these classes' profiles, `data/pool/profiles/L16_*.txt`, and their host behaviours: the
same values and the same behaviour, checked by the arena example's challenges giving the same RAM hashes.)

On Kalebo III's own level its enemies wait for the level's data: its paths (the walks, the flights, the areas they watch),
its moby groups (the Gadgetron robots' doors), its markers, a trigger object that wakes them. A level mod has none of it
(the composer keeps the ship's flight path only), so a `base_spawn` of these classes on a Kalebo III base gives the object
what it reads: paths of its own (the game's path table's free entries, from its end; their points in the mods' block of
memory below the level mods' texts, 0xdf000..0xe0000, 4 KB, room for about 40 of these objects in all), its pvars' indices
set to them or to none. Each `spawn` line's objects get theirs when the level loads (`[sdk] Kalebo III's enemies: N pool
objects given paths of their own`; one with no room left is never started: `base_spawn` skips it). `moby` lines of these
classes are not given any (they stay as before: do not use them).

| Class | What | `base_spawn` (guarding) | `base_hunt(m, 1)` | Needs |
|---|---|---|---|---|
| 52 | the bomb bots: round, a red eye, they roll at Ratchet and blow up (they hurt him, die with it) | watches a square of 24 round where it was spawned: comes for him within it (12 units, 24 alerted) | its alert timer (+0x190) kept running: its target search takes Ratchet at any distance, no giving up | its area: a path of its own (+0x180) |
| 541 | the Gadgetron robots, crash-test-dummy markings; three weapons (`base_option(h, RAC1_BASE_WEAPON, w)`): 0 a bomb launcher (lobs bombs, class 281), 1 a club (hits what it touches), 2 a flamethrower (the default) | patrols between where it was spawned and 5 units ahead (the club walks back and forth on it), attacks Ratchet from there, killed for good (no lives) | its patrol's two points put beside Ratchet on its side (the launcher 9 units away, the flamethrower 3.5, the club through him), again when he has moved 4 units; it walks on its own floor's height | on their level: hidden in an alcove until class 654 opens their group's door and walks them out (+0xcc, +0xc4), the lives +0xc8, the markers +0xd0/+0xd4, the area +0xd8. Here: no door, no lives, no markers (a marker's turn is Ratchet), the area the whole level; shown and put in its weapon's patrol after its first update |
| 638 | the jetpack troopers: hover, shoot green bolts (class 49) | where it was spawned (not on a path, not carried), watches a square of 24 round it; it settles half a unit over the floor under it (its model hangs over that point, as on its own level), the same over a ramp, a step or a block | its alert timer (+0x2e0: range 255) kept running, its home 10 units from Ratchet on its side (a few units aside each, short of a wall between them), flown to (state 1) when he has moved 4 units | on their level: a random point of a path (+0x120), carried by a dropship (+0x128, read once at its start: state 10 is "aboard"; cleared once it is dropped). Their own code flies straight at its target at the target's height, through anything (their level's paths keep clear of walls): here the game takes back a move into a wall (the level's collision), sliding along it or rising over it, and keeps them over the floor |
| 1356 | the dropships: fly in, drop their troopers, shoot (class 50), can be shot down | spawned at its **landing point** (`pos`; it hovers 9 units above it), facing `yaw`: comes down from 60 units behind and 25 up, drops its troopers (`base_option(h, RAC1_BASE_TROOPS, n)`, n <= 4, from class 638's `spawn` room: a `spawn 638` line is needed) one every half second, flies off ahead and up and is gone | stays over the landing point after dropping them and fights (the area the whole level) until shot down; its troopers hunt too | on their level: an arrival and a leave path, an area to wait for and one to fight in (+0xd8, +0xdc, +0xd4, +0xd0), the troopers' indices (+0xc0..+0xcc) |

```c
/* a dropship with three troopers, all hunting; a robot with the bomb launcher */
uint32_t ship = api->base_spawn(1356, landing, yaw);                 /* level.txt: spawn 1356 2, spawn 638 8 */
if (ship) { api->base_option(ship, RAC1_BASE_TROOPS, 3); api->base_hunt(api->base_moby(ship), 1); }
uint32_t bot = api->base_spawn(541, where, yaw);                     /* level.txt: spawn 541 6 */
if (bot) { api->base_option(bot, RAC1_BASE_WEAPON, 0); api->base_hunt(api->base_moby(bot), 1); }
```

Known: a dropship that stays to fight can be hit by its own troopers' shots too (it falls after a while even untouched);
the troopers fly straight at their point (a wall in the way slid along or flown over, never through; one they cannot get
past for 3 s: they stay where they are until Ratchet moves on); the robots walk straight (a wall stops them), as 1445.
A dropship's troopers are 638's room: `base_free(638)` counts them; `base_handle` gives their handles.

## Hits and damage

The game keeps a ring of **hit records** (64 of 0x40 bytes): an attack writes one for the moby it reaches (its
attacker, its kinds, its strength, where, the push), and the moby's class reads its own in its update. The SDK uses
the same records, so your objects and the game's understand each other:

- **Ratchet's attacks on your objects**: before an object's `update`, if its moby has a record with one of the
  player's kinds (`RAC1_HIT_PLAYER`: his weapons, the wrench, explosions), its class's `hit` gets it (`rac1_hit`) and
  it is consumed. What a weapon's `strength` and `kinds` are depends on the weapon (the wrench's and the blaster's are
  not the same): treat any hit as a hit, or tell them apart by `kinds` if you need to.
- **Your objects hurting Ratchet**: `hero_damage(amount, from)`. The game's hero code decides (he blinks after a hit
  and cannot be hurt again at once).
- **Your objects hurting the game's objects**: `hit_send(moby, kinds, strength, from)` with the kinds the target's
  class reads (a crate breaks with `RAC1_HIT_PROP`). `hit_send` on your own object's moby reaches its `hit` callback
  (the example's test does that to stand in for Ratchet's wrench).
- **Any moby**: `hit_take(moby, kinds, out)` reads (and consumes) a record yourself.

## Sounds

The level's own sounds (level.txt's `sound` lines) are one sound bank the game's sound driver loads at the level's
first frame and unloads when it is left, played through **the game's own sound slots**: they get the same distance
falloff (your `range`), stereo pan, Doppler pitch, walls muffling them, reverb and 5.1 / 7.1 placement as the game's
world sounds. So:

- `sfx_play` returns -1 when the game would not start it: too far to be heard (past its range's `far` from the
  camera), or all 26 slots busy. A sound on a moby follows it while it plays.
- A **looping** sound plays until `sfx_stop`, or until the game lets it go when it is too far to be heard (as it
  does its own). To keep one on an object, check `sfx_active(slot, sound)` now and then and play it again (the
  example's sentry hum does).
- Stop the looping sounds your objects started in their class's `remove`.

## Structs

```c
typedef struct rac1_class_def {     /* (ABI 2) you fill it: class_define */
    uint32_t size;                  /* sizeof(rac1_class_def) */
    const char* name;               /* level.txt's class name (copied) */
    uint32_t state_size;            /* bytes of state per object, zeroed at its spawn (0: none) */
    void* user;                     /* given back to its callbacks */
    void (*spawn)(void* user, uint32_t obj, const rac1_spawn* spawn);
    void (*update)(void* user, uint32_t obj);
    void (*hit)(void* user, uint32_t obj, const rac1_hit* hit);
    void (*remove)(void* user, uint32_t obj);
} rac1_class_def;

typedef struct rac1_spawn {         /* (ABI 2) the game gives it: spawn */
    uint32_t size;
    float pos[3];                   /* where */
    float yaw;                      /* turned, radians about z */
    const char* params;             /* a placed object's key=value words ("" otherwise) */
    int32_t placed;                 /* 1: level.txt's object line; 0: object_spawn */
    int32_t index;                  /* (placed) its line's index among the object lines; -1 */
} rac1_spawn;

typedef struct rac1_hit {           /* (ABI 2) the hit callback, hit_take */
    uint32_t size;
    uint32_t from;                  /* the attacker's moby (0: none known) */
    uint32_t kinds;                 /* RAC1_HIT_* bits */
    float strength;                 /* the game's damage value */
    float point[3];                 /* where (0 0 0 when the attack does not say) */
    float dir[3];                   /* its push (may be 0 0 0) */
} rac1_hit;

typedef struct rac1_ray_hit {       /* (ABI 2) collide_ray */
    uint32_t size;                  /* set it: sizeof(rac1_ray_hit) */
    float point[3];
    float normal[3];                /* unit */
    uint32_t moby;                  /* 0: the level's collision */
    uint32_t object;                /* 0: not a level object */
} rac1_ray_hit;
```

`rac1_level_info` (ABI 1): `level`, `slot`, `level_id`, `mine`, `objects` ([Callbacks](#callbacks-rac1_mod)).

## Limits

| What | Limit |
|---|---|
| Base level objects (`spawn` lines) | as the base level places objects: the placed ones and the pools' together at most as many as it places |
| Level objects at once | 256, and the game's moby pool (the base level's: the objects share it with the game's own mobys) |
| Classes a level | as many as the host meshes allow; 64 behaviours (`class_define`) a plugin |
| A class's model | as `meshes.obj`'s meshes: 16 materials, 64 vertex colours, textures to 256 x 256 |
| Sounds a level | 64; 26 playing at once in all (the game's slots, its own sounds included) |
| A sound | 8000 .. 48000 Hz, mono (stereo is mixed down); the bank goes into the sound driver's memory (no fixed limit, keep it to a few MB) |
| Triggers | no fixed limit (tested once a frame each) |
| `ee_alloc` | 53 KB for all the plugins, for the whole run (54 KB up to ABI 3: the last KB holds the frame's lights, `light_add`) |
| Kalebo III's enemies' paths (ABI 5) | 4 KB of paths for the `spawn` lines' objects of classes 52, 541, 638, 1356 (about 40 objects), and the path table's free entries (176 less the level's) |
| Imported objects' level data (ABI 8) | a planet's paths: its own count, the rest of 176 for the paths spawns and binds add (given back when their object is gone); groups: 112 a planet; a bound path 64 points |
| Jetpack troopers' flight (ABI 5) | half a unit over the floor under them; a wall in their way flown over up to 14 units over the floor; a sudden drop (a ledge) followed down at 0.05 units a frame |
| Lights (`light_add`) | 31 a frame for all the plugins, within the point lights' 96 a pixel (the game's 8, the hand-placed and the per-class lights before them, the automatic emitters after) |
| HUD lines | 15 a frame (`hud_text`, `hud_text_ex` together), 255 characters each; `hud_text_ex`'s scale at most 8 |
| A save's bytes | about 64 KB a plugin |

## The game's memory

A moby is 0x100 bytes of the game's memory: +0x10 its position (x y z), +0x20 its state (negative: gone), +0x24 its
class data, +0x2c its scale, +0x34 its flags, +0x40 its rotation (Euler angles x y z), +0x74 its update function,
+0x78 its pvars (its class's own values), +0xa4 its hit record's index, +0xa6 its class, +0xb2 its uid, +0xc0..+0xe0
its rotation's rows (made from +0x40 by `moby_moved`). Some of the game's addresses (Veldin's, NTSC-U; on a level
mod's level the code is Veldin's, so they hold there):

| Address | What |
|---|---|
| `0x0015ed84` | the current level (a level mod's level: its base level, 0) |
| `0x0015ed98` | bolts |
| `0x001415f8` | Ratchet's health |
| `0x0013c940` | pad 1 (+0x1b0 held, +0x1c4 pressed) |
| `ov(0x0015ffd8)` | the first moby (Ratchet) |
| `ov(0x00166dc0)` | the camera's position |
