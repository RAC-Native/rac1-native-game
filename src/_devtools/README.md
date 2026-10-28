# `src/_devtools/` — openly invented development tools

Per [ADR 0003](../../docs/decisions.md), this directory holds code that is **openly invented** to
support development:

- The debug menu (R3 / F1, `dev_menu.cpp`), inspired by R&C1's hidden Insomniac debug menu. Its tabs: Game,
  Player (teleport, the checkpoint, Kill Ratchet), Items and Unlocks (`dev_unlocks.cpp`), Mobys (freeze or
  retire them), Camera (free camera), Rendering (wireframe and shading views, the renderers' switches), Lights,
  Sun and Textures (the editors below), Display, Audio, Levels (level select), Engine and Profiler
- The free hoverboard on every level (`dev_hoverboard.cpp`, the Player tab's Hoverboard anywhere)

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
- **Sun** (tab), ambient particles: the level's particle lines (see [Ambient particles: the editor](../../docs/particles.md#the-editor)).
- **Lights** (tab), "Emissive surfaces and materials", and the **Textures** tab (the level's textures as thumbnails, a
  click puts one in hand): mark a texture emissive or give it a material (see
  [Lighting: emissive surfaces](../../docs/lighting.md#emissive-surfaces) and [Materials](../../docs/materials.md)).

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
  time. The GPU side (on Vulkan: the renderer's own timestamp queries, the same way): one `glQueryCounter(GL_TIMESTAMP)` at each boundary of a GPU-timed zone, the time between two
  given to the innermost zone open there; read back up to 32 frames later and only when ready (never a stall: a
  frame whose results did not come is left out of the GPU's averages; in uncapped headless runs they came back 4 to 17
  frames later). No flush between them (a flush per boundary made a run 50 % slower), so a stage's GPU
  time also holds the GPU's idle wait for the CPU to submit its commands: exact when the GPU is the bottleneck (its
  queue never empty), too high when the CPU is. The kernel's own account of the GPU's time (Linux DRM
  `drm-engine-gfx`) is shown beside it as the check, and decides "GPU-bound" where it exists.
  The stages: events and input, the game frames (the game's code on the EE coroutine, the audio step), the prepare
  (the frame's chain's capture and the scene's start, the wait for the scene's capture, the moby lists), the draw (the
  wait for the MRP scene worker, the sun cascades, the point cubes, the world, the mobys, the wait for the chain
  worker, the ambient occlusion, the sun rays, the render scale's resolve, the effects, the 2D / HUD,
  the frame captures, the debug menu itself), the profiler's own readback and sampling (with Mesa's GL thread a
  query's result is a sync: the driver's back-pressure can show there), the frame cap's wait and the present (the
  swap: the v-sync, or the driver's queue full when the GPU is behind).
  The game's code has sub-stages of its own (zones inside the EE coroutine, around the game's passes; "game code" in
  the summary is their sum): the game logic (the gameplay frame's own work, the mobys' updates, the hero, the
  particles' update, the camera) and the rendering preparation the game's code does (the frame's drawing without a
  sub-stage, the world passes, the moby pass's draw loop, the mobys' joints and skinning on the VU0, the effects and
  draw lists, the world lighting and the passes' ends), and the host's re-runs of game passes for the LOD slider and
  the widened view (the moby pass's, and the others': world, effects, draw lists). A zone left open when the coroutine
  waits for a vsync inside a blocking loop is closed with the game code's own zone. With the game thread (the game's update
  beside the pipelined frame's drawing, `runtime/ee/game_thread.h`) those zones are the game thread's, on a stack of their
  own, counted in the stages but not in the main thread's frame; the main thread's wait for it is "Wait: game thread", the
  game thread's for the drawing (the GS's local memory, a GL call asked of the main thread) "Game thread: wait for the draw".
- **Bottleneck**: GPU-bound (the GPU busy for 85 % of the frame or more), bound by a worker (the main thread waits
  15 % of the frame or more for the MRP scene build or the frame's chain), CPU-bound on the main thread (its own work 85 %
  of the frame or more), or held by the frame cap / v-sync.
- **Threads**: the chain worker's (the MRP's chain reader; the GS replica on the faithful path), the MRP scene worker's and the game thread's busy and CPU time per frame (their jobs timed), the job
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
after giving loads back with them: every byte here is in the save: its game block, the gold bolts in the level blocks). Only in the debug menu (`--debug-tools`); testing: `RAC1_UNLOCKS` (docs/env-vars.md).

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
- **Weapons and ammo** (the pause menu's Weapons page): Suck Cannon, Bomb Glove, Devastator, Visibomb Gun, Taunter,
  Blaster, Pyrocitor, Mine Glove, Walloper, Tesla Claw, Glove of Doom, Morph-o-Ray, R.Y.N.O., Drone Device, Decoy Glove,
  given and taken back as the items. **Max ammo**: the ammo `0x13d428[item]` of every item owned set to the maximum the
  HUD shows and the ammo pickups stop at (`0x1c40b0[item]` +0xe; the give amount +0x12 is smaller).
- **Skill points**: `0x13d408[0..29]`, one byte each set to 1 as the game's skill point tests and the pause menu's codes
  (0x2831c0) set them. The Goodies' Skill Points page ticks them; 15 open its Sketchbook, 30 its Epilogue (0x284ca0,
  when the pause menu opens).
- **Cheats**: unlocked `0x15edc0[n]` and switched on `0x15edb0[n]`, the bytes the cheats' moves set (0x206e00). The
  Goodies' Cheats page lists the unlocked ones and switches them. The tab lists the page's own cheats (its table
  0x1e91c8, read from the game; the names are the level's texts): unlock / lock (locking also switches off), on / off,
  each or all.
- **Challenge Mode**: the challenge mode count `0x15ee20`, set to 1 (when 0) as the challenge mode's new game (0x284950,
  after the game is finished) sets it to count + 1; off writes 0. With it the pause menu shows the Goodies (menu_open
  0x276190), the cheats' moves work, the bolts dropped are doubled, Novalis's gold weapon cases are the challenge mode's.
  (0x15eea0, the other flag menu_open tests, is never written by the game's code: left alone.)
- **Planets**: the game's `level_mark_visited` (0x2607d0: `0x13dd40[l]`, the visit order `0x13d510`) for levels 1..18,
  as the pause menu's codes do, so the ship's map lists every planet; their arrival messages are taken back. Not undone.
- **Unlock everything** / **Take everything back**: all of the above (the cheats unlocked, not switched on); taking back
  leaves the planets and the ammo.

## Hoverboard anywhere (the Player tab)

On Rilgar (5) and Kalebo III (16), the two levels that have it, the hoverboard of the races ridden away from the race; on every
other level a free board of our own (below).
The board is the class-439 moby (its update: Rilgar's L5:0x002f87a8, Kalebo's L16:0x002c7788). Standing (+0x20 = 0) it waits
for the hero within 5.5, then hands itself to him (`PLAYER+0x86c`), puts him in his board state 0x6b (sub-state 0x16; the
levels' own hero controllers ride it), keeps its spot (pvars +0x10) and goes to its riding state 1. Ticking the box does that
same mount with the level's nearest standing board, wherever Ratchet is; on Rilgar the board state's entry puts him on the
race's start line, so his position and heading (+0x98, +0x874) are put back. Unticking is the game's `hero_set_state(0, 1)`,
as the race's own end does; the board goes home by itself. Done between game frames (`dev_hoverboard_apply`); testing:
`RAC1_HOVERBOARD_AT` (docs/env-vars.md).

What the game's own board code still does, unchanged:
- The race's HUD (lap, time, place) shows while riding.
- Rilgar: the board is held to the race's track. Its crash checks (walls ahead, the heading against the track's, the height
  under the nearest track point: level 5's own) wipe it out (state 0x6d) away from the track and put Ratchet back on it; below
  61.4 (the water) it sinks (0x6e) and does the same. Tried: on the track anywhere along it (it starts where he stands), fine;
  the streets by the water, the vendor's dock, the taxi stand and the Hoverboard Girl's terrace: back on the track at once.
- Kalebo III: no track check; anywhere above 76 (the walkways, the courtyards, the Gadgetron buildings' level) is ridden; below
  it the board falls (0x6e) and Ratchet comes back on a point of the track.

### The free board (every other level: `dev_hoverboard.cpp`)

The board's riding is level 5's and 16's own code: their transliterated hero controllers (`rilgar_hero_tr_*`, `kalebo_hero_tr_*`)
read their state tables and globals at those levels' own data addresses, so they cannot run on another level's memory. What the
other levels have instead, and what the free board uses:
- **The state.** On every other level the hero controllers (Veldin's and the L1 family's ports, and the transliterated
  controllers of 6, 8, 11, 12, 14, 15, 17 and 18: their jump tables checked) send 0x6b where they send state 5: nothing per
  frame, no transitions of its own, the set_state's common tail only. The free board puts Ratchet in 0x6b with the game's
  `hero_set_state(0x6b, 1)`; the game then leaves his position alone (his animation, moby, camera, hits, items, water and the
  kill height still run as in any state), and the ride moves him between the game's frames (`dev_free_board_frame`, after
  each frame the hero was updated in; none in the pause menu, a dialog or the level's exit).
- **The riding** is ours: the stick turned through the camera as Ratchet's walk does (up: away from the camera; the board
  turns towards it at 4.5 rad/s standing, 2.2 at full speed), speeding up to 20 units/s, gravity 21. Cross jumps (also just
  off an edge): one jump a press (held, it does not jump again; a press is seen at any game frame and used at the ride's
  next one), always 6.25 units/s up, the arc integrated exactly, so every jump goes 0.93 up and lasts 0.6 s whatever the
  frame's length, as the race board's on Kalebo III (measured there: 0.93, 36 frames). The game's own segment query (`segment_collide`, 0x1efc70) finds the walls (two rays, at 0.35 and 1.1,
  slid along) and the floor (steps of 0.7 climbed, slopes followed down 0.9, more is a fall; a ramp's top throws him up), on a
  journal of EE memory with the contact registers kept (as `mrp_lod.cpp`'s shadow runs do), so the queries leave the game's
  memory as it was.
- **Off the level**: in the water (`PLAYER+0x12e4` / `+0x2f0`, from the game's surface check), below the level's kill height
  (`0x15f638`), off the map, airborne for 4 s, or put in the fall to death 0x77 by a level's pit: back on foot (state 0) on
  the last solid ground (where he rode a quarter second before, on flat ground). Never a race respawn.
- **The animations** are level data. The hero's sequences come from the level's core (its 256-entry hero sequence table, the
  level loader puts them in his class's slots); Kalebo III's core has the board's, played as its race board plays them
  (measured on Kalebo III): 82 the board ridden (standing too), 85 the jump (frames 0..4 a crouch the race skips, 5..13 in
  the air, 14..22 the landing), 58 a long fall. Veldin's core has every sequence, most levels have none of them (their slots
  are null). They are read once from Kalebo
  III's core on the disc, put at `host_layout::DEV_BOARD_SEQS` (0x02300000, above the game's 32 MB) with their frame table and
  trigger block made addresses as the game's load does (checked byte for byte against Kalebo III's own copies), and hooked into
  the class's null slots while riding; the slots are emptied again once Ratchet plays none of them. Played with the game's
  `hero_anim_set`; the game plays a sequence on at its own rate and loops it, so in the air the ride picks 85's frames
  along the arc (from 5 at the take-off to 14 at the landing, as the race does; driving off an edge from 11), lets the
  landing frames play once, then rides on 82; after 0.75 s falling, 58. (Before, 85 was taken for the riding and played on
  a loop, and 98, which the race does not jump with, for the jump.)
- **The board** is Kalebo III's class 439, drawn by the MRP only (`mrp_moby_renderer.cpp`, `append_guest`): its asset and the
  textures it names are read from `assets/generated/mrp/moby/level16/` and added to the level's (the level's assets loaded again
  once, at the first ride), and it is drawn after the frame's mobys on Ratchet as the frame drew him: his world matrix with the
  board's scale, 0.215 up, the joints as the game poses the standing board, his lights. No moby in the game's memory.
- **No sound.**
- **Getting off**: unticking (the game's `hero_set_state(0, 1)` where he is), a hit, an attack (square, R1...), a hazard
  floor, a cutscene, a pit, the water: the ride ends (the box unticks). A new level ends it too.

Tried headless on every level (`RAC1_HOVERBOARD_AT` once Ratchet stands after the arrival, ten seconds of stick forward with
turns and two jumps, then off; OpenGL, and Kerwan with Vulkan too): no crash, Ratchet on foot afterwards on every level.
How the scripted drive ended (a player steers; these say what each level does to a board ridden straight ahead):

| Level | The drive |
|---|---|
| Veldin, Blarg, Batalia, Oltanis, Quartu, Veldin finale | off an edge: back on foot on the ground before it |
| Novalis, Drek's fleet | into the water: back on foot |
| Kerwan | the whole drive on the board (walls slid along; the avenue north of the start, 214 194 47.5, rides well) |
| Aridia (0x68), Umbris (0x7b) | the level's hazard floor took him off |
| Eudora, Gaspar | an enemy's hit took him off |
| Pokitaru | a cutscene's trigger took him off |
| Hoven | killed on the way (on the board, as on foot) |
| Gemlik | off the base into its pit (0x77 above the kill height): back on foot |
| Orxon | not available while Clank is played (his class has no board sequences) |
| Rilgar, Kalebo III | the race's board, as before |
