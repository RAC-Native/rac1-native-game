# Roadmap

Where the project has been, where it is, and what comes next. For the planet-by-planet state, see
[Status](status.md); for the story of the project in plain words, [History](history.md).

## How the project got here

```mermaid
flowchart LR
    A["Setup and<br/>format research"] --> B["Hardware layer:<br/>EE memory, VU, GS, IOP"]
    B --> C["Faithful porting<br/>from the game's code"]
    C --> D["World rendering"]
    D --> E["Gameplay, engine<br/>systems, menus"]
    E --> F["Milestone 1:<br/>complete to Umbris<br/>(reached)"]
    F --> G["The rest of the game"]
```

1. **Setup and format research.** The disc's layout, the level containers, textures, models,
   collision and the overlays were documented, and decoders written (`src/formats/`).
2. **Hardware layer.** EE memory, the vector units, a GS replica for the 2D, the IOP's sound driver,
   the disc (`runtime/`).
3. **Faithful porting.** After early attempts to reimplement the game by observation failed, the
   project switched to porting the original code function by function, with every port proved
   against the original in an interpreter (see [Decisions](decisions.md), 2, 3 and 6).
4. **World rendering first,** so that every later port could be checked on screen; then the native
   renderer, the MRP.
5. **Gameplay, engine systems and menus:** the hero, mobys, weapons, camera, cutscenes, saves, the
   HUD, the pause and main menus, the per-level overlays.

## Milestone 1: the game complete up to Umbris (reached)

**Goal:** the game complete from the start to Umbris. **This milestone is reached.** Up to Umbris the planets can be visited in
different orders; after Umbris the story sends the player to Batalia.

**Planets in scope:** Veldin, Novalis, Aridia, Kerwan, Eudora, Rilgar, Blarg / Nebula G34 and Umbris.
The code and the game name some levels differently; see the [table of level names](status.md#level-names).

**What was asked of a planet:**

1. It is complete in play: from the arrival to every exit, with its missions, gold bolts, the
   Vendor, its secrets and special sections (the ship, Clank, the hoverboard races).
2. A full play-through reaches no `TODO_PORT` stub (checked automatically).
3. Every class of the level is ported and proved by the oracle.
4. A human playtest passes.

The known issues in [Status](status.md) are the places where a planet still falls short of this list.

**PC features that are part of the milestone:**

1. The debug tools only with `--debug-tools`; otherwise the player gets the game with the MRP.
2. Keyboard, mouse and controller support with a controller database, and a remapping page in
   Options.
3. A Graphics page: window or fullscreen, resolution, render scale, aspect ratio (4:3, 16:9 or
   filling the screen, with black bars or stretched), level of detail, field of view.
4. An "Original mode / Modern mode" choice at New Game.

All of these are implemented. A few known issues remain on the milestone's planets (listed per planet
in [Status](status.md)); they are fixes to make, not missing parts of the milestone.

**After the milestone:** the development repository is reorganised (old renderers and dead paths
removed), the documentation rewritten, and this public repository created with only what is needed
to build and play, plus the documentation.

## After Milestone 1

**The rest of the game is ported (0.3.0):** levels 8 to 18, up to the final fight, the credits and
the end menu. **0.4.0** was a round of fixes for the bugs reported on 0.3.0, and the
[Port inventory](port-inventory.md): every function of the disc's code sorted into ported, left to
port and never reached, and the code it found left to port, ported (one function remains).

**Since 0.4.0** (on `main`, for the next release; see the [Changelog](../CHANGELOG.md#unreleased)):
the launcher for Windows and Linux, which installs, updates and starts the game and manages the
saves; the modern lighting reworked (the level's own sun, interiors, Shadow Strength, lights per
object class, sun rays, six ambient occlusion modes); the sound driver closer to the PS2's, with 5.1
and 7.1 output; the full frame always shown; fixes from the reports. The repository moved to the
[RAC-Native](https://github.com/RAC-Native) organization, and Windows is no longer experimental.
Then: an experimental [Vulkan](vulkan.md) renderer, opt-in beside OpenGL; a widescreen HUD
(Options > Graphics > HUD); the Visibomb's view and the sand sea right in wide views; the frame-rate
cap's pacing; the game's own update 12 to 15 % cheaper on the CPU, the same state bit for bit (about
8 % more with `./setup.sh --native-arch on`); the debug menu's Profiler and Unlocks tabs.

What comes next, in no fixed order:

- **Playtesting and polishing** the planets after Orxon, and the fixes that come from it.
- **ARM64 and Android:** the port is in progress on the `port/android` branch, not on `main` yet
  (#78); the CI has a Linux ARM64 job, not blocking yet.
- **The last items of the inventory:** the one function left, and, as optional enhancements, the
  cut content and development features it lists (see [Port inventory](port-inventory.md)).
- **The renderer:** the HUD, menus and remaining effects drawn natively, the mobys' shadow meshes.
- **Testing on more machines:** the European PAL disc, Windows (now also through the launcher), and
  the Vulkan renderer (measured on one machine so far).

Ideas for later, from the project's list of [enhancements](enhancements.md) (each one optional and
off by default when it changes the game):

- higher frame rates by interpolating between the game's 60 Hz frames (the game's logic stays at
  60 Hz);
- HD textures, generated on the player's own machine from their own disc, never distributed;
- post-processing effects;
- a language switch with all the languages on the disc;
- optional revival of content the game carries but never shows;
- macOS builds (the plan: [Platforms](platforms.md));
- the PAL release's 50 Hz mode (the PAL disc already runs, in its 60 Hz mode), once the NTSC-U
  version is complete;
- other graphics backends (Metal, Direct3D) behind the renderer's interface, beside OpenGL and
  Vulkan.

## Not goals

- **A matching decompilation.** The code is rewritten as readable C++, not shaped to reproduce the
  original binary.
- **A full-system PS2 emulator.** PCSX2 exists. A targeted VU interpreter for not-yet-ported
  microcode is in scope, and temporary.
- **Distributing the game's assets.** Never. Everything is generated from the player's own disc.
- **The other Ratchet & Clank games** before this one is far along.
