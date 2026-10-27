# Changelog

What changed in each release. Releases are source only: you build the game with `./setup.sh`
(`setup.cmd` on Windows) from your own disc. Issue numbers refer to this repository's issues.

## Unreleased

### Graphics
- Lights: Anya-Thoughtmane's second lighting pass (#102) on Veldin to Rilgar: emissive surfaces, lamps by class and by hand, merged with ours.
- The levels' water, lava and sand seas (VU1 program 7's surfaces: Rilgar's harbour, Gaspar's lava sea, Aridia's sand sea, Hoven,
  Umbris, Novalis, Kalebo, Drek's fleet..., the levels' baked shadow meshes and the nanotech orbs) are drawn by the modern renderer
  from the game's own geometry, in the world's projection: no side cut or wedge in a widened view, fogged as the GS fogs them, and
  with Lighting not Original they take the sun's shadows, the point lights, and emissive / material lines (`surface`); the GS
  replica now draws no 3D in play. `RAC1_MRP_SEAS=0` for the old path. OpenGL and Vulkan.
- Ambient particles (Options > Graphics > Ambient Particles: Off by default, Low, High): our own dust, ash, embers, fireflies,
  light dust in the sunlight, rain with splashes and snow, per level from its light file (`particles` lines, editable in the
  debug menu's Sun tab); Batalia's rain and Hoven's snow drawn in place of the game's own while it is on. OpenGL and Vulkan.
  See [Ambient particles](docs/particles.md).
- Ambient particles, after the first playtest: embers and ash can rise from the level's lava itself (`source lava`, `rise`: points
  on the open lava faces of the level's collision near the camera, drifting slowly in the air's currents), as Umbris's (the
  Snagglebeast's cave) and Gaspar's now do, none away from the lava or indoors; the wind is steady unless a line asks for gusts
  (`gust`); Hoven's snow is a calm, light snowfall; Batalia has rain only.
- Ambient particles: no rain, snow or ash under a roof - Batalia's covered hall, Hoven's rooms, any tunnel or ledge -, and no
  splashes or materials' rain rings there: the level's collision seen from above (the height of its topmost face, read once per level
  on a thread of its own) keeps them out, softly at the openings; no box to place by hand. See
  [Ambient particles](docs/particles.md#under-a-roof).

### Lighting (the optional modern lighting, still experimental)
- The levels' own suns from Anya-Thoughtmane's lighting pass (#90, #101), set with the debug menu's Sun tab: Veldin, Novalis,
  Kerwan, Eudora, Umbris, Gaspar, Orxon, Pokitaru, Hoven, Gemlik Base, Quartu, Kalebo III, Drek's fleet and the Veldin finale
  get a sun of their own or a new one (to playtest); Aridia keeps its sun from the skybox's planet. See
  [The level's own sun](docs/lighting.md#the-levels-own-sun).
- Sun Rays off per level: a level's light file can turn them off there (`sun_rays off`; the debug menu's Sun tab has a checkbox
  for it, saved with the file). Off on Blarg's station, Rilgar, Batalia and Oltanis, from Anya-Thoughtmane's lighting
  pass (#101). See [Sun rays](docs/lighting.md#sun-rays).
- Emissive surfaces: a level's light file can mark a texture (`emissive <tfrag|tie|shrub|moby> <class> tex <n>
  [strength] [threshold]`) that keeps the original lighting's brightness where the sun's shadows and the relighting would darken it
  (lamps, screens, panels painted in textures), marked in the debug menu's Lights tab; Veldin's domed building lamps and Novalis's
  landing pad lamps, neon tubes and bulbs are marked (#102).
- Surface materials, a new Options > Graphics row Surface Detail (Off, the default; Low; High). A level's light file
  tags a texture `material <tfrag|tie|shrub|moby> <class> tex <n> <type> [a] [b] [c]` - metal, wet, puddle, lava, water, mud,
  quicksand - lit and animated over the PS2 texture (specular with Fresnel, wet ground with puddles and rain ripples, a slow flow);
  High adds reflections from the last frame's picture and the lava's heat shimmer. `rain on` and `sky r g b` per level. Tagged in
  the debug menu's Lights tab. Batalia's platform, railings and buildings (metal) and its paved ground (puddles, rain) and Umbris's
  swampy ground (mud). The puddles, ripples, rain rings and the lava's pulse are placed by an exact integer noise, the same on
  every GPU and driver. See [Materials](docs/materials.md).
- More lamps light the world on Veldin and Novalis (nanotech crates, fires, plasma beams, landing pad, eyes, bombs), and
  lights per object class follow the objects the game spawns later (thanks Anya-Thoughtmane, #102).
- Novalis's Buzzers light with their green eyes, and the Blarg ship (Novalis's drop ship, Gemlik's ships of the
  skill point) lights from its orange and blue lamps, which glow, instead of from inside its hull (#102).
- Light editor (debug menu, #101): click on the picture to pick a surface or an object part (the crosshair stays), a light added
  at the clicked surface (just off it along its normal), lines for a moby class on every level (`data/lights/classes.txt`, a "For
  every level" box; a level's own line wins), a moby light on a joint of its pose (`emit moby ... joint <n>`, for the Buzzers' flight),
  and a Textures tab: the level's tfrag, tie, shrub and moby textures as thumbnails with their classes, a click to mark one emissive
  or give it a material. See [Lighting](docs/lighting.md#the-light-editor).

### Performance
- The game's update runs on a thread of its own while the main thread draws the pipelined frame (a window's default): the host
  frame costs the update plus the frame's preparation, its drawing beside them; the game's state the same, word for word. With
  the two changes below, a game frame costs about a fifth less (Rilgar 12.2 to 9.7 ms, Kerwan 13.8 to 12.0 ms, headless at a
  2528 x 686 window with LOD 3 and the modern lighting), the main thread's own work about half.
  The debug menu's Display tab (Game update on its own thread), `RAC1_GAME_THREAD`. See [Performance](docs/performance.md).
- The MRP world's triangles and batches are built on the scene's worker, no longer on the main thread when the frame is drawn
  (`RAC1_MRPW_BUILD_THREAD=0` for the old way).
- The moby joints' pose (0x250fe0) keeps the VU0's registers in SIMD registers through its hot loops, two matrix chains at once
  on a CPU with AVX2: about a quarter less time for the joints and skinning, the same results bit for bit (`RAC1_VU0_SIMD=0` for
  the old way; oracle batch 470).
- A pipelined frame's modern lighting and ambient particles read the game's memory as it was when the frame was prepared (a
  copy), no longer the next update's.
- Vulkan: the ambient occlusion reads the 3D's depth where it is instead of copying it every frame: GTAO at 5053 x 1371 about a
  quarter less (Rilgar 4.1 to 3.1 ms of GPU, Kerwan 5.0 to 3.9; GTAO 1/2 2.8 to 1.7), the same picture
  (`RAC1_AO_DEPTH_COPY=1` for the old way). See [Lighting](docs/lighting.md#ambient-occlusion).
- The sun's shadows: the mobys over a cascade's static cache are put back and drawn in up to four rectangles instead of one
  around them all, and the two far cascades' mobys are drawn every other frame in turn: the cascades' GPU time 2.1 to 1.3 ms on Rilgar, 2.4 to 0.6 on Kerwan, the same shadows (a far cascade's moby
  shadow a frame late every other frame)
  (`RAC1_SHADOW_CLUSTERS=1`, `RAC1_SHADOW_FAR_EVERY=0` for the old way). OpenGL and Vulkan.
- The profiler with Vulkan: the sun cascades' stage is closed where OpenGL closes it (left open, the GPU's work after the world's
  draw without a stage of its own counted as the cascades'), and the point lights' cube shadows have their own stage.

### Debug tools (`--debug-tools`)
- Hoverboard anywhere (Player tab): on Rilgar and Kalebo III, gets Ratchet on the hoverboard race's board where he stands, as the board
  itself does when he walks up to it, and off again when unticked. Testing: `RAC1_HOVERBOARD_AT`.
- Hoverboard anywhere on every other level: a free board (Kalebo III's board drawn on Ratchet's feet, his own board animations
  read from Kalebo III's data, our own riding with the game's collision): the stick steers and speeds up, cross jumps; in the water,
  below the level, in a pit or after a long fall he is back on foot on the last solid ground. No race rules. OpenGL and Vulkan.

### Fixes
- Modern lighting: Ratchet and the other mobys are lit by the lights placed by hand, the lights per class and the automatic
  emitters, per pixel as the world (they had only the game's own lights; #110). OpenGL and Vulkan.
- Surface materials: a metal's roughness works as it reads - low is polished, a sharp highlight and a clear mirror of the sky (and
  on High of the picture), high is brushed - instead of looking duller when polished (#101).
- When the computer falls behind (or with V-Sync on a 75 Hz display) and a frame runs two or more of the game's
  frames, the frame after no longer reuses the undrawn frame's drawing data without what it needs: the game no
  longer freezes at Aridia's arrival, and no stray graphics programs run on Kerwan, Rilgar and other levels (#104).
- The debug menu's light editor no longer changes the numbers of a level's light file when it saves it: each Save took
  one unit off the last digit of the `emit` lines' offsets, colours and aim (the game rounds toward zero, as the PS2
  does, and the file was read and written so), and the file's comments and lines now stay where they were (#101).
- Widescreen: Rilgar's water near the camera no longer loses a wedge (a dark band across the water, the sea bed showing
  through) or shows a flat, stretched patch: the large water triangles that cross the whole 4:3 frame were stretched as if
  they were full-screen 2D, their corners past the frame's sides moved to the window's edges.
- Vulkan: the effects (glows, trails, smoke, the levels' seas) no longer make a new graphics pipeline for every batch of every
  frame, never freed: the game's memory no longer grows the longer it runs (Kerwan, 2400 frames: 403 to 370 MB), and the
  effects' draw costs a quarter of what it did.
- Vulkan: a few frame-timing fixes: objects freed between two frames (a level's load) wait for the frame that may still use
  them, the shadow maps' views are no longer freed while a frame draws into them (a Shadow Quality change), each swapchain
  image has its own "rendering finished" semaphore, the per-frame buffers are aligned as the GPU asks (Intel and other GPUs
  that need more than 16 bytes), and the debug menu's overlay no longer recreates the render scale's images every frame.
- Windows: with the game's update on its own thread (the default since the last change), the game's coroutine is no longer
  resumed from a thread that is not a fiber (undefined in Windows' fiber API, most likely a crash; found by reading the
  code, not yet tried on Windows).
- The settings files (graphics, Advanced, controls, the console's language), the memory card's saves and the setup's copy of
  the boot executable are written to a temporary file and renamed over the old one: a crash or a full disk while writing
  no longer leaves a file cut short (a save lost, or the settings back to their defaults).
- Options > Sound's 5.1 / 7.1: the reopened audio device's thread gets real-time priority again, as at start.
- The game no longer reads its Advanced settings file every frame while the title's logos and intro play.
- `RAC1_PROGRESS` is honoured in a normal (windowed) run too, as the launcher's page says; it was dropped without
  `--debug-tools`.

### Code clean-up
- Removed code nothing used any more: the first GS renderer (`GsRenderer`, `gs_state`: its `GsPrimitive` also clashed with the
  VU interpreter's), the old VIF unpacker, the first VU0 helpers (`runtime/kernel/vu0.cpp`), the level loader's file-path
  variants, unused helpers in the movie, sound, memory and format code, the temporary `RAC1_VU7_WHERE` trace.
- Bounds checks that could overflow (the level files', the disc's directory records, `ByteReader`), the job pool safe
  if two threads ask it at once, the VU profiler's clock on ARM64, compiler warnings.
- Docs checked against the code: env-vars.md's defaults, the update settings' place (Main menu > Updates), the setup's PAL
  disc, the launcher's buttons, and other out-of-date statements.

### Launcher and setup
- The game writes `progress.json` in its data folder for the [RAC Native launcher](https://github.com/RAC-Native/rac-native-launcher):
  the planet played, the planets reached, the share explored and the time played (thanks @mat-f96, #107).
  See [The launcher](docs/launcher.md#progressjson).
- The Steam Deck (SteamOS), Bazzite and other immutable Linux systems: the setup sees that the build's packages cannot be
  installed there and gives the commands that build the game in a distrobox (or toolbox) container instead (exit status
  20; `--check-system` checks the system only), and the launcher offers to set up that container, builds the game in it
  and starts it from it, with a script to add to Steam for the Gaming Mode (#108). See
  [Setup](docs/setup.md#steam-deck-bazzite-and-other-immutable-systems).
- The setup no longer trusts its record of a step whose files were deleted since (`assets/generated`, or part of it: the
  moby or world assets, the generated code, the overlay maps, the boot executable): it says which is missing and makes it
  again, instead of leaving the game without the modern renderer's assets until `--force`. The game says so at start too
  when those assets are missing, and points to `./setup.sh`.
- `./setup.sh --update` on the development channel no longer resets a local `main` that has commits of its own (after a
  stable update had checked out a release); `--update --native-arch` is kept for the rebuild; Ninja builds show their
  progress on Linux too.
- The launcher: Cancel right after a task starts stops it on Windows too; restoring a saves backup only writes inside the
  memory card's folder; the status of a running task is no longer replaced by a late refresh.
- The update check on the development channel offers main's latest commit even when it is more than 250 commits ahead.

## 0.5.0

The first release from the [RAC-Native](https://github.com/RAC-Native) organization on GitHub: the
project moved to <https://github.com/RAC-Native/rac1-native-game> (the old address redirects), and
the updater, the update check and the launcher use the new address. Windows is no longer
experimental: players have played through the game on it, and the new launcher, attached to the
release, installs and updates the game with a double-click on Windows and Linux. Also new: an
experimental Vulkan renderer (opt-in), a widescreen HUD, the modern lighting reworked (the level's
own sun, sun rays, six ambient occlusion modes), 5.1 and 7.1 sound, less CPU time for the game's
own update, and the fixes from the 0.4.0 reports. Playtested on Linux; the Vulkan renderer is
experimental.

### Graphics
- **Vulkan (experimental, opt-in):** a new Graphics row, Renderer (OpenGL, Vulkan), draws the game
  with Vulkan 1.3 instead of OpenGL 4.6, from the next start; `RAC1_RENDERER=vulkan` for one run.
  Everything the modern renderer draws is there (the world, the mobys, the effects, the modern
  lighting with its shadows, occlusion and sun rays, the HUD and menus), the debug menu and its
  Profiler too. OpenGL stays the default, and the game falls back to it when Vulkan cannot start.
  Its shaders are compiled when the game is built, with `glslc` (an optional package the setup now
  lists). See [Vulkan](docs/vulkan.md).
- **Vulkan:** the pause menu opens at once, as with OpenGL (the game stopped for about a fifth of a
  second first: the paused picture was read from slow memory about 50 times).
- **The full frame, always:** the Overscan row is removed. The thin slivers the game itself leaves
  at the frame's edge as a HUD element slides away (the Suck Cannon's gauge, the air meter), which a
  TV hid, are not drawn. Older settings and saves with the row load fine.
- **Widescreen:** water, lava and the sand sea are cut at the sides of the widened view in fewer
  places (Gaspar's lava): the game's own on-screen test now sees the wider view. A few surfaces are
  still cut, listed in [Rendering](docs/rendering.md#widescreen-and-render-scale).
- **Widescreen:** the Visibomb's camera view is green with scan lines across the whole screen, and
  its out-of-range static covers the whole screen too (both stopped at the 4:3 middle).
- **Widescreen:** Aridia's sand sea near the camera is no longer dropped at the sides of a very wide
  view (32:9).
- **Widescreen HUD:** a new Graphics row, HUD (4:3 Original, 16:9, 21:9, 32:9, Fit Screen), spreads
  the HUD over a wider box when the 3D is widened: the ammo, the quick select and the other elements
  the game puts at the left move left, the bolts, the air meter and the others at the right move
  right; health and the centred messages stay. On a 32:9 screen, 16:9 or 21:9 keeps it within a
  glance of the middle. 4:3 Original, the default, is the original's layout.
- The title menu's frames and the Vendor's monitors line up at any Field of View and in widescreen
  (#68).
- Text scaled up by the modern renderer no longer shows thin lines from the neighbouring letters
  (#76).
- Faster: the Gadgetron Vendor's screens and some of the GPU work; no more hitch when the pause
  menu opens.
- **V-Sync and the frame-rate cap:** with the cap on Unlimited the pictures now come on the game's
  60 a second, as with the caps of 60 and above (they came at the end of each frame's work, unevenly
  on a 240 Hz display). The debug menu's stats overlay counts the pictures presented (it counted the
  host loop's turns, about 120 for 60 pictures), and the log says what V-Sync was set to and what the
  swaps do. See [Settings](docs/settings.md#graphics).

### Performance
- **Less CPU time for the game's own update** (the EE code, on one thread as on the PS2), the game's
  state the same bit for bit: the VU0 macro ops keep their flags in registers through the mobys'
  joints and the moby draw loop, and the renderer's moby scene (the chains' decoding, the draw lists'
  transforms) is built on several cores (`src/jobs`, deterministic). With the modern lighting at
  5053x1371 and the widened view, the game's update went from 14.2 to 12.1 ms a frame on Kerwan, 12.7
  to 11.1 on Rilgar, 9.7 to 8.5 on Hoven, the moby draw lists from 2.3 to 2.0 ms (Kerwan).
- **`./setup.sh --native-arch on`** compiles the game for your own CPU (AVX2, AVX-512 where it has
  them): about 8 % less again (Kerwan 11.2 ms, Rilgar 10.3, Hoven 7.9), the same game state. Off by
  default: such a build runs on that CPU only.
- The Profiler tab splits the game's code into its parts: the game logic (mobys, hero, particles,
  camera), the rendering preparation the game's code does (world passes, moby pass, joints and
  skinning, effects) and the host's re-runs for the LOD slider and the widened view.

### Lighting (the optional modern lighting, still experimental)
- **Shadows without tiers:** a point is lit or in shadow, whatever blocks the sun, near or far;
  overlapping shadows no longer add up or cancel.
- **The level's own sun:** a level's file can turn the sun where the game's direction leaves whole
  areas in the shade (Novalis, Kerwan, Eudora, Oltanis, Quartu; on Aridia it comes from the big
  planet in the sky, on Gemlik Base from the sun in its skybox); the debug menu's Sun tab edits it
  live. Kerwan's sun now comes low through the
  first tunnel's mouth, the city backlit.
- **The world lit by our sun:** the baked key light is taken off the world and the mobys and put
  back from the sun, times its shadow, so lit and shaded ground follow the same sun.
- The original's drop shadows are no longer drawn with the lighting on.
- **Interiors** come from the level files (Orxon's base's first rooms, a closed room on Blarg): the
  sun turns off there with a short fade. The old check of how much of the view sees the sun is off
  by default.
- **Shadow Strength** (Options > Graphics, 50 to 100 %, default 70 %): how much of the sun a shadow
  hides.
- **Lights per object class:** every object of a class can be a light, from the level's file:
  Novalis's neon tubes and hanging bulbs, Kerwan's street lamps, Oltanis's plaza lamps. A lamp can
  be aimed down (a pool of light on the floor under it): Novalis's neons now light the floor and the
  walls below them, which a flat plate under the yellow press had put in its shadow, and were too weak.
- **Sun Rays** (Options > Graphics: Off, Low, Medium, High): shafts of light from the sun where the
  sky shows through the scene. Medium is the earlier Low, High the earlier High at a finer
  resolution, Low a cheaper Medium (settings and saves with Low read as Medium, High as High).
  Facing the sun under an open sky they no longer wash the whole picture out: the sky's broad glow
  stays near the sun and the shafts through openings keep their light; what is bright gains less.
  The steps differ in strength too (Low the earlier Low's, High the earlier High's, Medium between).
  No Sun Rays in the ship's flights (leaving a planet, and between planets on the loading screen),
  where the sun was still the planet left behind (#66).
- **Ambient Occlusion** has six values (Off, SSAO, SSAO+, HBAO, GTAO, GTAO at half resolution), and
  each mode is now visible and different (#75).
- **AO Intensity** (Options > Graphics, 25 to 125 %, default 75 %): how dark the ambient occlusion
  is. The characters and objects get less of it than the world (Big Al's face and arms in his shop
  were too dark); the ground around them keeps its contact darkening.

With Lighting on Original (the default) the picture is unchanged.

### Audio
- The game's sound is no longer 6 dB quieter than the movies: the master volume is read as the
  original's sound driver reads it (#21).
- The sound driver closer to the PS2's: an SPU2-style reverb, the original's autopan, the voices
  summed in 32 bits, fixes to the ADPCM decoding, and volume changes ramped so they do not click.
- The reverb routed as on the PS2 (#86).
- The reverb's pipe preset no longer fills the slide down Novalis's sewer pipe with static; two of
  the studio presets corrected.
- **5.1 and 7.1 output:** Options > Sound's last row steps Mono, Stereo, 5.1 and 7.1. The world's
  sounds come from their direction, the dialogue from the centre. See
  [Settings](docs/settings.md#sound-51-and-71).

### Launcher and setup
- **The launcher**: one file to download and open, attached to each release, `rac1-launcher.exe`
  for Windows and `rac1-launcher-linux-x64` for Linux. It downloads the game, sets it up from your
  disc image, updates it (and itself), starts it, and backs up, restores and deletes saves. It
  follows the update channel (stable releases, or development: `main`), the same setting as the
  game's own. On Linux it offers to install the packages the build needs (your desktop asks for your
  password). When the game closes right after starting, it shows why. See
  [The launcher](docs/launcher.md).
- Windows: the game no longer drops `RAC1_PYTHON`, which the setup sets for the game's setup mode.
- An update from the source zip leaves the files the new version does not change as they are: the
  build then recompiles only what changed, instead of the whole game at every update (an update
  with nothing new took 6 minutes on a 20-thread PC, now a few seconds).
- An update stopped before it changes anything (no network, local changes) no longer leaves its
  backups behind (a copy of the program, about 350 MB, and of the generated code).
- Windows: the setup no longer fails downloading GLFW with "error adding trust anchors" when the game's
  folder has a space in its path (#79, #69, #63): MSYS2's certificate files, left empty by its
  CLANG64 tool there, are made again by `tools\windows\toolchain.ps1` (also on a setup made before).
- A code generation job whose Python crashes ("Fatal Python error", #72) is run once more on its own.
- A clearer message when the program disappears after the build (an antivirus removing it, #69).
- Windows: the setup checks the C compiler too (clang, beside clang++): when it is missing, `toolchain.ps1`
  installs the compiler again and the setup says so, instead of CMake failing (#88).

### Fixes
- Pre-rendered movies no longer add to the time played, as on the PS2 (#24).
- The Controls page: a mouse movement no longer rebinds the D-pad, only the sticks, as the page's
  help says (#61).
- Gamepads: every gamepad the game recognises is read and their inputs merged, not only the first one,
  so a virtual or silent device can no longer hide the real pad (#98). The Controls page names the
  pad last used, and the log lists every joystick at start.
- With the Mirrored Levels cheat, strafing (Strafing, RAC 3) no longer draws the level unmirrored,
  which reversed the controls, and the Modern aiming camera stays over the right shoulder (#95).

### Debug tools (`--debug-tools`)
- **Profiler** tab and overlay: the frame time (a graph, min / avg / max / p99 / 1 % lows), every
  stage of a frame on the CPU and on the GPU with the bottleneck, the threads' CPU, the memory and
  the GPU's, exports to CSV and to a Chrome trace; `RAC1_PROFILER_DUMP` and the others for headless
  runs. Off until opened: nothing is measured.
- **Unlocks** tab: gives and takes back the items, the goodies, the weapons and their ammo, the gold
  bolts and the gold weapons, the skill points, the cheats (unlocked, and switched on one by one), the
  Challenge Mode (which shows the pause menu's Goodies) and the planets on the ship's map, one by one
  or all at once with "Unlock everything", writing what the game's own code writes.

### Building
- The generated code is compiled without debug info (the same machine code): a faster setup, less
  memory per compile job and a program of about 140 MB instead of 335 MB. `RAC1_GEN_DEBUG_INFO=ON`
  keeps it for debugging.
- The CI also builds on Linux ARM64. It does not block yet: the ARM64 port is in progress (#78).
- The game frame's coroutine switches stacks with a hand-written switch on x86-64 and ARM64 (from the R&C2
  port), not `ucontext`, which macOS deprecates and Android lacks; `RAC1_COROUTINE=ucontext` compares.

## 0.4.0

Bug fixes from the 0.3.0 reports, and almost all of the game's code is now ported: of the code the
game runs, one function is left (see the [port inventory](docs/port-inventory.md)). The whole game
has been played through to the end on it, and the start of a New Game+ checked; tested on Linux only
so far.

### Crashes
- **Quartu:** the crash under water when the air meter runs low (very likely #48's at the Box
  Breaker path): the game asks for a sound that does not exist, which the PS2 ignores.
- **Kalebo III:** the crash during the landing scene in widescreen with a raised level of detail.

### Gameplay
- **Modern aiming** aims up and down with every weapon that fires in a direction (Devastator,
  R.Y.N.O., Visibomb, Pyrocitor, Tesla Claw, Morph-o-Ray, Suck Cannon), and the gloves' throws land
  nearer or farther with the aim (#48).
- **RAC 3 movement:** jumping during a standing wrench throw (#50).
- The camera no longer locks for a second after a strafe (#56, thanks to MJorink).
- Giant Clank's triangle attack no longer leaves streaks behind (#51); the laser beams on Drek's
  fleet move instead of hanging frozen (#53). Every particle type of the game is now ported.
- Kerwan's hanging cables are caught as in the original (Kerwan's own rail search).
- The Metal Detector's display now shows on every planet, not only Batalia and Drek's fleet.
- **Quit Game** now goes back to the title screen and its main menu, as the original, instead of
  closing the game.
- A cutscene started while strafing no longer keeps Ratchet in the strafe pose (#40).
- The pause menu's **Goodies** pages after finishing the game: Cheats, In-Level Movies, the
  Sketchbook, the Epilogue and the Skill Points text.

### Graphics
- **Widescreen (16:9, 32:9):** the water, lava and sand sea are no longer cut at the 4:3 sides (#6);
  thin far objects (Gaspar's antennas and spires, far plants) are no longer missing at a raised
  level of detail or in widescreen.
- Force fields, Quartu's Hologuise gates and Orxon's O2 barriers are see-through and tinted instead
  of black (#52).
- The Vendor's screens on Aridia, Eudora, Umbris and Oltanis (the Tesla Claw page) (#47, #48).
- Hoven: the ice cube around Ratchet is whole again (#48).
- A high Field of View no longer clips the sky (a black band at the top, Pokitaru's sky after
  surfacing), and the ship menu's planets are no longer drawn as squares.
- The starry sky in the loading screen's flight after Load Game from the title, with the lighting on.
- **Modern lighting** on Batalia to the Veldin finale: rooms and corridors no longer go dark under
  the roof's shadow; false lamps on Gaspar and Orxon removed.
- A **Monitor** row on the Graphics page, to choose the screen for fullscreen (#44, thanks to CDCast).

### Windows and the updater
- **Update now** without git: a clone that git cannot update is updated from the source zip,
  keeping your own files (#47). (Players on 0.3.0 without git need to update by hand once.)
- Windows: a click in the console no longer freezes the game (QuickEdit off), the frame timing is
  more precise, and alt-tab from fullscreen / a minimized window no longer breaks the picture (#41).
- Window sizes down to 640x480 (#46).

## 0.3.0

**Every planet of the game is ported**, up to the final fight with Drek, the credits and the end
menu. The planets after Orxon are new and **not fully tested yet**: expect bugs and glitches; some
polishing work is still to do. Tested on Linux only so far.

### New planets
- **Pokitaru** (finished), **Hoven**, **Gemlik Base** (Qwark's space fight), **Oltanis**, **Quartu**
  (Giant Clank), **Kalebo III** (Gadgetron's headquarters, the hoverboard race), **Drek's fleet** and
  **the Veldin finale**, each with its own build of the hero's code, its classes, effects, cameras
  and sounds, ported from the game's code and proved against the original.
- **The end of the game:** the credits, the last movie and the end menu (a new game keeping your
  weapons and bolts, or back to before the fight with Drek).
- The PDA now opens the Vendor on every level.

### Performance
- Faster vector math (SSE2), a faster dispatch of the game's callbacks and lighter renderer passes;
  the experimental lighting is faster too. The game's own state is unchanged.
- The levels are read straight from the disc image: the setup no longer copies them (335 MB less).

### The updater
- **Update now** from the update notice: the game closes, updates its sources (git, or the release's
  source zip), runs the setup again and restarts. Source only, as always; local changes are never
  overwritten (#13).
- A dedicated **Updates** page on the title's main menu: the check on / off, the channel (Stable /
  Development) and **Check now**.

### Also new
- **Strafing movement** (Advanced): R&C 1's movement with the strafe and the strafe flips (#34).

### Fixes
- Batalia's cutscenes out of sync: the pause menu now pauses the dialogue and music as on the PS2 (#23).
- Particles that froze in place on several planets (smoke, debris, effects), as the helicopter
  bullets did before.
- Kalebo III's race HUD showed its text in the wrong colour.
- Experimental lighting: the sun's shadow no longer flips when entering buildings (#3).
- The black bars of the 4:3 view were missing (#39).

## 0.2.1

Windows fixes, confirmed by a player on a laptop with an NVIDIA GPU (#35):
- On laptops with two GPUs, the game now starts on the dedicated one (NVIDIA or AMD) instead of the
  integrated one. A choice made in Windows' settings or in the driver's panel still wins.
- The flickering (the title's scene in a wrong view, the menus' frames disappearing) is gone: a
  frame that came sooner than the game's next one was drawn again with the wide-screen projection
  applied twice.
- `play.cmd` checks that the setup is up to date and, after a new version or an unfinished setup,
  offers to run it again. The setup no longer redoes everything when run from another Windows shell.

## 0.2.0

### New
- **Batalia, Gaspar and Orxon** are ported and playable: Batalia's rain, its turret and the Trespasser
  puzzle; Gaspar's lava and the Pilot's Helmet (it could not be picked up before, on any level);
  Orxon's toxic goo, Clank's section and Ratchet's part with the O2 mask.
- Weapons and gadgets shared by every level: the Drone Device's drones, the Morph-o-Ray's chickens
  and its HUD element.
- **The European PAL disc** (`SCES-50916`) works on its own, without the North American disc (#12). It
  runs in the PAL game's own 60 Hz mode. Ten functions that the PAL release changed are still
  temporary versions, not yet proved against the original (see the Setup page).
- **Windows support, experimental** (#11): the same sources build on Windows; `setup.cmd` downloads
  everything it needs, and `play.cmd` starts the game.
- **An update check** (#14): at start the game checks GitHub for a new release and says so at the main
  menu. It downloads nothing, and can be turned off in Options > Advanced. A work in progress.
- **The oracle is in this repository** (`tests/oracle`, [Oracle](docs/oracle.md)): the tests that prove
  each port against the original code.

### Fixed
- Controllers missing from the controller database (for example the GameSir G7 SE) were ignored: on
  Linux, Xbox-style controllers now get the Xbox layout automatically (#1).
- The language chosen in the game is kept for the next start, as a PS2 keeps its system language (#15).
- Below 60 fps (the 30 FPS cap, V-Sync at 75 Hz, a heavy scene) the game lost frames: it ran slow and
  the movies' sound fluttered (#4, #22; better on #8 and #24).
- Effects turned black or invisible after a while on Aridia on NVIDIA cards (glows, smoke, bolts, the
  sand), which could then crash the game (#19; to be confirmed on NVIDIA).
- The air meter left a line at the screen's edge after diving (#5).
- The Vendor showed a chicken icon and 0/0 ammo for the centre weapon (#27).
- Kerwan's helicopters' bullets froze in mid-air (#26).
- Advanced options changed at the main menu were lost if you quit without playing.
- On Windows, the setup failed with a disc image whose path contains `&` (#35).

## 0.1.0

Milestone 1: the game playable from Veldin to Umbris, on Linux, from the North American disc.
