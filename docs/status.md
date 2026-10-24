# Status

*State as of October 2026.*

**Every planet of the game is ported (0.3.0), and almost all of its code (0.4.0).** Milestone 1, the
game complete up to Umbris, was reached first (Veldin, Novalis, Aridia, Kerwan, Eudora, Rilgar,
Blarg / Nebula G34 and Umbris; see [Roadmap](roadmap.md)); Batalia, Gaspar and Orxon followed in
0.2.0, and the rest of the game, from Pokitaru to the Veldin finale, the credits and the end menu, in
0.3.0. 0.4.0 fixes the bugs reported on 0.3.0 (see the [Changelog](../CHANGELOG.md)), and of the code
the game runs, one function is left to port (see the [Port inventory](port-inventory.md)). The
whole game has been played through to the end on 0.4.0 (Linux), and the start of a New Game+
checked; the planets after Orxon have had one full playthrough so far, so expect some bugs and
glitches there; the port as a whole still needs polishing. 0.5.0 adds the launcher for Windows and
Linux, an experimental Vulkan renderer (opt-in), a widescreen HUD option, a reworked modern
lighting, 5.1 / 7.1 sound and a faster game update, and fixes the bugs reported on 0.4.0 (see the
[Changelog](../CHANGELOG.md#050)); it has been playtested on Linux. Windows is no longer
experimental: players have played through the game on it.

Every planet of the milestone has its own classes ported and proved against the original code, and
runs on its own overlay. The gaps that remain on them are listed below as **known issues**; they are
bugs to fix, not missing parts of the milestone.

## Level names

The code and the game's data name the levels by their folder on the disc (`levels/<n>_<name>`),
and some of those names differ from the planet or location the player sees in the game. This table
is the one place that maps them; other pages link here.

| Level | Name in the code | Name in the game |
|---|---|---|
| boot module | the boot module (not a level) | The start of the game: intro movie, title screen, main menu, New Game, story cards |
| 0 | Veldin (tutorial) | Veldin |
| 1 | Novalis | Novalis |
| 2 | Aridia | Aridia |
| 3 | Kerwan | Kerwan (its city is called Metropolis) |
| 4 | Eudora | Eudora |
| 5 | Rilgar | Rilgar (its city is Blackwater City, where the hoverboard race is) |
| 6 | Nebula G34 | Blarg's station in the Nebula G34 galaxy |
| 7 | Umbris | Umbris |
| 8 | Batalia | Batalia |
| 9 | Gaspar | Gaspar |
| 10 | Orxon | Orxon |
| 11 | Pokitaru | Pokitaru |
| 12 | Hoven | Hoven |
| 13 | Oltanis Orbit | Gemlik Base |
| 14 | Oltanis | Oltanis |
| 15 | Quartu | Quartu |
| 16 | Kalebo III | Kalebo III |
| 17 | Veldin Orbit | Drek's fleet |
| 18 | Veldin, the finale | Veldin, the return at the end of the game (called "Veldin II" in some notes; unverified) |

The rest of this page uses the in-game name first and the code's name in brackets where they differ.

## The planets

Levels 0 to 7 are Milestone 1, and the whole milestone has been played through from Veldin to Umbris (included): apart from a few small bugs and glitches it plays without major problems. Batalia, Gaspar and Orxon (0.2.0) have been played through too.

> **The planets after Orxon (0.3.0; played through for 0.4.0):** Pokitaru, Hoven, Gemlik Base, Oltanis, Quartu, Kalebo III, Drek's fleet and the Veldin finale have their own code ported and proved against the original, and automated runs on each of them reach no unported code of the level. The crashes reported on 0.3.0 (under water on Quartu, Kalebo III's landing scene) are fixed in 0.4.0, and the whole game has been played through by hand to the end on 0.4.0 (Linux), the start of a New Game+ included. It is one playthrough so far, so expect some bugs and glitches. Bug reports are welcome.

Every one of the game's 19 levels (0 to 18) loads and runs on its own overlay (the level's own build of the
engine, see [Architecture](architecture.md#per-level-overlays-and-the-ov-macros)), without crashing,
in automated runs that idle, walk and attack. What differs from planet to planet is how much of its
own code is ported. Code that is not ported yet is a `TODO_PORT` stub: it logs once and does
nothing, so an object whose class is not ported stands still or does not appear.

| # | Planet | State | Known gaps |
|---|---|---|---|
| 0 | Veldin | Complete: boot, intro movie, title screen, the opening, the tutorial and the ending movie to Novalis. | |
| 1 | Novalis | Complete. | |
| 2 | Aridia | Complete. | |
| 3 | Kerwan | Complete. | |
| 4 | Eudora | Complete. | |
| 5 | Rilgar | Complete, including the hoverboard race. | |
| 6 | Nebula G34 (Blarg's station; see [names](#level-names)) | Complete, including the boss fight. | |
| 7 | Umbris | Complete, including the Snagglebeast. | |
| 8 | Batalia | Ported: its own classes and their draw callbacks (the turret's sights and radars, the Trespasser puzzle, the Sandmouse), the rain, the Metal Detector's HUD element. Its exits (the Commando to Orxon, the deserter to Gaspar, the ship) reached in automated runs. Played through. | The turret challenge has not been won in an automated run. A few of its classes are never placed on the level, so the original could not be compared with them. |
| 9 | Gaspar | Its own classes ported, with their draw callbacks (the lava's animated texture, the background ships' glows, the Visibomb's static) and the Pilot's Helmet pickup. Played through. | |
| 10 | Orxon | Its own classes ported, with their draw callbacks: Clank's section, and Ratchet's part once he has the O2 mask. Played through. | |
| 11 | Pokitaru | Its own classes, hero code, sound emitters and camera hook ported; the weapons and gadgets fire there without unported code; the PDA's call of the vendor screens (hero item 0x20) is no longer a stub there. Played through (also by a tester on 0.3.0, #48). | The Jet Fighter section and the Thruster-Pack lock have not been reached in an automated run. A few of its classes are never placed on the level, so the original could not be compared with them. |
| 12 | Hoven | Its own classes (with their draw callbacks), its own build of the hero code, sound emitters, camera hook and class hooks ported. No unported code of its own met in automated runs next to each of its classes. Played through (also by a tester on 0.3.0, #48). | A few of its classes are never placed on the level, so the original could not be compared with them. |
| 13 | Gemlik Base (code: Oltanis Orbit) | Its own code ported: its 60 classes with their draw callbacks, Qwark's space fight (the player's ship, Qwark's ship and his health bar), camera type 0x13, its sound emitter, the camera mode hook, class 63's carry hooks; particle type 0x44 (shared, the ship's engines). Played through (also by a tester on 0.3.0, #48). | Qwark has not been beaten in an automated run (the fight runs; no scripted run aimed the ship's lasers at him). |
| 14 | Oltanis | Its own code ported: its own build of the hero controller (24 functions, with the RAC 3 movement hook points), its 59 classes with their draw callbacks, camera types 0x13 / 0x14 / 0x15, four sound emitter types, the camera mode hook, class 252's carry hooks, the arrival scene's extras. Played through (also by a tester on 0.3.0, #48). | Not played through end to end in an automated run yet. |
| 15 | Quartu | Its own code ported: its own build of the hero controller (with the RAC 3 movement options), its classes and the callbacks they install (the draw callbacks among them), its camera types, sound emitters and class hooks, and particle type 0x47 (shared: the drek-veldin work's port, also proved on Quartu's copy). Giant Clank's section played in automated runs. Played through for 0.4.0. | A few of its classes are not placed on the level (spawned by others or by scenes): proved on mobys the check spawns. |
| 16 | Kalebo III | Its own code ported: its own build of the hero controller (the hoverboard's, with the RAC 3 movement options), its classes and the callbacks they install, its camera types, sound emitters and class hooks. The hoverboard race and the Gadgetron headquarters reached in automated runs. Played through for 0.4.0. | The race has not been finished in an automated run. |
| 17 | Drek's fleet (code: Veldin Orbit) | Ported: its own build of the hero's code (12 functions), its own classes (59) with their draw callbacks, its camera type, sound emitter, class hooks and camera hook; generated from the disc; proved by the oracle (batches 260, 261, 271 and the hero batches 44 .. 55). Played through for 0.4.0. | |
| 18 | Veldin, the finale | Ported: its own build of the hero's code (26 functions), its own classes (51) with their draw callbacks, its camera type, sound emitters, class hooks and camera hook, the final boss (Drek's robot) and its health bar; the end of the game: the credits, the last movie and the menu after them (new game with the weapons and bolts kept, or back to before the fight). Generated from the disc; proved by the oracle (batches 270, 261, 271 and the hero batches 44 .. 55). Played through for 0.4.0. | The fight itself has only been reached by setting the boss's state in automated runs. |

### Fixed since the first playtest

- RAC 3 movement (Advanced): the wrench can no longer be thrown again before it is back in Ratchet's hand.

- The flight between planets: the loading screen shows the ship flying through space to the next
  planet, then the planet and its name (it was a dark screen with only the sound).
- The pause menu's 3D models: Ratchet holding the selected weapon or gadget, and the item models on
  the menus' pages, are drawn.
- The Trespasser puzzle (Aridia, Eudora) is drawn, takes its input and can be solved.
- The Suck Cannon pulls enemies in and stores them as ammo, on every level.
- Objects whose drawing the port had skipped are drawn: the Visibomb's out-of-range static, the
  Sandmouse's glows and the Thruster-Pack's jets.
- In widescreen, effects such as the ship's canopy glass and the glows no longer drift sideways as
  the camera turns.
- The Advanced page's movement options (RAC 3 movement, the strafe, modern aiming) now work fully on
  every level, including those with their own build of the hero's code.
- Cutscene subtitles are drawn when Options > Subtitles is on, in every language the disc carries.
- Clank's Gadgebot command menu (the ring opened with triangle while playing as Clank) is drawn and
  its commands reach the Gadgebots, on Nebula G34 and Orxon.

## Engine and PC features

| Area | State |
|---|---|
| Boot, intro movie, title screen, main menu, New Game and the story cards and movies to Veldin | Ported. |
| Quit Game (the pause menu's Options): back to the title screen and its main menu, as the original (the boot module read back from the disc, without the intro movie) | Ported; playtested. |
| Memory card, save and load, the save on planet change | Works, but may still change: the save files' format, location and size have not been checked by hand yet. See [Saves](saves.md). |
| Pause menu (with its 3D models), HUD, Vendor, the ship's planet menu, the loading screen's flight | Ported. Known issue: the planet menu's information panels draw empty. |
| Sound and music (989snd), dialogue streams, movies; 5.1 and 7.1 output | Working; see [Audio](audio.md). The sound mirror that keeps music playing through loads is still a proposal (see [Decisions](decisions.md#13-the-iops-sound-runs-twice)). |
| 3D rendering (the MRP) | The default renderer; see [Rendering](rendering.md). |
| The Vulkan renderer (Options > Graphics > Renderer) | Experimental, opt-in: the same picture as OpenGL, which stays the default and takes over when Vulkan cannot start; see [Vulkan](vulkan.md). |
| Modern lighting (optional) | Working, off by default; see [Lighting](lighting.md). |
| Options > Graphics (including Texture Detail and the widescreen HUD), Controls, Advanced; the Original / Modern / Experimental choice at New Game | Implemented, known issues may remain; see [Settings](settings.md). |
| Keyboard, mouse and controller mapping, remapping in Options > Controls | Implemented, known issues may remain; see [Input](input.md). |
| Debug tools behind `--debug-tools` (with the Profiler and Unlocks tabs) | Implemented; see [Settings](settings.md#debug-tools). |
| The launcher (Windows and Linux x86-64): install, update, play, saves | New in 0.5.0; see [The launcher](launcher.md). |

## Not done yet

- **Playtesting and polishing the second half of the game.** Every level's own code is ported and
  the game has been played through to the end once; the planets after Orxon need more playthroughs.
- **The code left to port.** Every level still reaches a few `TODO_PORT` stubs that are not its
  own code: some kernel and DMA calls the host does not need. The [Port inventory](port-inventory.md)
  (generated by `tools/inventory/port_inventory.py` from the call graph of the disc's code and a
  headless tour) counts about 4,250 unique functions behind the 43,500 copies of the boot module and
  the 19 overlays: all but one of those the game can reach are ported, replaced by host code on
  purpose (hardware, DMA, the movie library) or debug features (the one left is the boot module's
  copy of a help page's focus-in, to review). About 200 unique functions are not connected to anything
  (cut content and dead code), listed there too.
- **Not everything is native in the renderer.** The HUD, menus and some effects are still drawn
  through the GS replica, and the mobys' flattened shadow meshes are not drawn yet (see
  [Rendering](rendering.md#limits)).
- **Widescreen menus.** Options > Graphics > HUD spreads the HUD over a widened view; the menus, the
  pause screen and the Vendor stay 4:3 in the middle (see [Settings](settings.md#graphics)).
- **Platforms.** Linux x86-64; Windows x86-64 (see [Platforms](platforms.md)). A port
  to ARM64 and Android is in progress on the `port/android` branch (#78); the CI has a Linux ARM64
  job, not blocking yet. A Vulkan renderer is in, experimental and opt-in ([Vulkan](vulkan.md));
  Direct3D and macOS may come later.
- **Regions.** The North American disc (`SCUS-97199`) is the reference. The European PAL disc
  (`SCES-50916`) can now be used on its own, through its region table: the setup, the generated code
  and the game run from it, in the PAL game's 60 Hz mode. This is new and awaits playtesting. Still to
  do for PAL: the functions the PAL build changed run as the North American port, or, for ten
  generated ones, as stand-ins translated from the PAL code that are not yet proved; the 50 Hz mode; the
  levels after Umbris have not been tried. See [Setup](setup.md#the-european-pal-disc).
