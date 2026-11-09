# Custom levels: code

A mod can carry **code**: a native plugin, written in C (or C++) against the game's small C interface,
[`sdk/rac1_mod.h`](../sdk/rac1_mod.h). It is how a [level mod](mods.md#level-mods) gets behaviour of its own (a moving
platform, a counter, a HUD line, its own rules for its objects), and how any mod can watch and change the game while it
runs. This page is for modders: what a plugin can do, how it is built and loaded, and how to test it. The example level
(`examples/custom-level/`) has one: [`plugin/example_plugin.c`](../examples/custom-level/plugin/example_plugin.c).
**The SDK's full reference** (every struct, service and callback, the lifecycle, the versions, the limits) is
[The mod SDK: reference](sdk.md).

**For players:** a mod with code runs a program of its own on your computer, with your rights, as any program you
download. The game keeps such a mod **off** when it first finds it; Options > Mods says "Contains code." in its box, and
turning it on asks again ("Sure? Cross: on"). Turn it on only if you trust where it comes from. The code is built on your
own machine from the mod's source by the setup, so you (or someone you trust) can read what it does.

## The plugin folder

```
mods/my-level/
  mod.json
  level/ ...                 (a level mod's level; a mod with code needs none)
  plugin/
    CMakeLists.txt
    my_level.c
```

`plugin/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_level C)
include(${RAC1_SDK}/rac1_mod.cmake)
rac1_mod_plugin(my_level my_level.c)
```

`rac1_mod_plugin` (in [`sdk/rac1_mod.cmake`](../sdk/rac1_mod.cmake)) makes a loadable module named `rac1_mod` (`.so`,
or `.dll` on Windows) with the game's float rules: no fused multiply-add (`-ffp-contract=off`), `-frounding-math`, no
fast math, no type-based alias analysis. Add C++ sources and `CXX` to `project()` if you want them; link only what
your plugin needs (the C library and libm are there).

## Building: the setup

`./build/rac1-native --setup --level-mods` (the setup's last step) composes the level mods and builds every mod's
plugin whose build is missing or older than its sources; `--mod-plugins` builds only the plugins, `--mod-plugin
<folder>` one, always. It runs CMake with the compilers and the generator the game was built with, into
`assets/generated/mods/<folder>/plugin/` (`rac1_mod.so`, `plugin.json` with the stamp of the sources, `build.log` with
the compiler's output; the build tree in `plugin-build/`). A plugin whose sources changed since is not loaded ("Needs
setup" in Options > Mods) until the setup builds it again.

A mod may instead ship `plugin/prebuilt/rac1_mod.so` (or `.dll`) and no `CMakeLists.txt`: the setup copies it. Prefer
the source: a prebuilt library is the author's machine's, and nobody can see what it is made from.

## Loading

At the game's start, every mod on whose code may run is loaded (`dlopen` / `LoadLibrary`), in the mods' load order. The
game calls the plugin's two exported functions:

```c
#include <rac1_mod.h>

RAC1_MOD_DECLARE_ABI()   /* rac1_mod_abi(): the ABI the plugin was built with */

RAC1_MOD_EXPORT int rac1_mod_main(const rac1_api* api, rac1_mod* mod) {
    mod->size = sizeof(rac1_mod);
    mod->frame = my_frame;   /* the callbacks you use; the others stay NULL */
    return 0;                /* anything else: not used */
}
```

- A plugin built for a newer ABI than the game's is refused ("needs a newer game"); an older one runs: both tables only
  grow at their end. Before using a service added after ABI 1, test `RAC1_API_HAS(api, service)`.
- A plugin that fails to load, lacks the two functions or returns non-zero is not used; the rest of its mod (its level,
  its textures) runs. Options > Mods shows the mod as "Not loaded" with the reason in its box; the log has a
  `[mods] <folder>: ...` line.
- In a test run (`RAC1_HEADLESS=1`) the code of mods is loaded only with `RAC1_MODS=1` **and** `RAC1_MOD_CODE=1`.

## Callbacks

Every callback gets your `mod->user` back. All are optional.

| Callback | When |
|---|---|
| `level_start(user, level)` | a level's first frame of play (its objects spawned; on a level with a ship, after its landing, Ratchet off the ship). `level->mine` is 1 on your own mod's level; `level->objects` its placed objects' count (uids 1..objects, the hero's skipped) |
| `level_end(user)` | the level is left (another level loads, a save is loaded) |
| `frame(user)` | once a frame of play (not while a menu is open), after the objects' updates |
| `camera(user)` | after the game's camera update: change the camera's memory here |
| `hud(user)` | the HUD is drawn: `api->hud_text` works only here. It may run twice a frame (see below): draw only |
| `object_update(user, moby, uid, cls)` | each frame, before one of your level's placed objects (level.txt's `moby` lines) is updated: return 1 to skip its class's own update (yours instead), 0 to let it run |
| `new_game(user)` | a new game starts: reset your progress |
| `save(user, buf, cap)` | a save is written: your bytes (up to `cap`, about 64 KB) into `buf`, their count returned. They go into `save<N>.pc` beside the memory card's save, under your mod's folder name |
| `load(user, data, size)` | a save is loaded: your bytes as saved, or `NULL` / 0 when it has none (reset) |
| `test(user, text, frame)` | testing: `RAC1_MOD_TEST=<text>` given, once a frame of play |
| `shutdown(user)` | the game ends |
| `trigger(user, index, name, inside)` | (ABI 2) Ratchet entered or left one of level.txt's `trigger` boxes |

A save's bytes of a mod that is off or missing are kept: saving again writes them back as they were.

## Services

`api` gives (see the header for the exact signatures):

| Group | Services |
|---|---|
| The log, files | `log(text)` (`[mod <folder>] text`), `mod_path(name, out, cap)` (a file of your mod's folder) |
| Memory | `ram` (read the game's memory inline: `rac1_lw`, `rac1_lh`, `rac1_lb`, `rac1_lwf`), `sw`, `sh`, `sb`, `swf` (write it), `ov(addr)` (Veldin's overlay address on the current level), `ee_alloc(size)` (game memory of your own, 53 KB shared by the plugins: take it once), `replaying()` |
| Objects (mobys) | `moby_by_uid`, `moby_spawn(class)`, `moby_remove`, `moby_moved` (after writing a position, rotation or scale), `moby_link` / `moby_set_link` (a pvar word naming another moby) |
| Collision | `collide_line(from, to, hit)`: the level's collision and the mobys |
| The hero | `hero()` (his moby), `hero_place(pos, yaw)`, `bolts()`, `bolts_add(n)`, `health()` |
| Sound, music | `sound(index, moby)` (the level's sound table: the base level's, so the indices change with the base level; ABI 2's `sfx_*` are the mod's own), `music(track)` (the level's music table) |
| Effects | `smoke(pos, vel, size)` (Veldin's smoke) |
| HUD | `hud_text(x, y, abgr, text)`: the small font, x 0..512, y 0..448, colour 0xAABBGGRR with alpha 0x80 opaque, plain ASCII |
| Camera | `camera_position(out)` |
| Pad | `pad_held()`, `pad_pressed()`: the game's bits (0x10 triangle, 0x20 circle, 0x40 cross, 0x80 square, 0x800 start, 0x1000 up, 0x2000 right, 0x4000 down, 0x8000 left, 1 L2, 2 R2, 4 L1, 8 R1) |
| Random | `random()`: numbers of your own (the game's random numbers untouched), the same each run from a level's start |
| The game's code | `call_port(veldin_addr, arg)`: one of the game's functions the game itself calls through a pointer (its objects' update functions, their draw callbacks: the port's dispatch table, `ovl_register_moby_fn`), with one argument; 0 when the port has no such function |
| (ABI 2) The level's objects | `class_define(def)` (a level class's behaviour: spawn, update, hit, remove, a state per object), `class_find`, `object_spawn`, `object_remove`, `object_moby`, `object_of_moby`, `object_class`, `object_state`, `object_param`, `object_named`, `object_place`, `object_show`, `object_solid`, `object_count`, `object_at` |
| (ABI 2) Queries | `collide_ray(from, to, ignore, flags, out)` (the point, the normal, the moby or object hit), `objects_near`, `mobys_near` |
| (ABI 2) Damage | `hero_damage(amount, from)`, `hit_send(moby, kinds, strength, from)`, `hit_take(moby, kinds, out)` |
| (ABI 2) The level's sounds | `sfx_find(name)`, `sfx_play(sound, moby, pos)`, `sfx_stop(slot)`, `sfx_active(slot, sound)` |
| (ABI 2) Triggers, time | `trigger_find(name)`, `trigger_inside(index)`, `frame_count()` |
| (ABI 3) The base level's objects | `base_spawn(class, pos, yaw)` (an object of the base level's class, its own behaviour: level.txt's `spawn` lines give the room), `base_moby(handle)` (0 once it died), `base_free(class)`: [The mod SDK](sdk.md#base-level-objects) |
| (ABI 4) The vendor, the pad, the HUD, lights | `vendor_open()`, `input_hold(on)` (your menu has the pad: Start and Select stay the game's), `hud_rect`, `hud_text_ex` (larger text, anchored left / centre / right for a wide HUD), `hud_text_width`, `base_hunt(moby, on)`, `light_add(pos, rgb, intensity, radius)`: [The mod SDK](sdk.md#the-vendor-the-pad-the-huds-panels-lights-abi-4) |

A moby is 0x100 bytes of the game's memory: +0x10 its position (x y z), +0x20 its state (negative: gone), +0x24 its
class data, +0x2c its scale, +0x40 its rotation (Euler angles), +0x74 its update function, +0x78 its pvars (its class's
own values), +0xa6 its class, +0xb2 its uid. Some of the game's other addresses (Veldin's, NTSC-U):

| Address | What |
|---|---|
| `0x0015ed84` | the current level (a level mod's level: its base level, `level.base`) |
| `0x0015ed98` | bolts |
| `0x001415f8` | Ratchet's health |
| `0x0013c940` | pad 1 (+0x1b0 held, +0x1c4 pressed) |
| `ov(0x0015ffd8)` | the first moby (Ratchet) |
| `ov(0x00166dc0)` | the camera's position |

## Your own objects and sounds (ABI 2)

A level mod can have **classes of its own**: a model (an OBJ), a hit box, and behaviour in the plugin. level.txt says
what there is, the plugin says what it does:

```
class sentry orb.obj size 1.2            (level.txt: a class, its model, its hit box)
object sentry 12 14 2 name=east          (an object of it, placed; key=value words for the plugin)
sound zap zap.wav range 6 60             (a sound of the level's own: a WAV, an .ogg or a VAG)
trigger ramp -21 -8 -1 -15 8 6           (a box: the trigger callback)
```

```c
rac1_class_def d = {sizeof d};          /* in rac1_mod_main */
d.name = "sentry";
d.state_size = sizeof(sentry);          /* your per-object state: api->object_state(obj) */
d.spawn = sentry_spawn;                 /* (obj, spawn: where, its key=value words) */
d.update = sentry_update;               /* once a frame */
d.hit = sentry_hit;                     /* Ratchet's attacks reached it */
d.remove = sentry_remove;
api->class_define(&d);
```

Each object is a moby of the game (Ratchet stands on it, his weapons hit it, the game's rays see it) drawn with your
model by the modern renderer; the sounds play through the game's own sound slots (distance, pan, walls, reverb). The
example's plugin has two classes (a sentry that watches Ratchet with a ray, zaps him, breaks into a gem; gems to
collect), seven sounds and a trigger. Everything about them: [The mod SDK: reference](sdk.md), "Objects", "Hits and
damage", "Sounds".

## The rules

- **Memory**: read it through `api->ram`, write it only through `sw` / `sh` / `sb` / `swf`. The renderer runs some of
  the game's code a second time in a frame (the widened view, the level of detail) inside a journal of the memory, and
  puts the memory back after: writes through the API are in that journal, writes through a pointer would not be.
- **Replays**: while the renderer replays (`api->replaying()` is 1) write none of your own variables: the game's memory
  is put back, yours would not be. Only `hud` may run then; `frame`, `camera`, `object_update` and the level callbacks
  never do. Make your HUD text in `frame` and only draw it in `hud` (the example does).
- **Floats**: your code runs on the game's thread with the FPU rounding toward zero, as the PS2's. The SDK's float
  rules keep your arithmetic the same on every machine; `sinf` and the like may still differ in the last bit between
  C libraries: do not feed them into the game's own state if a replay must be exact.
- **Threads**: call the API only from the callbacks (the game's thread).
- **Addresses** are Veldin's, the NTSC-U disc's. On a level mod's level the code is Veldin's, so they hold there.

## Testing

```
RAC1_HEADLESS=1 RAC1_MODS=1 RAC1_MOD_CODE=1 RAC1_GOTO=<level.id>@60 RAC1_MOD_TEST=<text> ./build/rac1-native
```

`RAC1_MOD_TEST` is given to your `test` callback each frame: let it log what you want to check, or do scripted steps.
The example plugin understands `log` (the platform's and Ratchet's places every second), `ride` (Ratchet put on the
platform at its 200th frame: he rides it) and `sdk` (the level's own objects step by step: Ratchet put by a sentry,
which sees and zaps him; three hits sent to it, so it breaks into a gem; Ratchet put on the gem, which he takes; then on
the ramp, the trigger; two rays; the objects, his health and bolts logged every second). `RAC1_LEVEL_SAVE_IMAGE=<frame>:<prefix>` writes the `.pc` items a save
would get, yours included.

## Untested yet

The plugins are built and run on Linux (GCC). The Windows code paths (`LoadLibraryW`, `.dll`, MinGW with
`-static-libgcc`) and macOS's (`dlopen` of the `.so` CMake makes for a module) are written but not run.
