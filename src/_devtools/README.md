# `src/_devtools/` — openly invented development tools

Per [ADR 0003](../../docs/decisions.md), this directory holds code that is **openly invented** to
support development:

- A dev menu inspired by R&C1's hidden Insomniac debug menu
- Spawn / kill / teleport helpers
- Render-state toggles (wireframe, no-cull, depth-test off)
- Force-state controls (skip cutscene, give gadget, set checkpoint)
- Visual-debug overlays (collision wireframes, bounding boxes, etc.)

## Policy

- **Openly invented**: there is no `// orig:` requirement here. This code is *not* a port; do not
  pretend it is.
- **Compiled only when** the `RAC1_DEVTOOLS` CMake option is ON (the default). A build with
  `RAC1_DEVTOOLS=OFF` leaves this whole tree out.
- **No header from here may be `#include`d by** `src/game/` or `runtime/`. Only `src/host/` and
  other files in `src/_devtools/` may include them.
- **Uses ImGui freely**: that is the point.
- May read and write EE RAM and call the hardware layer in `runtime/`. It should not call functions
  in `src/game/` that are not meant to be called from outside.

## The lighting's editors

- **Lights** (tab): the modern lighting's lights placed by hand and the automatic emitters' classes
  turned off, per level, saved in `data/lights/levelNN.txt` (see
  [Lighting: the light editor](../../docs/lighting.md#the-light-editor)). Its "Lights per object class" part picks the object
  under a crosshair (the depth there: a tie instance's box, the nearest moby) and makes every instance of its class emit
  (`emit` lines; see [Lighting: lights per object class](../../docs/lighting.md#lights-per-object-class)).
- **Sun** (tab): the level's own sun direction (azimuth and elevation, or the game's key light),
  applied at once while playing; Save writes the `sun` line into the same level file, keeping its
  comments and lights (see [Lighting: the sun editor](../../docs/lighting.md#the-sun-editor)).
- **Sun** (tab), interiors: the level's `interior` boxes (the camera inside: the sun off, the original's look) drawn over the
  picture, added around the camera, edited, deleted; `interior_set` light sets (the hero lit with one); saved with the level file
  (see [Lighting: interiors](../../docs/lighting.md#interiors)).

## The profiler

The **Profiler** tab (`dev_profiler.cpp`; the measuring is `src/profiler/host_profiler.{h,cpp}`, which the renderers and
the workers call) shows where a host frame's time goes. It records only while the tab is shown or its **Overlay** is
on: closed, every zone is one relaxed atomic load and no GL query is made; open, a run at Daino's 5053x1371 took 2-3 %
longer (the dumps' GL object counts included). `RAC1_PROFILER_DUMP`, `RAC1_PROFILER_CSV` and
`RAC1_PROFILER_TRACE` ([env-vars](../../docs/env-vars.md)) record without the menu, also headless (and
`RAC1_DEVMENU_SHOT=Profiler RAC1_PROFILER_OVERLAY=1` shows the tab and the overlay in a headless screenshot).

- **Frame time**: fps, the frame's period (min / avg / max / p99, 1 % lows) over the last N frames (the slider), a graph
  of the period (white), the main thread's own work (blue: the period minus its waits) and the GPU's busy time
  (orange), with the game's code (the EE update) against the host's own work.
- **Stages**, CPU and GPU side by side: a stacked bar each and a sortable table (the busiest stage on each side in
  yellow, the waits greyed). Each stage's time is its *self* time: a zone inside another (the GS 2D inside the
  effects) counts in its own stage only, so the stages add up to the frame. The CPU side is the main thread's wall
  time. The GPU side: one `glQueryCounter(GL_TIMESTAMP)` at each boundary of a GPU-timed zone, the time between two
  given to the innermost zone open there; read back up to 32 frames later and only when ready (never a stall: a
  frame whose results did not come is left out of the GPU's averages; in uncapped headless runs they came back 4 to 17
  frames later). No flush between them (a flush per boundary made a run 50 % slower), so a stage's GPU
  time also holds the GPU's idle wait for the CPU to submit its commands: exact when the GPU is the bottleneck (its
  queue never empty), too high when the CPU is. The kernel's own account of the GPU's time (Linux DRM
  `drm-engine-gfx`) is shown beside it as the check, and decides "GPU-bound" where it exists.
  The stages: events and input, the game frames (the game's code on the EE coroutine, the audio step), the prepare
  (the GS chain's capture and the scene's start, the wait for the scene's capture, the moby lists), the draw (the
  wait for the MRP scene worker, the sun cascades, the point cubes, the world, the mobys, the wait for the GS chain
  worker, the ambient occlusion, the sun rays, the render scale's resolve, the effects, the GS replica's 2D / HUD,
  the frame captures, the debug menu itself), the profiler's own readback and sampling (with Mesa's GL thread a
  query's result is a sync: the driver's back-pressure can show there), the frame cap's wait and the present (the
  swap: the v-sync, or the driver's queue full when the GPU is behind).
  The game's code has sub-stages of its own (zones inside the EE coroutine, around the game's passes; "game code" in
  the summary is their sum): the game logic (the gameplay frame's own work, the mobys' updates, the hero, the
  particles' update, the camera) and the rendering preparation the game's code does (the frame's drawing without a
  sub-stage, the world passes, the moby pass's draw loop, the mobys' joints and skinning on the VU0, the effects and
  draw lists, the world lighting and the passes' ends), and the host's re-runs of game passes for the LOD slider and
  the widened view (the moby pass's, and the others': world, effects, draw lists). A zone left open when the coroutine
  waits for a vsync inside a blocking loop is closed with the game code's own zone.
- **Bottleneck**: GPU-bound (the GPU busy for 85 % of the frame or more), bound by a worker (the main thread waits
  15 % of the frame or more for the MRP scene build or the GS chain), CPU-bound on the main thread (its own work 85 %
  of the frame or more), or held by the frame cap / v-sync.
- **Threads**: the GS chain worker's and the MRP scene worker's busy and CPU time per frame (their jobs timed), the job
  pool's workers' (`src/jobs`, summed over its workers; a track each in the Chrome trace), and
  every thread of the process with its CPU % (Linux: `/proc/self/task/*/schedstat`; Windows: the threads' times;
  the driver's threads included), and the process's CPU %.
- **Memory**: the process's resident memory and its peak (Linux `/proc/self/status`; Windows
  `GetProcessMemoryInfo`), the GPU's: the kernel's account of this process's VRAM and GTT and the device's totals
  (Linux DRM `/proc/self/fdinfo`, amdgpu's sysfs; the GPU's busy % from `drm-engine-gfx`), `GL_NVX_gpu_memory_info` /
  `GL_ATI_meminfo` where the driver has them, and **Count GL objects**: every texture, buffer and renderbuffer the
  context holds, sized from GL itself, per owner (the renderers tag their allocations with `prof::gl_track_n`; the
  rest is "(untracked)").
- **Export CSV** (the last N frames) and **Chrome trace** (the next 300 frames, for chrome://tracing or
  ui.perfetto.dev: the main thread's zones, the GPU's on the CPU's time line, the workers' jobs), written in the
  working directory as `rac1_profile_<date>_<time>.csv / .json`.

A new stage is a `prof::Stage` (its name, colour, whether it is a wait and whether it is timed on the GPU) and a
`RAC1_PROF_ZONE(kStage)` (or `prof::begin` / `prof::end`) around the code, on the main thread; a worker's job is a
`prof::Job`. The old `RAC1_FRAME_PROF` (coarser, printed) and the per-pass `RAC1_AO_PROF` / `RAC1_SHADOW_PROF` /
`RAC1_SUN_RAYS_TRACE` timers are left as they were.

## The Unlocks tab (`dev_unlocks.cpp`)

What the game keeps of what was found and bought, given and taken back through the game's own state: the same bytes and
writes as the game's code, so the pause menu, the Vendor, the cases and a save see them as the game's own (a save made
after giving loads back with them). Only in the debug menu (`--debug-tools`); testing: `RAC1_UNLOCKS` (docs/env-vars.md).

- **Items** (the pause menu's Gadgets and Items pages): the gadgets (Swingshot, Hydrodisplacer, Trespasser, Metal
  Detector, Hologuise, PDA), the packs, the helmets (Sonic Summoner, O2 Mask, Pilot's Helmet), the boots, the
  Hoverboard, the Map-o-matic, the Bolt Grabber, the Persuader: the items owned `0x13d4c0[id]`, given by the game's
  `item_give` (0x260860; owned, "seen" `0x13d4e8`, ammo, the level gadget's quick-select place). Taken back: owned and seen
  cleared, out of the quick select (`0x141ea0`, `0x15edd0`), a worn item taken off (the slot's request `0x141408[kind]` =
  0x26, as the pause menu's closing does), a held one replaced by the wrench. The goodies of the Items page are the bytes
  `0x13d388[i]`: Zoomerator 0, Raritanium 1, CodeBot 2, Premium Nanotech 4, Ultra Nanotech 5; the nanotech also sets the
  health maximum `0x15eda0` (5 / 8, back to 4 without them) and fills the health, as Orxon's vendor does.
- **Gold bolts**: `0x14bec0[level * 4 + k]`, one byte a bolt (the Gold Bolt moby 1134 sets its pvar +0's byte); 40 in all,
  per planet: Novalis 3, Aridia 4, Kerwan 3, Eudora 1, Rilgar 2, Nebula G34 2, Umbris 2, Batalia 2, Gaspar 2, Orxon 2,
  Pokitaru 1, Hoven 2, Oltanis Orbit 1, Oltanis 4, Quartu 2, Kalebo III 2, Veldin Orbit 2, Veldin (finale) 3 (counted on
  every level). The pause menu's Items page shows found (at most 40), used (4 per gold weapon) and left. The bolts of the
  planet you are on appear / disappear when it is loaded again.
- **Gold weapons**: `0x13e520[item]`, as Novalis's gold weapon cases (class 304) set it: Tesla Claw, Bomb Glove,
  Devastator, Pyrocitor, Mine Glove, Blaster, Morph-o-ray, Glove of Doom, Decoy Glove, Suck Cannon (the cases' table). The
  menus' icons and names, the quick select, the Vendor's models and the weapon in hand turn gold at once. "All" also
  gives the weapons not owned (the cases sell a gold weapon only for one owned); "None" leaves the weapons.
