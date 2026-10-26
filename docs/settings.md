# Settings

The port's own settings: three pages added to the game's Options (Graphics, Controls, Advanced), the
Original, Modern or Experimental choice at New Game, the update check, where settings are kept, and the debug tools.

## Removed compromises and enhancements

The port keeps two kinds of settings apart (see
[Overview](overview.md#faithful-to-the-games-code-not-to-the-ps2s-limits)):

- **PS2 compromises removed**, such as the 4:3 frame, short draw distances, small textures and
  fades that hid disc loading. Removing them changes the picture, not the game's state, and the
  original's way stays available. Most of these are on the Graphics page; Fades is on the Advanced
  page.
- **Enhancements**, which change the game's behaviour: the quick-select pause, modern aiming,
  Ratchet & Clank 3 movement, and, on the picture side, the modern lighting. They are optional, off by
  default, and labelled as changes.

## The pages

The three pages are added to Options in both the pause menu and the main menu, before Quit Game.
They are built from the game's own menu widgets and data (a copy of the Camera page), so they look
and behave like the game's own pages. Left and right change a row's value; Cross steps it forward.

### Sound: 5.1 and 7.1

The game's own Sound page (Options > Sound) is kept as it is; its last row, the game's Mono / Stereo switch, also
offers **5.1** and **7.1**: Cross steps Mono, Stereo, 5.1, 7.1, then Mono again. 5.1 and 7.1 are settings of this PC
version: the game's own byte stays at Stereo (it is in the game's save and its code reads it), so the game plays
exactly as in Stereo, and the choice is kept in the preferences (`audio_channels 6` or `8` in
`advanced_options.txt`), not in the saves. The text shown is "5.1" / "7.1" in every language. The sound device is
opened again with 6 or 8 channels (7.1 at most, PCM); the world's sounds come from their direction, the dialogue from
the centre, the music from the front (see [Audio](audio.md#surround-51-and-71)). With every enhancement off (the
headless runs) the row is the game's own Mono / Stereo.

### Graphics

`src/game/enhance/graphics.cpp`. Every row changes only what the host draws, never the game's state.

| Row | Values | Default |
|---|---|---|
| Renderer | OpenGL, Vulkan (experimental; from the next start, see [Vulkan](vulkan.md)) | OpenGL |
| Display | Window, Fullscreen | Window |
| Monitor | the connected monitors by name (DP-1, HDMI-A-1, ...) | the first one |
| Window Size | 640x480 (the original's) to 3840x2160 (16 sizes, 4:3 and 16:9 among them) | 960x720 |
| V-Sync | Off, On | On |
| Frame Rate Cap | 30, 60, 120, 144, 240, Unlimited | 60 |
| Render Resolution | x0.50 to x2.00 of the window's, in steps of 0.25 | x1.00 |
| Aspect Ratio | 4:3 Original, 16:9, Fill Screen | 4:3 Original |
| Fit to Screen | Black Bars, Stretch | Black Bars |
| HUD | 4:3 Original, 16:9, 21:9, 32:9, Fit Screen | 4:3 Original |
| Field of View | x0.75 to x1.50 of the original's | x1.00 |
| Level of Detail | Original, Low, Medium, High, Maximum | Original |
| Lighting | Original, Low, Medium, High | Original |
| Shadow Strength | 50, 60, 70, 80, 90, 100 (%) | 70 |
| Shadow Distance | Low, Medium, High, Maximum | Maximum |
| Dynamic Shadows | Off, On | Off |
| Secondary Lights | Off, Game's, All | All |
| Ambient Occlusion | Off, SSAO, SSAO+, HBAO, GTAO, GTAO 1/2 | Off |
| AO Intensity | 25, 50, 75, 100, 125 (%) | 75 |
| Sun Rays | Off, Low, Medium, High (more quality, strength and cost each step; Low has the strength of 0.4.x's Low, High of its High) | Off |
| Surface Detail | Off, Low, High (materials: shiny metal, wet ground and puddles, water, lava, mud; High adds reflections and the lava's heat shimmer; with Lighting not Original, see [Materials](materials.md)) | Off |
| Ambient Particles | Off, Low, High: our own dust, embers, fireflies, rain and snow on some levels, not in the original (see [Ambient particles](particles.md); `ambient_particles` in the preferences, item 50 of a save's block) | Off |
| Texture Detail | Full, Original (PS2) | Full |

- **The frame-rate cap** only paces the picture. The game itself always runs at 60 frames a second,
  and the port draws one picture per game frame (there is no interpolation between them): a host
  frame that comes before the next game frame shows nothing new and is not presented. So the picture
  changes 60 times a second at most whatever the cap: 120, 144, 240 and Unlimited all present 60
  pictures a second, on a precise 1/60 s slot (Unlimited too; up to 0.4.0 its presents came at
  the end of each frame's work, unevenly). Only 30 lowers it. On a 120 / 240 Hz display each picture is
  simply held for 2 / 4 refreshes; a counter that reads 60 (or a 240 Hz display that shows 60) is the
  game's rate, not V-Sync. Above 60 the cap applies only while the debug menu holds the game.
- **V-Sync** sets the swap interval: On waits for the display's refresh (no tearing), Off does not.
  On Wayland (KDE Plasma, GNOME) the swap interval is always 0, because an interval of 1 stops the
  game while its window is not shown; the compositor shows whole frames anyway, so Off never tears
  there and the row changes nothing while the game runs (with Unlimited it paces the held game at the
  display's refresh). Mesa's EGL has no tearing support on Wayland. On X11 and Windows, Off can tear;
  the driver can override the game's choice (Mesa: `vblank_mode=0` forces it off, `vblank_mode=3` forces it
  on; AMD / NVIDIA control panels). The log says what was set and what the swaps do:
  `[display] start: V-Sync off, swap interval 0 on Wayland (EGL) ...` at the start and at each
  change of the display settings, then `[display] presents: 60.0 a second; between them median
  16.67 ms ...; the swap waited ... ms` four seconds later (a swap that waits about a refresh each
  time means V-Sync is in effect, whatever was asked). The debug menu's stats overlay counts
  presents too (up to 0.4.0 it counted the host loop's turns, about twice the pictures shown with the
  cap Unlimited: 120 for 60).
- **Monitor** chooses which screen the window is on: fullscreen goes to it at its own mode, and in
  window mode the window is centred on it. GLFW cannot place a window on Wayland, where the
  compositor decides, so the window mode half does nothing there. The choice is kept by the
  monitor's name, not by its place in the list, because that order is not fixed; a monitor that is
  not connected when the game starts falls back to the first one.
- **Render Resolution** scales the 3D; the HUD stays at the window's resolution.
- **Aspect Ratio** Fill Screen widens the 3D to the window's shape without changing what the game
  sees; **Fit to Screen** applies when the chosen shape is not the window's. See
  [Rendering](rendering.md#widescreen-and-render-scale).
- **HUD** spreads the HUD over a wider area when the 3D is widened (Aspect Ratio 16:9 or Fill Screen).
  4:3 Original keeps it on the original's 4:3 frame in the middle; 16:9, 21:9 (64:27, an ultrawide
  2560 x 1080's) and 32:9 spread it over a box of that shape centred in the view; Fit Screen over the
  view's whole width. The box is never wider than the view (16:9 on a 16:9 view is Fit Screen). On a
  32:9 screen, 16:9 or 21:9 keeps the HUD near the middle of the view; Fit Screen puts it at the far
  edges. Only the elements the game anchors to a side move, each by the same amount, toward its
  side: the ammo and the weapon, the quick select (and Gadgebot's commands), the bottom left slot on
  the left; the bolts, the air meter, the Morph-o-Ray, the Metal Detector and the bottom right slot on
  the right. The elements the game centres stay in the middle (health, the help panel, the help
  messages, subtitles, the bosses' bars in the top middle slot). The game's own HUD layout decides:
  each of its 13 slots is aligned left, right or centred (see [Rendering](rendering.md#the-hud-in-a-widened-view)).
  Menus, the pause screen, the Vendor, cards, pictures, fades and the letterbox are not changed.
- **The full frame** is always shown, everything the PS2 drew, edges included. Until 0.4 an Overscan
  row (TV, the default, or Full Frame) could hide a thin strip at the left, right and top edges, as a
  TV of the time did; the row was removed. The one thing the game itself draws there that a TV hid, a
  thin strip of a HUD element as it slides away (the Suck Cannon's gauge, the air meter), is not drawn
  (see [Rendering](rendering.md)). The `overscan` key of older preferences and the Overscan item of
  older saves are read and ignored.
- **Level of Detail** draws more detail further away, without changing the game's own decisions. See
  [Rendering](rendering.md#level-of-detail).
- **Lighting and the eight rows after it** (Shadow Strength to Surface Detail) are the optional modern
  lighting; the eight only act when Lighting is not Original. See [Lighting](lighting.md) and, for
  Surface Detail, [Materials](materials.md).
- **Ambient Particles** works with any Lighting: our own particles, drawn over the world in play, never
  changing the game's state. See [Ambient particles](particles.md).

### Controls

The controller and keyboard bindings, mouse look, look inversion and mouse sensitivity. See
[Input](input.md#the-controls-page).

### Advanced

| Row | Values (default first) |
|---|---|
| Quick Select | Normal, Pause |
| Aiming | Original, Modern |
| Movement | Original, Strafing, RAC 3 |
| Fades | Fast, Original |

See [Advanced options](advanced.md) for what each one does and how it is built.

### Updates (main menu)

On the title screen's main menu (below New Game, Load Game and Options), **Updates** opens the update settings:

| Row | Values (default first) |
|---|---|
| Update Check | On, Off |
| Update Channel | Stable, Development |
| Check Now | checks GitHub immediately |

Those settings belong to the machine: they are kept in the preferences only, not with the saves.

## Original, Modern or Experimental mode

New Game opens a choice between three modes before the game starts
(`graphics_new_game_choice`). Each says that every setting can be changed at any time in Options.

| | Original | Modern | Experimental |
|---|---|---|---|
| Aspect Ratio | 4:3 Original | Fill Screen | Fill Screen |
| Level of Detail | Original | High | High |
| Texture Detail | Original (PS2) | Full | Full |
| Quick Select, Aiming, Movement | Normal, Original, Original | Pause, Modern, RAC 3 | Pause, Modern, RAC 3 |
| Modern lighting (Lighting, Dynamic Shadows, Secondary Lights, Shadow Distance, Ambient Occlusion) | off (the defaults) | off (the defaults) | Lighting High, Dynamic Shadows On, Secondary Lights All, Shadow Distance Maximum, Ambient Occlusion SSAO (Shadow Strength, Sun Rays, Surface Detail and Ambient Particles left as they are) |

Experimental is Modern plus the [modern lighting](lighting.md), which is experimental. No mode changes
the window settings or Fades. The mode chosen is kept with the save.

## The update check

When the player opens the main menu on the title screen (or presses **Check Now** in the main menu's
**Updates** page), the game asks GitHub whether there is something newer than the installed build. When there
is, a panel at the main menu says so: the new version (or, on the development channel, how many new
commits there are on `main`), the release notes in short, and three choices:

- **Update Now** quits the game, runs `./setup.sh --update --play` (`setup.cmd --update --play` on
  Windows) to fetch the new sources and rebuild only what changed, and restarts the game when it is
  done. If the update fails (network, build, local changes), the previous build is kept as it was.
- **Not Now** closes the panel (it comes back at the next start).
- **Skip Version** closes it for good for that version.

Details:

- **What is sent:** one anonymous HTTPS request to the GitHub API (`api.github.com`), for this
  repository only: `GET /repos/RAC-Native/rac1-native-game/releases/latest` (Stable), or the
  comparison of the installed build with `main` (Development). Nothing else: no identifier, no
  telemetry, nothing about the player or the machine (GitHub sees the request's IP address, as for any
  web request). The answer is kept for 6 hours in `update_check.txt` (in the settings' folder below):
  an automatic check within them asks nothing (**Check Now** always asks GitHub).
- **When:** once when opening the main menu on the title screen, in the background (`tools/update_check.py`,
  Python's standard library, run by `src/host/update_check.cpp`), when the channel is switched in **Updates**,
  and on demand with **Updates > Check Now**: start-up never waits for it. No network, an error or GitHub's
  rate limit: nothing is shown. Never in a headless run. The panel is shown at the main menu only, never
  during play.
- **Turning it off:** Main menu > Updates > Update Check = Off (in `advanced_options.txt`:
  `update_check_off 1`).
- **The channel:** Main menu > Updates > Update Channel. Players can switch between **Stable** (the
  latest release) and **Development** (`main`) at any time. A developer clone on a branch other than
  `main` stays on its branch.
- **The installed version** is printed at start (`[orig] rac1-native 0.1.0 (v0.1.0-10-g7dee3d1, main)`):
  the release's version and, for a git clone, `git describe` and the branch, recorded by the build
  (`cmake/version.cmake`).
- **Skip Version** is kept in `advanced_options.txt` (`update_skip <tag or commit>`); remove the
  line (or use **Updates > Check Now**) to see the notice again.

To try the panel without a new release: `RAC1_UPDATE_FAKE=0.9.0` (or `dev:12`) with `--debug-tools`
(see [Environment variables](env-vars.md)).

## Where settings are kept

| What | Where |
|---|---|
| Graphics page | `~/.config/rac1-native/graphics_options.txt` |
| Advanced page (and the update check's Skip Version, the Sound page's 5.1 / 7.1) | `~/.config/rac1-native/advanced_options.txt` |
| The update check's last answer | `~/.config/rac1-native/update_check.txt` |
| Controls | `~/.config/rac1-native/controls.txt` (see [Input](input.md#the-bindings-file-controlstxt)) |
| The picture settings, the Advanced settings and the mode, per save | `save<N>.pc` next to each save (see [Saves](saves.md#the-pc-settings-next-to-each-save)) |
| The game's own options | the game's own settings file on the memory card, as on the PS2 |
| The language | `~/.config/rac1-native/console.txt` (see below) |

`~/.config` stands for `$XDG_CONFIG_HOME` when it is set. On Windows the folder is
`%APPDATA%\rac1-native\`. `RAC1_GRAPHICS_FILE`, `RAC1_ADVANCED_FILE` and `RAC1_CONTROLS_FILE` point
to other files.

The window's own settings (display mode, window size, v-sync, frame-rate cap) belong to the machine
and are kept only in the preferences. The picture settings (render resolution, aspect ratio, fit, HUD,
field of view, level of detail, the lighting rows, texture detail) and the Advanced settings are also written into
each save's `save<N>.pc`, so loading a save brings back the settings it was played with. A save
without that file (an older one) keeps the current settings.

**The language.** As on the PS2, the game doesn't keep its language in its save: at the start it
takes the console's language. The port has no console, so it keeps one for it: the language last
chosen in the game's Options is kept in `console.txt` and is the console's language at the next
start, as if the player had set their PS2 to it. The first time, it is the system's language when
the disc has it (English, French, German, Spanish or Italian), else English. A headless run always
starts in English and keeps nothing; `RAC1_LANGUAGE` (testing) sets the language for one run.

## Debug tools

The debug tools are for development and testing. They are off unless asked for with
`--debug-tools` on the command line or `RAC1_DEBUG_TOOLS=1` in the environment (the gate is
`debug_tools_gate` in `src/host/orig_main.cpp`, which runs before anything else):

```bash
./build/rac1-native --debug-tools
./build/rac1-native --debug-tools --level 3    # start directly on Kerwan
```

| | Without the debug tools | With `--debug-tools` |
|---|---|---|
| The debug menu (F1, or R3 on the pad) | not available; R3 is the game's | open/close |
| Moving Ratchet by hand (F2, or L3) | not available; L3 is the game's | on/off |
| Renderer choice (MRP or the faithful VU path), layer toggles, textures' mip choice | the MRP, everything drawn | in the debug menu's Rendering tab |
| The light editor | not available | the debug menu's Lights tab |
| The Profiler (frame time, CPU and GPU per stage, threads, memory, exports) | not available | the debug menu's Profiler tab, and its overlay |
| Giving and taking back items, goodies, weapons and ammo, gold bolts and gold weapons, skill points, cheats (unlocked and switched on), the Challenge Mode (the pause menu's Goodies), the planets on the ship's map; "Unlock everything" | not available | the debug menu's Unlocks tab |
| Hoverboard anywhere (the race's hoverboard ridden away from the race; Rilgar and Kalebo III only) | not available | the debug menu's Player tab |
| `--level N` | ignored, with a message | starts on level N |
| `RAC1_*` testing variables | removed from the environment at start, with a message | honoured |

The debug menu's code is in `src/_devtools/` and can be left out of the build entirely with
`-DRAC1_DEVTOOLS=OFF`.

**Environment variables a player may use** (always honoured): `RAC1_ISO` (the disc image's path),
`RAC1_MEMCARD_DIR` (the memory card's directory), `RAC1_GRAPHICS_FILE`, `RAC1_ADVANCED_FILE`,
`RAC1_CONTROLS_FILE`, `RAC1_RENDERER`, `RAC1_WINDOW`, `RAC1_FULLSCREEN`, `RAC1_VSYNC`, `RAC1_FPS_CAP`,
`RAC1_NO_AUDIO`, `RAC1_AUDIO_DEVICE`, `RAC1_AUDIO_PERIOD_MS`, `RAC1_AUDIO_PERIODS`,
`RAC1_MOVIE_VOLUME`, `RAC1_PRIORITY` and `RAC1_IOP_MIRROR`. `RAC1_HEADLESS` runs without a visible
window for automated tests, and also allows the testing variables.
