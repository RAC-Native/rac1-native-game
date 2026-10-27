# rac1-native

> **Current state (0.5.0):** every planet of the game is ported, from Veldin to the final fight with
> Drek, the credits and the end menu, and of the game's code the game runs, one function is left to
> port. **The whole game has been played through to the end** on 0.4.0. 0.5.0 brings a launcher for
> Windows and Linux, an experimental Vulkan renderer (opt-in), a widescreen HUD option, a reworked
> modern lighting (sun rays, ambient occlusion modes), 5.1 / 7.1 sound, a faster game update and
> the fixes from the 0.4.0 reports: see the [Changelog](CHANGELOG.md#050). It has been playtested on
> Linux; Windows is no longer experimental: players have played through the game on it. Expect some
> bugs and glitches.
> Since 0.5.0, `main` has gained (not released yet, see the [Changelog](CHANGELOG.md#unreleased)): the levels'
> water, lava and sand seas drawn by the native renderer too (so it draws all the 3D in play), surface materials
> and emissive surfaces for the modern lighting, optional ambient particles (dust, embers, rain, snow), new level
> suns, a faster frame (the game's update on its own thread), the setup on the Steam Deck and other immutable
> Linux systems, and fixes.
> See [Status](docs/status.md).

A native PC port of **Ratchet & Clank** (PlayStation 2, 2002), for Linux and Windows.

The game's own code is ported to C++, function by function, from the original North American release
(`SCUS-97199`), and each port is proved against the original code. Support for the
European PAL release (`SCES-50916`) is new and awaits testing: it goes through a table of where the PAL
build put the same code (see [Setup](docs/setup.md#the-european-pal-disc)). It is not an emulator: all of the game's
code, the PS2's vector-unit microcode included, runs as native C++ while you play. The 3D is drawn by
a native renderer (OpenGL, or Vulkan as an experimental option) from the game's own decisions, which
makes widescreen, high resolutions, full-detail textures and longer draw distances possible without
changing how the game behaves.

<!-- Screenshot: to be added. -->

## How it works

1. **Setup, from your disc.** `./setup.sh` (`setup.cmd` on Windows) reads your own ISO. It extracts
   the assets (levels, textures, models, sounds) and, with the generators in `tools/codegen/`,
   translates much of the game's MIPS and vector-unit (VU) code into C++, instruction by instruction:
   about 2,150 of the 4,000 ported functions, most of the levels' own object code (the mobys'
   updates), skinning, culling and the VU microprograms among them. Nothing from the disc is in this
   repository; the generated code stays in `assets/generated/`.
2. **The rest is ported by hand.** About 1,600 functions, among them the engine's systems, the hero,
   the cameras, the menus and the code around the generated parts, are C++ written in this repository,
   function by function; some 250 more pair a generated body with hand-written code. Each function
   carries the address of the original it replaces (`// orig: 0x...`). The hand-written headers declare
   the generated functions too, so both halves call each other directly.
3. **Built together, run natively.** CMake compiles the hand-written and the generated code into one
   program. While you play nothing is emulated or interpreted: the ISO is only read for data (a
   level, streamed music and voices), and the PS2's hardware (DMA, GS, sound processor, memory card,
   pads) is replaced by host code in `runtime/`.
4. **A renderer of its own.** The 3D is not drawn by emulating the PS2's graphics chip: the renderer
   (OpenGL, or Vulkan) reads what the game has decided to draw (its objects, world, effects, water and
   lava) and draws it natively; only the 2D (HUD, menus, text) still goes through a replica of the
   PS2's graphics chip. That is what makes widescreen, a wider HUD, high resolutions and the optional
   modern lighting possible, while the game's own logic stays unchanged.
5. **Proved against the original.** The oracle runs the original code in an R5900 interpreter beside
   each port and compares the results bit for bit; the reference images and per-frame RAM hashes
   catch any change in behaviour.

The details are in [Architecture](docs/architecture.md) (the generated code, the region layer for
the PAL disc), [Rendering](docs/rendering.md), [The oracle](docs/oracle.md) and the
[Port inventory](docs/port-inventory.md), with every function listed in the
[Port catalog](docs/catalog/README.md).

## Status

**Every planet of the game is ported (0.3.0), and almost all of its code (0.4.0).** Every level's
code is ported and proved against the original code, and **the whole game has been played through
to the end** on 0.4.0, from Veldin to the final fight with Drek, the credits and the end menu, with
the start of a New Game+ checked too. Veldin to Orxon have been played through many times; the
planets after Orxon have had one full playthrough so far (on Linux), so more playtesting by players
is welcome. 0.5.0 (see the [Changelog](CHANGELOG.md)) adds the launcher, the experimental Vulkan
renderer, the widescreen HUD, a reworked modern lighting and 5.1 / 7.1 sound, and fixes the bugs
reported on 0.4.0; it has been playtested on Linux, and players have played its development builds
on Windows. What `main` has gained since is in the [Changelog](CHANGELOG.md#unreleased). Of the code the game runs, one function is left to port (see the [Port inventory](docs/port-inventory.md)).
The port as a whole still needs polishing. See [Status](docs/status.md) (which also maps the names
the code uses for the levels to the names in the game), the [Roadmap](docs/roadmap.md) and the
[History](docs/history.md).

### Tested on

The port is developed and tested mainly on one machine:

- **CPU:** AMD Ryzen AI 9 HX 470 (12 cores, 24 threads)
- **GPU:** the integrated AMD Radeon 890M
- **Memory:** 64 GB
- **System:** Ubuntu 26.04 LTS, Linux 7.0, Mesa 26.0 (radeonsi for OpenGL 4.6, RADV for Vulkan)

Windows is supported too: players have played through the game on it, and it is tested on a laptop (see [Windows](#windows)).

Other hardware, drivers and distributions should work, but they have not been tried yet. Reports
are welcome.

### Windows

The game builds and runs on Windows from the same sources, and players have played through it there;
the launcher installs and updates it with a double-click. Linux remains the main development platform,
so a bug that only Windows shows is still possible: reports are welcome. See
[Setup](docs/setup.md#on-windows) and [Platforms](docs/platforms.md).

### Modern lighting is experimental

The optional modern lighting (sun shadows, per-pixel lights, ambient occlusion, sun rays, emissive
surfaces and surface materials) is **EXPERIMENTAL**.
It is off by default, and with it off the picture is the original's lighting, unchanged. It is not
yet fully usable on some planets: Nebula G34 (Blarg's station) is still a mess, with some zones
pitch dark. That is not a porting priority and will be dealt with later. Every level runs with it,
with varying results. The sun is the light the game itself lights Ratchet with in his current zone,
turned by the level's own file where needed; the glowing objects are found automatically. See
[Lighting](docs/lighting.md), which lists how well it works on each level.

## Why C++ and OpenGL

This is a **port**: the game's own code, carried over to the PC and proved to behave the same. It is
not a recreation that copies how the game looks and feels. Remakes are a valid way to bring the game
to the PC, and some are very good, but they are a different thing. The choice of language and renderer
follows from that.

- **C++, because the original is C and C++.** The game was written in C and C++ and compiled for the
  PS2's processor. Porting it function by function means keeping its integer sizes, its
  wrap-around arithmetic, its structure layouts, and its data where the game expects it in memory.
  C++ does that directly, with nothing in between. It also gives control over the processor's
  floating point: the PS2 rounds toward zero, the PC normally to nearest, and differences that
  small hang the game ([Decisions](docs/decisions.md)). And it lets each port be compared with the
  original, side by side: the same inputs, the same memory, the same results.
- **OpenGL, because it is enough, and runs everywhere.** The renderer draws what the game's own code
  decides to draw. The costly work is in that code, not in talking to the graphics card, and the
  static world already sits on the GPU. OpenGL 4.6 covers what the renderer needs, compute shaders
  included, and it runs on every desktop GPU and Linux driver in use. The renderer is kept apart
  from the game code, so another graphics API can be added beside it: a
  [Vulkan](docs/vulkan.md) backend now is, experimental and opt-in (Options > Graphics > Renderer),
  drawing the same picture. OpenGL stays the default.
- **Other platforms and graphics APIs, later.** Direct3D or Metal renderers may come in the future.
  The code is portable C++, but other devices need more than a renderer:
  - **Graphics API.** Phones and handhelds speak OpenGL ES or their own APIs, not desktop OpenGL 4.6.
  - **Processor.** The game's memory is little-endian, like the PS2 and the PC.
  - **Memory and speed.** The device must have enough of both.

  Each port to another device is its own project. A port to ARM64 (Android) is in progress on the
  `port/android` branch, not on `main` yet (#78).
- **Every choice is written down.** The big decisions, and the ones we reversed, are explained in
  [Decisions](docs/decisions.md) and [History](docs/history.md).

## Legal

- This repository contains **no game assets and none of the game's code**: no executable, no
  levels, textures, models, sounds or movies.
- To play, you need **your own copy** of the game, as an ISO image you made yourself: the North
  American PS2 disc `SCUS-97199`, or the European PAL disc `SCES-50916`. Either disc alone is enough.
  The setup extracts and converts everything from it on your machine, including the part of the port
  that is a mechanical translation of the game's own code.
- One kind of file about the game is committed: the region table, `data/regions/SCES-50916.txt`.
  It lists where the PAL build put the code and data of the North American build the port is written
  against. The rule (Daino's decision): addresses, and the shifts between two builds' addresses, may
  be committed like `orig:` annotations (the region table); never bytes of code, data or text. The
  table holds addresses, shifts and the PAL executable's checksum, nothing else.
- *Ratchet & Clank* is a trademark of Sony Interactive Entertainment. This project is not affiliated
  with, authorised or endorsed by Sony Interactive Entertainment or Insomniac Games.

## Quick start

You need Linux or [Windows](#windows) (x86-64), a GPU with OpenGL 4.6, and your ISO of the game
(the North American or the European PAL disc). On the Steam Deck (SteamOS), Bazzite and other
immutable Linux systems the game is built in a distrobox container, which the launcher sets up: see
[Setup](docs/setup.md#steam-deck-bazzite-and-other-immutable-systems). The experimental Vulkan renderer, optional, needs a
Vulkan 1.3 driver, and `glslc` when the game is built (the setup lists it as optional): see
[Vulkan](docs/vulkan.md).

**The easiest way, on Windows and Linux, is the launcher**: download `rac1-launcher.exe` (Windows) or
`rac1-launcher-linux-x64` (Linux, x86-64) from the
[latest release](https://github.com/RAC-Native/rac1-native-game/releases/latest), open it, click
**Install** and choose your disc image. It downloads the game, builds it from your disc, keeps it up
to date and manages your saves: see [The launcher](docs/launcher.md). (The launcher is attached to
the releases from 0.5.0 on.)

By hand, on Linux:

```bash
git clone https://github.com/RAC-Native/rac1-native-game.git
cd rac1-native-game
./setup.sh --iso /path/to/your/ratchet.iso
```

The setup checks your system (and tells you what to install), checks your disc, generates part of the
game's code from it, builds the game and generates the game's data. Run it again after a `git pull`: it redoes only what changed. Then play from the repository's root:

```bash
./build/rac1-native
```

The full guide, with requirements and troubleshooting: [Setup](docs/setup.md). On Windows without
the launcher, run `setup.cmd` from the repository's folder, then play with `play.cmd`: see
[Setup](docs/setup.md#on-windows).

When the main menu opens on the title screen, the game checks in the background whether a newer version is out, and
says so there (it downloads nothing; this is new and still a work in progress). That is one anonymous request to GitHub's
API, no telemetry; Main menu > Updates > Update Check turns it off. See [The update check](docs/settings.md#the-update-check).

## Documentation

For players:

- [Setup](docs/setup.md): requirements, step by step, troubleshooting.
- [The launcher](docs/launcher.md): the one-file installer, updater and save manager, for Windows and Linux.
- [Settings](docs/settings.md): the Graphics, Controls and Advanced pages, the Original, Modern and Experimental modes,
  the update check, where settings are kept, the debug tools.
- [Input](docs/input.md): controllers, keyboard and mouse, rebinding.
- [Saves](docs/saves.md): where your saves are. Saving and loading work, but the save files may still change: they have not been checked by hand yet.
- [Platforms](docs/platforms.md): Windows, ARM64 and Android in progress, macOS later.
- [Advanced options](docs/advanced.md): the optional gameplay changes, including Ratchet & Clank 3
  movement and modern aiming.
- [Enhancements](docs/enhancements.md): the optional improvements, those available now and those
  planned after the faithful port.
- [Status](docs/status.md) (with the table of level names), [Roadmap](docs/roadmap.md) and
  [History](docs/history.md): how the project got here, in plain words.

For contributors, and for reverse-engineering and decompilation people:

- [Overview](docs/overview.md): the approach; port, emulation and decompilation; what "faithful"
  means; how a port is proved.
- [The oracle](docs/oracle.md): the proof of the ports against the original code; building and
  running it, adding a batch. Every change to ported game code ships with a batch.
- [Decisions](docs/decisions.md): the project's architectural decisions.
- [Architecture](docs/architecture.md): the source tree, the layers, the `// orig:` annotations, the
  per-level overlays.
- [Port inventory](docs/port-inventory.md): what is ported, what is left, what the game never runs.
- [Port catalog](docs/catalog/README.md): every function and moby class of the game, one by one, with its
  port and its oracle batches, and the host code that stands in for the PS2's hardware and libraries.
- [Game logic](docs/game-logic.md): the main loop, levels, mobys, the hero, the camera, cutscenes.
- [Rendering](docs/rendering.md): how a frame is drawn by the native renderer (the MRP).
- [Vulkan](docs/vulkan.md): the experimental Vulkan backend of the same renderer.
- [Performance](docs/performance.md): where a frame's CPU time goes, the threads beside the game's, the switches to compare,
  how to measure and how a speed-up is proved to leave the game as it was.
- [Lighting](docs/lighting.md): the original lighting and the optional modern lighting.
- [Materials](docs/materials.md): the modern lighting's surface materials (Surface Detail) and how to tag them.
- [Ambient particles](docs/particles.md): the optional dust, embers, rain and snow of our own.
- [Audio](docs/audio.md): the 989snd sound driver, streams, movies.
- [File formats](docs/formats/README.md): the game's formats and their decoders.
- [Environment variables](docs/env-vars.md): every `RAC1_*` variable, for players, debugging and
  automated tests.

## Licence

The project's own code is under the [MIT licence](LICENSE). The third-party components it uses are
listed, with their licences, in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Acknowledgements

This project stands on OpenGOAL, Wrench, PCSX2, Ghidra and its Emotion Engine plugin, and the
libraries it uses. See [Acknowledgements](docs/acknowledgements.md).
