# Settings

The port's own settings: the Language page and four pages added to the game's Options (Display, Graphics, Controls, Advanced), the
New Game choices (Original or Modern, then a graphics preset), the update check, where settings are kept, and the debug tools.

## Removed compromises and enhancements

The port keeps two kinds of settings apart (see
[Overview](overview.md#faithful-to-the-games-code-not-to-the-ps2s-limits)):

- **PS2 compromises removed**, such as the 4:3 frame, short draw distances, small textures and
  fades that hid disc loading. Removing them changes the picture, not the game's state, and the
  original's way stays available. Most of these are on the Display page; Fades is on the Advanced
  page.
- **Enhancements**, which change the game's behaviour: the quick-select pause, modern aiming,
  Ratchet & Clank 3 movement, and, on the picture side, our additions on the Graphics page (the modern
  lighting, surface detail, grass, ambient particles). They are optional, off by default, and labelled as changes.

## The pages

The four pages are added to Options in both the pause menu and the main menu, before Quit Game.
They are built from the game's own menu widgets and data (a copy of the Camera page), so they look
and behave like the game's own pages. Left and right change a row's value; Cross steps it forward.

Two kinds of settings, kept apart:

- **The machine's**: Display, Graphics, Controls, Language, Sound's 5.1 / 7.1, the update check, and the main menu's
  Advanced rows (the global ones). They are the player's preferences, set from the main menu and the pause menu,
  never taken from a save and never changed by loading one.
- **A playthrough's**: the gameplay rows of the pause menu's Advanced page. New Game's Original / Modern sets them, they
  are kept with each save and loading a save brings them back.

### Sound: 5.1 and 7.1

The game's own Sound page (Options > Sound) is kept as it is; its last row, the game's Mono / Stereo switch, also
offers **5.1** and **7.1**: Cross steps Mono, Stereo, 5.1, 7.1, then Mono again. 5.1 and 7.1 are settings of this PC
version: the game's own byte stays at Stereo (it is in the game's save and its code reads it), so the game plays
exactly as in Stereo, and the choice is kept in the preferences (`audio_channels 6` or `8` in
`advanced_options.txt`), not in the saves. The text shown is "5.1" / "7.1" in every language. The sound device is
opened again with 6 or 8 channels (7.1 at most, PCM); the world's sounds come from their direction, the dialogue from
the centre, the music from the front (see [Audio](audio.md#surround-51-and-71)). With every enhancement off (the
headless runs) the row is the game's own Mono / Stereo.

### Display

`src/game/enhance/graphics.cpp`. The window, the picture's shape, the view and the detail: the PS2's compromises removed. Every
row changes only what the host draws, never the game's state.

| Row | Values | Default |
|---|---|---|
| Renderer | OpenGL, Vulkan (optional; from the next start, see [Vulkan](vulkan.md)); on macOS Metal only ([Metal](metal.md)) | OpenGL (macOS: Metal) |
| Display Mode | Window, Fullscreen | Window |
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
  not connected when the game starts falls back to the first one. Two monitors with the same name
  are told apart by their place on the desktop: "Generic PnP Monitor (at 1920,0)".
- **Render Resolution** scales the 3D; the HUD stays at the window's resolution.
- **Aspect Ratio** Fill Screen widens the 3D to the window's shape without changing what the game
  sees; **Fit to Screen** applies when the chosen shape is not the window's. See
  [Rendering](rendering.md#widescreen-and-render-scale). 16:9 is not offered on a 16:9 screen, where it
  is Fill Screen's picture.
- **HUD** spreads the HUD over a wider area when the 3D is widened (Aspect Ratio 16:9 or Fill Screen).
  4:3 Original keeps it on the original's 4:3 frame in the middle; 16:9, 21:9 (64:27, an ultrawide
  2560 x 1080's) and 32:9 spread it over a box of that shape centred in the view; Fit Screen over the
  view's whole width. The box is never wider than the view (16:9 on a 16:9 view is Fit Screen), so
  the row offers only the widths narrower than the view, besides 4:3 Original and Fit Screen; a width
  chosen on a wider screen stays until it is changed. On a
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
- **Display Mode** was called Display until the page of that name came (the `display` key of the preferences).

### Graphics

`src/game/enhance/graphics.cpp`. One switch between the original's picture and the renderer with our additions, a preset, then
every addition. None of them changes the game's state.

| Row | Values | Default |
|---|---|---|
| Graphics | Original, Enhanced | Original |
| Preset | Low, Medium, High (Custom is shown when the rows below match none) | High |
| Lighting | Low, Medium, High: the modern lighting's sun shadows, their maps at 1024, 2048, 4096 (see [Lighting](lighting.md)) | High |
| Shadow Strength | 50, 60, 70, 80, 90, 100 (%) | 70 |
| Shadow Distance | Low, Medium, High, Maximum | Maximum |
| Dynamic Shadows | Off, On | On |
| Secondary Lights | Off, Game's, All | All |
| Ambient Occlusion | Off, SSAO, SSAO+, HBAO, GTAO, GTAO 1/2, SSAO 1/2, SSAO+ 1/2, HBAO 1/2, MXAO, MXAO 1/2 (the 1/2 ones at half resolution; MXAO with smooth normals, see [Lighting](lighting.md#mxao)) | MXAO |
| AO Intensity | 25, 50, 75, 100, 125 (%) | 75 |
| Sun Rays | Off, Low, Medium, High (more quality, strength and cost each step; Low has the strength of 0.4.x's Low, High of its High) | Low |
| Surface Detail | Off, Low, High (materials: shiny metal, wet ground and puddles, water - held still, without the game's rise and fall -, lava, mud; High adds the puddles' and polished metals' reflections and the lava's heat shimmer; see [Materials](materials.md)) | High |
| Water Reflections | Low, Medium, Medium (RT), High (RT): what Surface Detail's water mirrors. Low cube maps of the sky and the far scenery from fixed probes (one per zone of the level and body of water; nothing near, nothing that moves, never moving on its own); Medium the scene drawn again mirrored about the water at half resolution each frame (unlit, only where the water is), Ratchet and every moving object in it, the cube maps where it has nothing; Medium (RT) the reflection ray traced through the level's static world (ties, shrubs, terrain) and shaded from what it hits, at half resolution: no planar view's hole or cut and the least flicker, but no characters or objects, about 1 ms more than Medium at 5120 x 1440 on a Radeon 890M; High (RT) the same with Ratchet, the ships and every moving object traced too (see [Materials](materials.md#water-reflections), [Rendering](rendering.md#ray-tracing-phase-3-the-mobys); `water_reflections` in the preferences: 0 Low, 1 Medium, 2 Medium (RT), 3 High (RT)). The RT values are shown only with the Vulkan renderer on a GPU with ray queries and non-uniform sampled-image indexing; saved and played elsewhere (OpenGL, another GPU) they play as Medium, said once in the log | Medium |
| Grass | Off, Low, High: the ground a level's light file tags as grass gets a fine relief of blades (Low, no geometry), and on High also a few very low layers of blades near the camera (`grass` in the preferences) | High |
| Ambient Particles | Off, Low, High: our own dust, embers, fireflies, rain and snow on some levels, not in the original (see [Ambient particles](particles.md); `ambient_particles` in the preferences) | High |

- **Graphics: Original** draws the original's picture, byte for byte the same as with every addition off, whatever the rows
  under it hold: they are hidden (the page shows the switch alone) and ignored, not reset. **Enhanced** shows them and they
  apply. Switching keeps every row's value, so comparing the two is one press. The one gate in the code is
  `graphics_enhanced()` (`src/game/enhance/graphics.h`): the settings set the renderer's switches through it, and the debug menu's
  live switch is the same one. The Display page's rows apply in both.
- **Preset** sets every row under it at once (Shadow Strength and AO Intensity included). Changing any row by hand shows Custom;
  stepping the preset from Custom goes to Low (right) or High (left). The preset is not kept: it is what the rows match.
- **The defaults** of the rows under the switch are the preset High: what Graphics: Enhanced first shows. They do not matter
  while Graphics is Original (the default).
- **Grass and Ambient Particles** were outside the lighting's control before the switch: with Lighting Original,
  Ambient Particles still drew. Now every addition follows the one switch.
- **Water Reflections** is a row of its own, not Surface Detail's levels: Surface Detail's High still gives the puddles and the
  polished metals the last frame's picture (and the lava its shimmer), which the water no longer uses, and a player may want the water's
  reflections without that cost or the other way round; it acts only with Surface Detail on (its water material draws them).
- **Water** (Off, Low, High: our own water over or in place of the game's) was a row until 2026-11-02: Surface Detail's water
  over the game's own looked better. Its `water` key is read and ignored; what it fixed is
  Surface Detail's now (the water held still, Pokitaru's clear trapezoids: [Materials](materials.md#the-water-held-still)).

#### Presets

Chosen for their cost at 5120 x 1440 (the GPU times in [Lighting](lighting.md): a sun cascade about 0.5 ms each, GTAO about
3 to 4 ms, GTAO 1/2 about 2 to 3 ms, Sun Rays Low about 0.5 ms and High about 1.4 ms; the particles cost little).

| Row | Low | Medium | High |
|---|---|---|---|
| Lighting | Low (1024) | Medium (2048) | High (4096) |
| Shadow Strength | 70 | 70 | 70 |
| Shadow Distance | Medium | High | Maximum |
| Dynamic Shadows | Off | Off | On |
| Secondary Lights | Game's | All | All |
| Ambient Occlusion | Off | GTAO 1/2 | MXAO |
| AO Intensity | 75 | 75 | 75 |
| Sun Rays | Off | Low | Low |
| Surface Detail | Low | Low | High |
| Grass | Off | Low | High |
| Ambient Particles | Low | High | High |
| Water Reflections | Low | Medium | Medium |

High: Lighting High, Dynamic Shadows, all the lights, Shadow Distance Maximum, MXAO, Sun Rays
Low, Surface Detail, Grass and Ambient Particles High, Water Reflections Medium. Until 2026-11-02 it had GTAO and Sun Rays High (Daino: the rays too strong;
stronger ones are picked by hand); rows set to that old High now show Custom.

#### Before the switch: the preferences

Until the switch, the Lighting row had a fourth value, Original, which turned the modern lighting off; Water, Grass and Ambient
Particles did not follow it. Old files are read this way:

- `lighting 0` (Original) in `graphics_options.txt`: Graphics Original, the Lighting row at its default (High), every other
  row kept.
- `lighting 1` .. `3` (Low .. High): Graphics Enhanced, Lighting Low .. High, every other row kept.

The new keys are `graphics` (0 Original, 1 Enhanced) and `lighting_quality` (0 Low, 1 Medium, 2 High). The preferences still
write the old `lighting` key (0 when Original, else the Lighting row + 1) for the versions before the switch, which read only
that; a version with the switch reads the old key only when the new one is missing. A preferences file without `graphics` is
written again in the new form at the first frame. (The saves held the same as items 38, 55 and 56 until the graphics became
the machine's, 2026-11-02: read and ignored now.)

### Controls

The controller and keyboard bindings (with the PC's Next / Previous Weapon and the mouse wheel), look inversion, the
mouse's mode (Direct: it turns the camera and the aim by its movement; Stick: a simulated right stick), its
sensitivity and its sensitivity while aiming (mouse look itself is the `mouse_look` line of `controls.txt`). They are
the machine's, kept in `controls.txt`, not with the saves, and no New Game mode changes them. See
[Input](input.md#the-controls-page).

### Advanced

Two pages of the same name. **The pause menu's** holds a playthrough's settings, the gameplay rows:

| Row | Values (default first) | Kept |
|---|---|---|
| Quick Select | Normal, Pause | with the save |
| Aiming | Original, Modern, Modern, No Zoom | with the save |
| Movement | Original, Strafing, RAC 3 | with the save |
| Fades | Fast, Original | with the save |
| Buy All Ammo | Off, On | with the save |
| Vibration (experimental) | Off, Low, High | the preferences (the machine's pad) |
| Adaptive Triggers (experimental) | Off, On (shown only when the pad in use is a DualSense) | the preferences (the machine's pad) |
| Skip Cutscenes | Original, Always | with the save |

**The main menu's** holds the global settings, those of the whole game rather than of a playthrough, kept in the
preferences only:

| Row | Values (default first) | Kept |
|---|---|---|
| Cut Content | Off, On: things the game has on the disc but never shows, brought back (Kerwan's citizens so far); read at a level's load | the preferences |
| Corrected Texts | Off, On: the texts as the later discs correct them (shown only with a data disc that gives them: the PAL or the Japanese disc, beside the North American one); at once on the main menu | the preferences |
| 1.02 Fixes | Off, On: fixes of the game's later build (1.02, the Japanese and Platinum discs) on the North American code: New Game keeping the sound settings, explosions sparing moby slots, at most 20 Glove of Doom agents, the Thruster-Pack's jump sound, Ratchet's colour pulse in cutscenes, the talk prompt, the pause menu's goals | the preferences |
| 1.02 Gameplay | Off, On: the later build's fixes that change how the game plays (exploits stay possible with it off): the RYNO's targets, the Visibomb's steering, deadly ground for the Hologuise, the hoverboard and quicksand, the characters' talk guards, Ratchet's tilt to the ground, the Hologuise's use | the preferences |

All of the pause menu's rows are changed in the pause menu (New Game's Original / Modern sets them first); the main
menu's Options has an Advanced entry only while there is a global row.

See [Advanced options](advanced.md) for what each one does and how it is built.

### Language

Everything about language is on one page, **Options > Language**, in the main menu (the game's own Language entry
opens it) and in the pause menu (after Subtitles). Its rows step with left and right, as the other pages' rows:

| Row | Values | Where |
|---|---|---|
| Text | English, French, German, Spanish, Italian | main menu only |
| Voice | Same as Text (the default), English, French, German, Spanish, Italian, Japanese (only with the Japanese data disc); see [Voice](#voice) | both |
| Subtitles | Off, On | both |
| Movie Subtitles | Off (the default), On; see [Movie subtitles](movie-subtitles.md) | both |

- **Text** is the game's own language (`0x15ed88`): the menus, the HUD, the help and the subtitles' texts. It is
  what the game's own Language page chose (a list of the five with a tick), now a row; it is kept as the console's
  language (`console.txt`, below). As in the game, it is only in the main menu: a level's texts are copied for the
  language when the level loads, so it cannot change in the pause menu.
- **Voice** is the spoken language (below). It was the Advanced page's last row before.
- **Subtitles** is the game's own Subtitles switch (the pause menu's Options > Subtitles, one of the game's options,
  kept in its save), offered in the main menu too, before a game exists. The choice is kept in the preferences
  (`subtitles on` / `off` in `advanced_options.txt`) and put in the game's switch whenever the game reads a state: a
  New Game (the disc's default save, where they are off), a loaded save (its own value is replaced, also in a save
  from before this page), the title's. So what was chosen last, on this page or on the game's own Subtitles page
  (which changes the preference too), is what every game gets. Until it is chosen once, the preferences have no
  `subtitles` line and the game's own value stays (off for a new game, the save's for a load).

- **Movie Subtitles** is ours: the original never shows text over its movies. On, the movies show the subtitle files in
  `assets/movie-subtitles/`, in the text language: the ones the setup makes from your disc with speech to text when you
  ask for them (`./setup.sh --movie-subtitles`, or the launcher's Settings), or a subtitles pack's, or files you write
  yourself. A language without files shows none. Kept as `movie_subtitles on|off` in `advanced_options.txt`;
  `RAC1_MOVIE_SUBTITLES` overrides it. See [Movie subtitles](movie-subtitles.md).

The page is `src/game/enhance/language.{h,cpp}`. With every enhancement off (the headless runs) the game's own
Language page and Subtitles switch are there as they are.

### Voice

The game has one language, which picks both its texts and its voices. The **Voice** row of the Language page sets
the voices apart: the Text row keeps choosing the texts (the game's language, `0x15ed88`), and Voice chooses the
spoken language. **Same as Text**, the default, is the original: the voices follow the texts, and nothing of the game
changes. Any other value is an enhancement.

- **English, French, German, Spanish, Italian**: the North American disc (and the PAL one, the same recordings)
  carries all five, so any voice goes with any text.
- **Japanese**: offered only when the Japanese disc was given to the setup as a data disc
  ([Setup](setup.md#data-discs)). Its voices are read from that disc: the scenes' and the levels' dialogue, the
  Vendor's lines, the lines of the table of contents' two dialogue tables (the game's 30000 and 50000 lines), the movies' sound (the Japanese disc's movie of
  the same row, its pictures still the game's disc's), and Ratchet's own efforts (the wrench, jumps, falls: each
  level's sound bank is the Japanese disc's, where about ten of his samples differ). A line the Japanese disc lacks
  plays in the text language; a line the game's disc lacks is not added. A level's sound bank is read when the level
  loads: changing Voice to or from Japanese changes Ratchet's efforts from the next level (or a reload), the rest at
  once.
- The texts and the subtitles stay in the text language. The Japanese scenes' lines were recorded to the same scenes:
  their lengths match the North American ones within 0.05 s, except one Kalebo III scene (level 16, scene 2), whose
  Japanese line runs 1.5 s longer and is cut by the scene's end, as the game's own code ends it. Six lines Nebula
  G34 plays during play (not in a scene) are 1.5 to 4 s longer in Japanese, and the Japanese lines of the two tables
  are longer on average (a third longer for the 30000 lines): nothing cuts those, but a long one may still be
  speaking when the next starts.

The choice belongs to the machine, not to a save: `voice_language` in `advanced_options.txt` (`same`, `en`, `fr`,
`de`, `es`, `it`, `ja`). `RAC1_VOICE_LANGUAGE` (the same names) overrides it, also in a headless run. The code is
`src/game/enhance/voice_language.cpp`: every place the game picks a voice by its language asks it which sector to
play.

### Updates (main menu)

On the title screen's main menu (below New Game, Load Game and Options), **Updates** opens the update settings:

| Row | Values (default first) |
|---|---|
| Update Check | On, Off |
| Update Channel | Stable, Development |
| Check Now | checks GitHub immediately |

Those settings belong to the machine: they are kept in the preferences only, not with the saves.

## New Game: Original or Modern, then the graphics

New Game opens two windows before the game starts, in the game's own style (copies of its Options page).

**The first: Original or Modern.** Only the gameplay rows (the pause menu's Advanced page) are set:

| | Original | Modern |
|---|---|---|
| Quick Select | Normal | Pause |
| Aiming | Original | Modern |
| Movement | Original | RAC 3 |
| Buy All Ammo (the Vendor) | Off | On |
| Vibration | Off | High |
| Adaptive Triggers (a DualSense in use) | Off | On |
| Skip Cutscenes | Original | Always |

Fades is left as it is. The mode is kept with the save (item 37 of `save<N>.pc`).

**The second: the graphics preset.** It sets the Graphics page alone:

| Choice | The Graphics page |
|---|---|
| Current (selected first) | left as it is |
| Original | Graphics: Original (the rows under it kept, hidden) |
| Low, Medium, High | Graphics: Enhanced, that [preset](#presets) |

The Display page (aspect ratio, level of detail, texture detail, the window) is never touched by New Game. Triangle on the
second window goes back to the first. Both are built in `graphics_main_menu_install` and applied by
`graphics_new_game_choice` (`src/game/enhance/graphics.cpp`).

Until 2026-11-02 there was one window with three modes: Original and Modern also set the Display page (Original: 4:3,
original detail and textures; Modern: Fill Screen, LOD High, full textures) and a third, Experimental, was Modern plus
Graphics: Enhanced at the preset High. A save made in Experimental mode loads as Modern (its gameplay rows are Modern's)
and leaves the graphics alone.

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
- **The installed version** is printed at start (`[orig] rac1-native 0.7.0 (v0.7.0-12-g1a2b3c4, main)`):
  the release's version and, for a git clone, `git describe` and the branch, recorded by the build
  (`cmake/version.cmake`).
- **Skip Version** is kept in `advanced_options.txt` (`update_skip <tag or commit>`); remove the
  line (or use **Updates > Check Now**) to see the notice again.

To try the panel without a new release: `RAC1_UPDATE_FAKE=0.9.0` (or `dev:12`) with `--debug-tools`
(see [Environment variables](env-vars.md)).

## Where settings are kept

| What | Where |
|---|---|
| Display and Graphics pages | `~/.config/rac1-native/graphics_options.txt` |
| Advanced page, the Language page's Voice and Subtitles (and the update check's Skip Version, the Sound page's 5.1 / 7.1) | `~/.config/rac1-native/advanced_options.txt` |
| The update check's last answer | `~/.config/rac1-native/update_check.txt` |
| Controls | `~/.config/rac1-native/controls.txt` (see [Input](input.md#the-bindings-file-controlstxt)) |
| The pause menu's Advanced settings (the gameplay rows) and the mode, per save | `save<N>.pc` next to each save (see [Saves](saves.md#the-pc-settings-next-to-each-save)) |
| The game's own options | the game's own settings file on the memory card, as on the PS2 |
| The language | `~/.config/rac1-native/console.txt` (see below) |

`~/.config` stands for `$XDG_CONFIG_HOME` when it is set. On Windows the folder is
`%APPDATA%\rac1-native\`. `RAC1_GRAPHICS_FILE`, `RAC1_ADVANCED_FILE` and `RAC1_CONTROLS_FILE` point
to other files.

The Display and Graphics pages belong to the machine and are kept only in the preferences: loading a save never changes
them. The pause menu's Advanced settings (the gameplay rows but Vibration and Adaptive Triggers) and the New Game mode are
also written into each save's `save<N>.pc`, so loading a save brings back the way it was played. A save without that file
(an older one) keeps the current settings. Saves written before 2026-11-02 also hold the picture settings and every
Graphics row (items 32 to 56): they are read and ignored, the graphics stay the machine's.

**The language.** As on the PS2, the game doesn't keep its language in its save: at the start it
takes the console's language. The port has no console, so it keeps one for it: the language last
chosen in the game's Options is kept in `console.txt` and is the console's language at the next
start, as if the player had set their PS2 to it (the Language page's Text row, or the game's own page with every
enhancement off). The first time, it is the system's language when
the disc has it (English, French, German, Spanish or Italian), else English. A headless run always
starts in English and keeps nothing; `RAC1_LANGUAGE` (testing) sets the language for one run.

## Debug tools

The debug tools are for development and testing. They are off unless asked for with
`--debug-tools` on the command line or `RAC1_DEBUG_TOOLS=1` in the environment (the gate is
`debug_tools_gate` in `src/host/orig_main.cpp`, which runs before anything else), or from the launcher with
Settings > **Enable debug tools** ([The launcher](launcher.md)):

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
| Every graphics addition switched live, with its cost (Graphics: Original / Enhanced and the preset at the top, as the Graphics page; "Save as my settings") | not available | the debug menu's Enhancements tab |
| The Profiler (frame time, CPU and GPU per stage, threads, memory, exports) | not available | the debug menu's Profiler tab, and its overlay |
| Giving and taking back items, goodies, weapons and ammo, gold bolts and gold weapons, skill points, cheats (unlocked and switched on), the Challenge Mode (the pause menu's Goodies), the planets on the ship's map; "Unlock everything" | not available | the debug menu's Unlocks tab |
| Hoverboard anywhere (the race's hoverboard ridden away from the race on Rilgar and Kalebo III; a free board on every other level) | not available | the debug menu's Player tab |
| `--level N` | ignored, with a message | starts on level N |
| `RAC1_*` testing variables | removed from the environment at start, with a message | honoured |

The debug menu's code is in `src/_devtools/` and can be left out of the build entirely with
`-DRAC1_DEVTOOLS=OFF`.

**Environment variables a player may use** (always honoured): `RAC1_ISO` (the disc image's path, for the setup),
`RAC1_MEMCARD_DIR` (the memory card's directory), `RAC1_GRAPHICS_FILE`, `RAC1_ADVANCED_FILE`,
`RAC1_CONTROLS_FILE`, `RAC1_RENDERER`, `RAC1_WINDOW`, `RAC1_FULLSCREEN`, `RAC1_VSYNC`, `RAC1_FPS_CAP`,
`RAC1_NO_AUDIO`, `RAC1_AUDIO_DEVICE`, `RAC1_AUDIO_PERIOD_MS`, `RAC1_AUDIO_PERIODS`,
`RAC1_MOVIE_VOLUME`, `RAC1_PRIORITY` and `RAC1_IOP_MIRROR`. `RAC1_HEADLESS` runs without a visible
window for automated tests, and also allows the testing variables.
