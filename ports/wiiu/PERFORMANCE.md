# Performance on the Wii U: the machine and the plan

After the port is complete (PLAN.md; MRP_LITE.md: Veldin rendered and playable first).
The direction (Daino, 2026-10-27): port everything, then optimise for the console in detail - the rendering pipeline may be
changed for the Wii U where it does not fit (the MRP was made to be malleable; a GS replica is not the way).

## The machine

- **CPU, Espresso**: three PowerPC 750-family cores (the Gekko / Broadway line) at 1.24 GHz, big-endian. Per core 32 KB of
  instruction and 32 KB of data cache; the L2 differs: 2 MB on core 1, 512 KB on cores 0 and 2 (the game runs on core 1). No
  AltiVec: the SIMD is the paired singles (two 32-bit floats per instruction, from the GameCube). Also from the GameCube: the
  locked cache (half the data cache as a scratchpad, filled by its own small DMA) and the write-gather pipe (data streamed to
  the GPU).
- **GPU, Latte**: AMD R700-derived (the Radeon HD 4000 family), ~160 shader units at 550 MHz (~176 GFLOPS), unified shaders,
  a tessellator. GX2 has no storage buffers, no multi-draw-indirect; GX2_SHADER_MODE_COMPUTE_SHADER exists in the API, its use
  from homebrew to verify. 32 MB of eDRAM on the GPU's die (MEM1): fast, for colour / depth buffers and render targets.
- **Memory**: 2 GB of DDR3, 1 GB for a game (MEM2), ~12.8 GB/s.
- **Coprocessors**: none like the PS2's VUs or the Cell's SPUs. A DSP for the audio (AX's mixing runs there: the port's two AX
  voices already use it), an ARM (Starbuck) for security and I/O that runs the system (not open to a game), the hardware video
  encoder / decoder (the GamePad's stream).

What the PS2 did on its VUs moves: VU1's work to the GPU's shaders (the MRP's), VU0's to the CPU's paired singles or the GPU.

## The plan, in order

1. **Measure first.** The CPU with the sampling profiler (RAC1_WIIU_SAMPLE, tools/samples.py); the GPU with GX2 timestamps
   around each pass (the world, the mobys, the 2D) - to know whether a frame is bound by the CPU or the GPU. Not known yet: a
   230-frame run took 19 s with one moby draw per frame, 22-35 s with all of them (2026-10-27).
2. **All three cores.** Core 1: the game's logic alone. Cores 0 and 2: the MRP's preparation (the mobys' frame, the world's
   instances, the colours), the chain reader, the audio. Then pipelined: while the game updates frame N + 1, the other cores
   prepare and the GPU draws frame N (the MRP's preparation already runs on workers on the PC).
3. **No emulation left on the hot path.** The GS replica's walk for the 2D replaced by main's mrp_2d with a GX2 backend (and
   main's chain reader, mrp_chain); the mobys' VU0 skinning (moby_joints, fin_lanes, maddbc, mulbc: at the top of the
   profile) written natively with the paired singles, or moved to the GPU.
4. **The GPU: less work per draw.** GX2 display lists (GX2BeginDisplayList / GX2CallDisplayList) for the static world,
   recorded once per level and replayed each frame; the level's data resident on the GPU (templates, colours), only the
   instances per frame - today blocks are written per draw; instancing (GX2DrawIndexedEx with several instances) where a mesh
   repeats; colour and depth buffers in MEM1.
5. **The resolution, an option** (to try in real time): the TV's picture rendered at 1080p or 720p (then scaled), chosen in
   the options or run.cfg; 1080p kept if it holds, 720p otherwise, as many Wii U games did. The GamePad is 854 x 480.
6. **CPU micro-optimisation, last, measured on the console each time**: with 32 KB of instruction cache bigger code is often
   slower (forced inlining made it 5-10% slower); compact data for the data cache; the locked cache for the hottest streams.

## Later

- The relighting of ties and shrubs (off on the Wii U with RAC1_WORLD_PASS_SKIP=6: their colours stay the level's load ones):
  the game's on a worker core, or a relighting of our own.
- The GamePad as a second screen: a real-time map, the weapons' quick select.

## Progress

- Measured on the console (2026-10-28; Veldin, explore, everything on): 3.45 host frames/s, 17.1 game steps/s; with the world off
  (RAC1_WIIU_WORLD=0) 6.74 frames/s, 28.8 steps/s; with the GamePad not drawn (RAC1_WIIU_DRC=0) only 3.75. The world backend's
  log: ~4,000 GX2 draws a call (the TV's and the GamePad's: two calls a frame), 72,800 triangles, 1 batch, 4.49 ms CPU a call, 155
  instances; the tfrags 161 draws a call (their CPU route 4.3 ms a frame); the mobys 424 draws, 2.17 ms a call. The game thread's
  samples ~47% in Cafe OS (waiting, most likely on the GPU). So the world's ties / shrubs - one GX2DrawIndexedEx per (instance,
  template run) with a uniform block per draw, Veldin's one batch drawn twice (its AFAIL pass) - are the main cost.
- The ties' / shrubs' draws merged (mrp_world_merge.inc, mrp_world_gx2.cpp; RAC1_WIIU_WORLD_MERGE, default 3). A draw command's
  record (texture word, layer, draw_flags, FOGCOL) belongs to its template run, not to its instance: the level's vertices carry their
  run (an attribute; a vertex two runs share copied - none on Veldin), the indices are made absolute, and per frame the runs' records
  go into a table (R32UI, 4 texels a run) the vertex shader reads by the vertex's run - a command whose record differs from its run's
  in that frame is drawn alone, as before. Then, in the commands' own order: an instance's consecutive commands are one draw (their
  indices follow each other in the level's: no copying on Veldin), the batch's texture units all bound (uT1..uT15: the pixel shader
  samples the triangle's unit, as the PC's units), and consecutive instances drawing the same commands - the tie / shrub passes draw a
  packet for each instance of a group in turn - one instanced draw (gl_InstanceID, read from R0.w in CafeGLSL's disassembly; the
  instances' 256-byte slots in a row). Nothing is reordered (an instanced draw is its instances' draws in turn), so the picture is the
  same whatever the blending (Veldin's batch blends). One block slot per instance per frame (the instanced rows besides), the frame's
  data written once by the TV's call into a ring of their own and reused by the GamePad's.
  On the PC (a harness over the same CPU code: the PC's static path at RAC1_MRP_LOD=1, Veldin explore 600 frames - ~2,000 commands,
  ~240 instances a frame, the Wii U's single batch): commands per frame 690-2,320; merged by instance and unit (mode 1) -27%; by
  instance (mode 2) about -50%; with the instancing (mode 3) 78-310 draws, ~8x fewer (~225 instanced draws of ~880 instances a
  frame); every frame's merged draws expanded back into (instance, vertices, record, unit) in order equal to the commands' (explore 600
  frames, GOTO=0@60 forward 600: 0 differences); the plan's CPU time 0.01-0.04 ms a frame on the PC. Expected on the console: ~4,000
  GX2 draws a call down to ~500, the per-draw blocks (~2,000 a frame) down to ~1,000 slots written once a frame.
  Not verified on the console yet: the R32UI run table's byte order (taken as the colours': swapped), gl_InstanceID on GX2's
  instanced draws, the 16 pixel samplers. RAC1_WIIU_WORLD_MERGE=0 / 1 / 2 / 3 isolate each step; the log line every 120 calls gives
  the draw commands, GX2 draws, triangles, uniform blocks written, CPU ms a call and the plan's share.
  Next: the tfrags' ranges by unit could be one draw per batch the same way (their vertices hold their unit; the pixel shader's units);
  the frame's preparation on another core.

### The host frame by stage (2026-10-28, RAC1_WIIU_TIMING=1)

Veldin explore, everything on, the world's draws merged (one log line every 60 host frames; ms per host frame):

| configuration | host frame | game steps | world prepare | finish (the GPU's frame) | frames/s |
|---|---|---|---|---|---|
| everything on | 231 | 119 | 22 | 81 | 3.48 |
| mobys not drawn (RAC1_WIIU_MRP_STAGE=3) | 226 | 119 | 22 | 76 | 3.55 |
| world not drawn (RAC1_WIIU_WORLD_STAGE=3, RAC1_WIIU_TFRAG_STAGE=3) | 172 | 117 | 22 | 25 | 4.50 |
| world grey (no texture, blending, alpha test: debug 2) | 216 | 122 | 22 | 63 | 3.75 |
| world one draw a call (debug 2048) | 176 | 119 | 22 | 26 | 4.51 |
| the frame at half size (RAC1_WIIU_VPSCALE=0.5) | 183 | 120 | 22 | 33 | 4.27 |

- The draw count was not the bottleneck: 8x fewer world draws (4,000 -> ~220 a call) changed nothing on screen (CPU 4.5 -> 0.8 ms
  a call). The commands' building is ~11 ms a frame in all.
- The GPU's frame (WHBGfxFinishRender waits for it) is ~80 ms, ~55 of them the world's, and mostly pixels: a quarter of the
  pixels takes it to 33. The PS2 drew 512 x 416; 1080p is 8.75 times the pixels, with the plants' and rocks' overdraw. The
  texturing is a part (~13 ms), not the most. -> the 720p option, then the overdraw (early Z with the alpha test's discard, the
  passes' order), then the pixel shader.
- The game's own steps are ~24 ms each (~5 per host frame with RAC1_REALTIME): for 60 steps a second they must come under
  16 ms - the CPU work (VU0 emulation, moby joints, the passes) is the second front.
- The world's CPU prepare, 22 ms a frame on the game's thread (static frame, scene, triangles, tfrag's CPU route): to a worker core.
- The GamePad's picture is cheap (~10%).

### CPU and GPU overlapped, the world's preparation on a worker (2026-10-28)

- wiiu_gfx.cpp no longer waits for the GPU at a frame's end (WHBGfxFinishRender's GX2DrawDone): the frame's commands' timestamp is
  waited for at the next frame's start, before it writes the memory the GPU reads (RAC1_WIIU_OVERLAP=0 as before).
- The world's preparation in two halves: what reads EE memory on the game's thread, build_triangles and tfrag's CPU route on a
  worker on core 0 (RAC1_WIIU_WORLD_ASYNC=0 all on the game's thread), joined before the world's draw.
- Veldin explore, everything on: host frame 231 -> 155 ms, 3.48 -> 4.96 frames a second, 17.1 -> 23.4 game steps a second; the
  GPU's wait 81 -> 6 ms. Left on the game's thread: the game's steps (~117 ms: ~5 x 23 ms) and the world's first half (~19 ms:
  the colours' compare, static_frame_build, scene_from_static). The picture as before (shots at game frames 150 / 200).

### The game's tfrag pass off (2026-10-28)

- RAC1_TFRAG_PASS_OFF=1 (a Wii U default now, with RAC1_NO_GRAPHICS=2, RAC1_MOBY_MRP=1, RAC1_WORLD_PASS_SKIP=6: wiiu_main.cpp, run.cfg
  may say otherwise): the game's tfrag pass is not run, nor its relighting at the passes' end (which reads the pass's lists: it threw
  on bad pointers without them) - the tfrags are the host's (tfrag_decide, the CPU route), their colours the level's load's. The game
  state the same on the PC's walk (hero and camera, 600 frames explore; across a level change). Veldin: 23.4 -> 25.4 game steps a
  second, 4.96 -> 5.33 frames a second; the ground drawn as before.
- The way back to the menu hung with the frames overlapped: the frames after the game's end no longer overlap (WHBGfxFinishRender's
  own wait), the end waits for the last frame's timestamp and the world's worker before the backends free their data; each step on
  the log ("rac1-wiiu: ending: ...").
- The game's own profiler (RAC1_PROFILER_DUMP=120) per host frame (~4.7 steps): the moby pass (the draw loop) 36.8 ms, the world
  passes 20.7, logic 14.0, moby updates 13.2, joints / skinning 12.2, the chain reader's wait 7.1 -> next: the moby pass's work the
  MRP does not need (its GS packets).
- The stutters (the game's thread stalled while the IOP's mirror plays on: "frames played alone") come with streams and files read
  from the ISO on the SD on the game's thread: the reads to a thread of their own (as the PS2's IOP did them), the disc's files
  extracted onto the SD instead of the ISO.

### The hitches: the disc's files on the SD card (2026-10-28)

- RAC1_WIIU_TIMING's [hitch] lines (a host frame over 400 ms, its stages) and RAC1_DISC_SLOWLOG=<ms> (a disc read slower than that, its
  file and the open's share): over 600 Veldin frames one hitch, game frame ~141, a 315-375 ms game step - the HelpDesk's voice stream
  (toc/row_1ac4.vag) opened and read on the game's thread: its open 190-228 ms, then each 64 KB read of it 100-127 ms (~0.5 MB/s;
  data.wad's 13 MB read at ~22 MB/s). On the PC no step passes 10 ms.
- Not the folder: toc/'s 1,581 files spread in 40 sub-folders (toc/1a/row_1ac4.vag; disc.cpp files_host_path on the Wii U,
  ports/wiiu/tools/sd_disc_layout.py for the card's copy) opened as slowly. The streams' sectors read in runs of 32 (one read for
  32, the same bytes) changed nothing either.
- Next: the streams' file I/O off the game's thread (as the PS2's IOP read them: the game asks, a reader thread on another core
  opens and prefetches, the game's IOP takes the bytes when there); and why an open takes ~200 ms on the card (the stat after the
  open? the file system's client on a busy core?).

### The streams' reads off the game's thread (2026-10-28)

- RAC1_STREAM_ASYNC=1 (the Wii U's default; 0 the PC's, the old path): runtime/iop/snd989_stream.cpp's streams read their parts through
  a cache both IOPs share (the game's and the mirror's), loaded by one I/O thread (core 2, priority 24) in 64 KB chunks, the part with
  the fewest bytes in first; a part is asked for at its 0x2c play and when it is queued (prefetched). A stream whose header is not in
  waits without a voice (its handle answered, 0x4f "not started", its times 0) and starts in the update once it is; a half whose bytes
  are not in is silence, the cursor kept (an underrun). Unheld parts evicted past 4 MB.
- On the PC with RAC1_DISC_DELAY_MS=200 (each disc read 200 ms; the Wii U-like path, RAC1_REALTIME, Veldin explore 600 frames): off,
  36 game steps of ~202 ms (the HelpDesk's start at game frame 141, then the music's refills every ~15-20 frames), 39.4 game steps a
  second; on, none after the level's start, 55.7 a second. The streams' bytes queued / played the same (music 261,120 / 230,400,
  HelpDesk 199,680 / 168,960), the hero and camera the same.
- On the console (2026-10-28, Veldin explore 250 frames, RAC1_STREAM_ASYNC on by default): the hitch at game frame ~141 gone (no step
  over 150 ms, no host frame over 400 ms past the level's start); the HelpDesk's voice starts 463 ms after its play, the game not
  waiting. The level's music started 5 s after its play: the I/O thread's reads wait behind the level's load (disc.cpp's files_handle
  opens under its lock) - to look at (the open out of the lock; the level's own reads first).

### The moby pass without its GS packets (2026-10-28)

- What the moby pass does per moby, and who reads it: (a) the MRP's scene (mrp_scene.cpp): the draw observer's moby fields and skinning
  record, the poses' joints, and - decoded from the chain - its parts (the MSCALs, the model-to-view matrix each part's program reads,
  the fade's / alpha test's GS state); (b) game state: the culling and the drawn flag (+0x31), the LOD counters and the per-group depths
  on the scratchpad, the skinning records in the ring (with the chain addresses of each part's data), the sub-mesh records, the deferred
  mobys' shadow words (0x1ac600, read by the drop shadows), the texture groups and VRAM slots (0x19a600 / 0x199e80), the packet
  buffer's positions (0x1611c0, 0x160000); (c) the GS / VU1 path only: the mobys' chain parts (matrix uploads, UNPACK headers, REFs,
  the DIRECTs, MSCALs) and the texture uploads' packets - the reader skips their ranges.
- RAC1_MOBY_PASS_MRP=1 (a Wii U default, wiiu_main.cpp; off elsewhere; on only in a frame with every MRP skip on, RAC1_MRP_SKIP_PARTS
  f): the draw loop and moby_textures compute everything as before but write none of (c); the packet pointer advances as if they
  had, so (b) is unchanged to the byte. Each moby's range is one NEXT over it (the uploads' too), and the draw loop gives the observer
  the parts it would have written (moby_draw.h MobyDrawPart), turned into the scene's parts as the chain's decoding makes them (their
  GS state read at once): the scene's chain decoding is not run. moby_skips_note gives the mobys one after another one range (the VU1
  double buffer flipped the same number of times at its first tag) and its memo checks the ranges' first tags, not a hash of the
  whole chain. RAC1_MOBY_PASS_MRP=verify writes the chain too and compares the given parts with the decoded ones.
- The proof (the PC, the Wii U-like path RAC1_NO_GRAPHICS=2 RAC1_WORLD_PASS_SKIP=6 RAC1_TFRAG_PASS_OFF=1 RAC1_MOBY_MRP=1
  RAC1_NO_GRAPHICS_WALK=1 RAC1_VU1_LIKE_WIIU=1): verify, explore 600 frames: 10,331 mobys / 93,834 parts (911 with the shadow mesh,
  1,775 parts with a GS state), 0 differ. Mode on against off, explore 600 and RAC1_GOTO=3@60 forward 400: the hero / camera lines the
  same; the scene's per-frame hash (RAC1_MRP_SCENE_HASH=frame: everything its users read, not the unpacks) the same on every frame;
  RAMHASH: only pages inside the two packet buffers differ (explore 0x2f7000-0x338fff and 0x4b7000-0x517fff, the buffers at 0x2f4000 /
  0x4b4000; the same on the level change, its loading screen not in the mode), the scratchpad and everything else the same; the chain
  reader's frame (every primitive, its state, vertices and texture pixels, the moby mark, the VU1 flips) the same on every frame.
- The PC's times (ms per game frame, explore 600 frames, three runs each): the draw loop 0.042 -> 0.039, the uploads 0.0021 -> 0.0015,
  moby_skips_note 0.027 -> 0.0017 (the chain's hash gone), the scene's decoding 0.022 -> 0.0003 (the observers 0.0095 -> 0.011), the
  chain worker 1.29 -> 1.18: about 0.1 -> 0.05 ms of the game thread's work. The profiler's "moby pass" zone barely moves (0.21-0.27
  ms, noise): on Veldin's start ~17 mobys are drawn, and the zone is mostly the drop shadows (moby_shadows: ~0.18 ms a frame of the
  zone's ~0.23 on the PC) and the culling / lighting's VU0 arithmetic, all (a) or (b).
- Next: the drop shadows (shadow_points, shadow_quads, shadow_packets - generated code) are most of the zone; skipped whole they change
  the overlay's globals (0x15f000-0x161fff), 0x1ac000, the scratchpad, the packet positions: only their packets could go, with
  shadow_packets' positions kept. Then the VU0 helpers (fin_lanes, mulbc, maddbc) on the paired singles.

### Played with the GamePad's debug menu (2026-10-28 evening)

- The menu (RAC1_WIIU_MENU=1, debug_menu.cpp) has two pages: Picture (the world's kinds, the mobys, the 2D, the overlap, the world's
  worker, the merged draws, the resolution) and Game (game_skips.h: the mobys' updates, the hero, the particles, the camera, the game's
  sound, HUD / lights / help, the effects, the shadows, the joints, the catch-up frames' drawing), with the host profiler's game stages
  on the right (and on the log, "[game]").
- Veldin, everything on, 1080p: ~150 ms a host frame, ~5 game frames a host frame. Turning the picture's pieces off barely moved it:
  the CPU. With every game part off and the resolution 0.5: 33 ms (30 fps) - the GPU's fill at full resolution then holds it at ~66 ms
  (1.0) / ~55 (0.75).
- The game code's stages a host frame (5 game frames): the moby pass 31-38 ms, the joints 13-22, the world passes 13-17, the moby
  updates 10-14, the hero 4-9, the camera 2.5-5. Most of it was the drawing of catch-up game frames never shown: RAC1_DRAW_LAST_STEP=1
  (now a Wii U default; game_skips.h) runs render_frame for the last game frame of a host frame only. Same place: 150 -> 83 ms a
  host frame, the game code 112 -> 45 ms (the moby pass 34.6 -> 7.4, the joints 22.5 -> 4.6, the world passes 17 -> 3.5); the game
  near its real speed again and the sound's stalls far fewer. On the PC (REALTIME, a 60 ms host delay, explore 900) the game runs on;
  its state drifts a little (hero 136.15 -> 136.30 at frame 840): what the drawing writes that the update reads.
- Left, the biggest first: the world's prepare on the game's thread (18-30 ms a host frame); the GPU at 1080p (a 720p TV mode, the
  overdraw, the shaders); the moby updates and the hero (~2.4 / ~1.5 ms a game frame: the collision, the VU0 helpers); the last game
  frame's moby pass (~7 ms); the menus with the item icons (a ~500 ms hitch per change, ~67 ms of chain join a frame); the boot's
  title (under 1 fps: its passes keep their GS chain); a hitch when the camera turns fast (the music skips too).
- Fixed on the way: the pads read once per game frame came back empty (no new VPAD / KPAD sample) and dropped held buttons (menus
  moving twice, presses lost); quitting to the title (its bank's header read in the wrong byte order; the world-pass skips left the
  title's relighting stale pointers); RAC1_FRAMES=0 for a played run (a headless run stops at 300 frames). Still open: closing from
  the HOME menu leaves the console hung on its way back (after "ending: the menu asked for").

### The world's prepare, the GPU, the way out (2026-10-28, later)

- The world's prepare (RAC1_WIIU_PREP_TIME=1: its first half's parts every 120 calls) waited 5-17 ms a host frame for the chain's
  reader, only for the GS registers at the first world pass: the frame before's are used now (mrp_world_wiiu_chain_joined; they
  hardly change; RAC1_WIIU_REGS_LAST=0 waits as before). The ties' / shrubs' colours (3.4 ms: the whole table compared every frame,
  though the relighting is off) are compared every RAC1_WIIU_COLOUR_EVERY host frames (30). The prepare: 18-30 -> 8-10 ms.
- The GPU: its passes timed with GX2SampleTop/BottomGPUCycle (RAC1_WIIU_GPU_TIME=1; the samples count a 27 MHz clock; the first run
  with them always on hung the console - not yet known why, hence off by default). Veldin at 1080p: the world ~36 ms, the mobys ~29.
  Every pixel shader has KILL_ENABLE (its discard) and WHB never sets up hierarchical Z: no early / hierarchical depth culling at all.
  Hierarchical Z (RAC1_WIIU_HIZ, now on by default; the menu's Picture page flips it): 101 -> 70 ms a host frame at the same place,
  the GPU's wait 9 -> 0 ms, the swap's 12 -> 0; with the pixels scissored away (the menu's "GPU test") still ~69: the GPU no longer
  holds the frame. The pixel shaders without discard (RAC1_WIIU_NOKILL) and the lighter mip choice (lean) measured nothing more;
  the lean variant's vertex shaders (vLodP) made the world untextured on the console: left out.
- To watch with the passes still to port (the sky, the effects, the alpha-fail passes, stencil shadows): a depth function changed
  mid-frame leaves hierarchical Z wrong - each pass's depth state checked against it.
- Leaving: HOME's close hung the console - wut's WHBProcIsRunning, called again after it returned false, runs ProcUIShutdown and
  then waits for a message that never comes. The exit path now asks ProcUI once (wiiu_main.cpp proc_running); the frames stop at
  the foreground's release. Also: the pause menu's Quit ends the game (RAC1_QUIT_ENDS=1, a Wii U default; the boot's title is under
  1 fps there), the GamePad menu's "Exit game", the Minus button held a second. All four seen back at the Wii U Menu.
- Veldin's start, everything on, 1080p: ~70 ms a host frame (from ~150 this morning), all of it the CPU's.

### The poses in plain floats (2026-10-28, later)

- What the console ran: moby_joints_pose's loops in their "SIMD" form (RAC1_VU0_SIMD), which on the Wii U is the SSE2 shim on GCC
  vectors (runtime/vu/vu_simd_shim.h: no AltiVec, four scalar lanes, every float op's operands clamped with integer masks, every
  result's MAC / status flags built lane by lane by fin_lanes). Built for the Wii U, the joints' matrices loop is 4553 instructions
  (18 KB, mostly straight-line, run once per joint), the rotations' 3293, the bind matrices' 1704, the lists' 1303: 43 KB together,
  more than Espresso's 32 KB instruction cache.
- Nothing reads what the plain-float form drops: the matrices go to the skinning, the MRP's pose observer and the strafe's hook only
  (all drawing; with RAC1_DRAW_LAST_STEP the game runs without them for most game frames already); no CFC2 of the status flag in
  the whole game and no VU0 program tests it; every MAC read (cfc2 17) follows an op of its own; the pose's later ops load or set
  the scratch registers they read, the skinning loads its own.
- RAC1_WIIU_FAST_POSE (moby_joints.cpp; 1 on the Wii U, 0 on the PC; the debug menu's Game page, "Fast poses": fast -> exact -> off):
  the four hot loops, the blend channels' loop and the one-keyframe rotations in plain floats, the scratchpad through its host
  pointer, the s16 keys made floats through their mantissa (no integer conversion). 1: the multiply-adds fused (fmadds), 1 / sqrt
  from frsqrte and three Newton steps (no fsqrt on the 750: sqrtf is software). 2: each op rounded as the VU0's, sqrtf. The
  matrices' loop is 325 instructions (1.3 KB), the rotations' 300, the bind matrices' 238, the lists' 158, the blend channel 242;
  the exact mode's copies are about as big (only one runs).
- RAC1_WIIU_FAST_POSE_CHECK=1 builds each pose twice (the VU0's way, then the fast one, kept) and prints "[pose]" every 2000 poses.
  PC, Veldin 600 frames (10000 poses, 161972 joints): mode 2 bit for bit the VU0 path's; mode 1 the rotation rows at most 1.1e-6 off,
  the translation rows 0.0068 (joints ~1000-8000 units from the moby's origin, where the bind's terms cancel: a few units in the
  last place of those terms). Two Newton steps from a 5-bit estimate left the rows 1e-5 off and the translations 0.05: three.
- The PC's own times say nothing (its VU0 path is SSE / AVX2, ~0.1 ms a frame either way). On the console to measure: the "[game]"
  line's "Game: moby joints / skinning (VU0)" with the menu's row at fast / exact / off (3-5 ms a host frame before).

### The game's update on a core of its own (2026-10-28, night)

- A host frame was the steps (~50-58 ms, 4 game frames), the prepare (~13: chain start, the mobys', the world's, the sky's capture) and
  the drawing (~20-25 with its waits: the world's worker join ~6, the swap's ~6.5) in series on core 1. Now (RAC1_WIIU_GAME_THREAD, on by
  default; the GamePad menu's Game page, "Game thread", in place of that page's "Exit game" - still on the Picture page): the frame
  prepared at the end of a host frame is drawn by the next one while its game frames run on the game thread (runtime/ee/game_thread.h:
  an OS thread on core 2 - RAC1_WIIU_GAME_CORE -, priority 25 - RAC1_WIIU_GAME_PRIORITY, below the streams' I/O thread, 24 -, a 1 MB
  stack, the main thread's FPSCR), joined before the next prepare. One frame of latency; a host frame about max(steps, drawing) + prepare.
- What the drawing read of the game and is now taken at the prepare: the backends' view-projection (mrp_world_view_proj, live in the
  tfrags' and the sky's frame blocks), the GS depth map (render_gs_depth), the frame size (0x1518d0) and the mobys' mip choice:
  mrp_world_gx2.h Gx2DrawView. The world's second half (its worker, now beside the update) read the sky's texture entries in EE memory:
  resolved by the first half (g_wiiu_sky_at_prepare). The chain's reader is joined before the update starts (its VU1 runtime's globals,
  the GS's local memory); the update's GS transfers do not wait for the drawing (it reads none of the local memory). The pads stay read
  per game frame, by the game thread alone meanwhile (the menu takes the touch from its last sample, its taps held until the join).
  thread_local being nothing here, the profiler's and the game thread's own "which thread" flags are by OSThread now; the console's
  line buffer (stdout and stderr share it) is locked.
- Not beside a frame of a blocking loop, a level's exit or load, the boot, a movie, the full-screen menu, the Vendor's picture: that
  frame drawn first, the update after it, in series.
- PC (RAC1_NO_GRAPHICS=2 + the Wii U's settings, RAC1_NO_GRAPHICS_WALK=1, RAC1_GOTO=0@60,1@700, 1500 frames, `forward`): RAMHASH with
  RAC1_WIIU_GAME_THREAD=1 identical to =0's, one and three game frames a host frame (547 / 182 host frames on the game thread, the
  rest the level change's). The PC's own game thread (RAC1_GAME_THREAD=2 RAC1_PIPELINE=1) identical to =0's; the references unchanged.
- To measure on the console: the "[timing]" line's "game thread join" (the main thread waiting for the steps) and its tail "game
  thread: N of 60 host frames, its steps X"; whether the steps are slower on core 2 (512 KB of L2 against core 1's 2 MB):
  RAC1_WIIU_GAME_CORE=0 / 2, and =0 for the series.

### The host's frame on the main thread: the world's whole prepare on core 0, the sky's run there, the GamePad copied (2026-10-28, night)

- The world's prepare's first half (8.7 ms a host frame on the console: the ties' / shrubs' frame 2-2.8, the tfrags' colours and
  decisions 1.5, the scene 4.5-5) now runs on its worker (core 0) before the second half (RAC1_WIIU_PREP_ALL, on by default; =0 on the
  main thread as before). The worker reads no EE memory - the game thread's update writes it meanwhile -: the main thread takes, at the
  prepare, the snapshots' copies (static_note_pass's, tfrag_note_pass's: the next update rewrites them), the projection A and the camera,
  scene_from_static's constants (the tfrag view and VU1 constants, the tie A state packet, the shrub A fog: mrp_world_wiiu.inc
  SceneStaticConsts) and the sky's textures; the culling reads the level's tables (class records, bounds, instance data, the tfrag table
  and its boxes) from host copies of their EE pages (mrp_world_wiiu.inc EePages: noted once a level - Veldin 538 pages, 2.1 MB -, the
  96 class flags' words copied every prepare, the rest RAC1_WIIU_PAGES_EVERY prepares round, default 30; a read outside them noted
  again). The colours stay on the main thread (EE reads): the tfrags' now as the ties' - the whole table every
  RAC1_WIIU_TFRAG_COLOUR_EVERY host frames (30), between them the entries the relighting relit (world_relight's tfrag_colours_touch).
  The relighting's marks (relight_visible) the worker's culling makes are applied by the next prepare, never while the update reads
  them: an entry relit a host frame later than before.
- Checked on the PC (RAC1_MRPW_PAGES_CHECK=1, any host: the capture step's static_frame_build / tfrag_decide made again from the
  snapshots' copies and the pages, compared bit for bit; with RAC1_MRPW_STATIC_CHECK=1 for the instances): Veldin GOTO=0@60 forward
  900 frames and Kerwan GOTO=3@60 forward 1500 (1420 pages, 5.6 MB), every frame the same instances, rows, draws, far variants and
  tfrag decisions, no read outside the pages. The sky's split (RAC1_MRPW_SKY_CHECK=1 RAC1_MRPW_THREAD=0, Veldin 600 frames): as the
  walker's but at frame 60, as before. The references unchanged, RAMHASH as main's; gt_check.sh's runs identical, and the Wii U-like
  run with the game thread and these switches off (=0 / 1 / 0 / 0 / 0) the same as with them on (the PC never reads them).
- The sky: its capture (1.9-3.7 ms on the main thread) is now the range's copy alone there (sky_capture_copy: the tags' streams, the
  asset, its entries, A); the codes - the DIRECTs' GIF streams, the particle program on the private VU1 - run on the worker
  (sky_capture_run) after the scene, once the chain's reader is done with VU1 (mrp_chain_wait_idle: the VU1 runtime's state is one for
  all threads here). The same packets in the same order (sky_capture is the two in a row on the PC). The game's sky pass still runs: the
  capture reads its chain. A sky without it - the decoded asset drawn once a level, the shells turned on the GPU - needs each level's
  variant of 0x23d968 for the host (Veldin's: shells 0, 1, the stars from the particle program, ALPHA_1 before shell 2, shell 3 turned
  about z by the frame counter, shell 4) and the decoder checked against the capture; the cache of option (a) gains little, the pass
  culling its clusters to the frustum and the stars being screen sprites: the captured set changes whenever the camera turns.
- The GamePad (RAC1_WIIU_GAMEPAD): 1, the default, the TV's colour buffer copied to the GamePad's scan buffer
  (GX2CopyColorBufferToScanBuffer scales it) - the frame no longer drawn twice, neither its CPU work (~2 ms) nor its GPU work; 2 drawn
  every other host frame; 0 drawn every frame as before. The debug menu: its quads made again only when its text changes, and only
  their bytes flushed (RAC1_WIIU_MENU_CACHE=0: every frame, the whole 1.1 MB buffer flushed, as before).
- The TV's 2D built before the wait for the GPU's frame before (RAC1_WIIU_2D_PREBUILD, on; the chain's reader joined there first):
  the "[timing]" slot "2D build (before the GPU wait)" now holds that part of "2D draws", overlapping the GPU.
- To measure on the console ("[timing]"): "world's prepare" (8.7 -> well under 1 ms expected), "sky's capture" (-> ~0.1), "GamePad"
  (-> ~0 with the menu off), "2D draws" (its build moved out); "world's worker join" may grow: core 0 now runs the chain's reader and
  the whole prepare (~15-18 ms of work on the console's times) - RAC1_WIIU_PREP_TIME=1 prints both halves' parts.

### The world pass's GPU time: the depth test after the shader, the frame sent at its end (2026-10-28, night)

- The GPU's marks (RAC1_WIIU_GPU_TIME=1, Veldin 1080p) had the world at 27-35 ms, the sky 2.5-9 for ~10 draws, and the GPU idle 9-43 ms
  between frames, with the main thread still waiting 22-36 ms for it. Two causes, both read off the code and the .gsh files (not yet
  measured on the console):
- Every pixel shader the world, the tfrags, the sky and the mobys draw with has DB_SHADER_CONTROL Z_ORDER = LATE_Z (CafeGLSL writes it
  for every shader: 0x50 with discard, 0x10 without - the RAC1_NO_ALPHA_TEST builds included, which is why RAC1_WIIU_NOKILL measured
  nothing): every pixel the rasterizer makes past hierarchical Z runs the whole shader before its depth test. mrp_world.frag's path is
  ~66 ALU groups a pixel (the merged draws' unit tree ~10 CF instructions more; `glslcompiler -v`): at Latte's 32 VLIW lanes about half
  a pixel a clock, ~7 ms of shading per 1080p screen of fragments - the world's overdraw and its alpha-fail passes shaded in full.
  RAC1_WIIU_EARLYZ (on): the 3D's pixel shaders' Z_ORDER made EARLY_Z_THEN_LATE_Z at their load (Mesa's r600 order with a discarding
  shader too; LATE_Z there only for the fixed-function alpha test, off here; RE_Z left alone - lockups on R6xx / R7xx). With a discard
  the early test only culls, the write stays late. RAC1_WIIU_NOKILL is now on by default: the batches with no alpha-tested draw get
  their depth written before the shader too.
- GX2 sent a frame's commands to the GPU at its end (WHBGfxFinishRender's flush): the GPU drew nothing while the CPU made the frame (the
  world's worker join, the draws) - the "idle between frames". RAC1_WIIU_FLUSH (2): GX2Flush after the depth clear, the sky, the world
  and the mobys (1), and inside the world every RAC1_WIIU_FLUSH_DRAWS GX2 draws (64) (2); 0 as before. A host frame should go from
  about the making plus the GPU's time to the larger of the two.
- RAC1_WIIU_UB_BULK (on): the uniform blocks (the world's, the sky's, the mobys' rings) only flushed out of the CPU's cache, their GPU
  caches invalidated once per call and after a ring's wrap, in place of a GX2Invalidate (a SURFACE_SYNC) per block - two per moby draw.
  The world's merged draws (mode 3) had none per draw already: its slots one invalidation per frame, its pixel blocks one per batch pass.
- The sky's batches without an alpha-tested draw use shaders/mrp_sky_noat.gsh (Gx2Perf::nokill; mrp_sky.vert + mrp_world.frag
  RAC1_NO_ALPHA_TEST). The world's path switches (tfrag <-> ties / shrubs) set the shader mode only when it changes.
- RAC1_WIIU_GPU_TIME=2: the sky's and the world's passes marked at each batch pass's end; the "[gpu-world]" line sums the GPU's time
  by kind (sky / tfrag / ties-shrubs, pass 1 / 2, the rest) with the command processor's gap (its top sample after the GPU had drawn
  everything before: the commands not there yet - the CPU's pace).
- To measure on the console: bench.sh with RAC1_WIIU_GPU_TIME=2, then RAC1_WIIU_EARLYZ=0, RAC1_WIIU_FLUSH=0, RAC1_WIIU_UB_BULK=0 one
  at a time; RAC1_WIIU_PIXELS_OFF=1 for the passes without their pixels.

### The world's pixels: a fast pixel shader, the 3D upscaled (2026-10-28, late night)

- Measured on the console before (Veldin, 1080p, hierarchical Z and the early depth test on): the world pass 27-35 ms of GPU a frame, 0.7
  ms with its pixels scissored away (RAC1_WIIU_PIXELS_OFF=1); the early depth test changed nothing - not occluded overdraw: the cost of
  each visible pixel's shader.
- RAC1_WIIU_FASTPS (on; the debug menu's "GPU: shaders / test" row: fast+nokill -> fast -> nokill -> off -> the tests): the world's, the
  tfrags' and the sky's pixels drawn by shaders/mrp_world_fast.frag. What mrp_world.frag decodes per pixel from the draw's texture word and
  draw_flags (TME, IIP, the texture function and TCC, the mip level's L / K / LCM / MXL and the texture's levels, the alpha test's mode,
  ATST, AREF) the vertex shaders built RAC1_FAST make once per vertex (mrp_world_fast.glsl's fast_terms: flat outputs), and the pixel's
  work is straight arithmetic: t' = t tm + ta, o = t' (A + B c) + C c.a per channel, the level floor(clamp(2^L (-log2 |q|) + K + 0.5,
  0, the last level + 0.5)), the alpha test one comparison (y = a 255 - AREF, y m + |y| n >= th; the strict
  tests' th the next float above). `glslcompiler -v` (rac1-scratch/gpuperf3: count.py, sem.py, pairs.py):
  - mrp_world.frag (every path before): 95 ALU groups, 110 CF instructions, 17 samples, 11 GPRs in the program - ~66 groups on a textured
    pixel's path, ~10 CF more through the merged draws' unit tree; RAC1_NO_ALPHA_TEST 77 groups, 105 CF;
  - mrp_world_fast.frag, one sampler (the tfrags', the sky's): 16 ALU groups, 4 CF (one ALU clause, the sample, one ALU clause, the export),
    11 GPRs; without the alpha test 10 groups, 8 GPRs;
  - with the units (RAC1_UNITS, the ties' / shrubs' merged draws, mode 3): 70 groups in the program, on a pixel's path ~26 (7 before the
    tree, 4 of its branches, 2 copies, 13 after; ~19 without the alpha test) and ~19 CF;
  - the vertex shaders: the ties' 78 -> 98 groups (24 -> 26 GPRs), the tfrags' / sky's 6 -> 31: per vertex, ~100 K vertices a frame.
- Each fast build is a pair: its own vertex shader (outputs vQF, vLod, vFnA, vFnB, vAt, vSel in place of vLodQ, vFog, vTex, vState)
  compiled with its pixel shader, bound together by the CPU (gx2_pair_variant: accepted when its attributes, blocks and samplers are where
  the main shaders' are - the main fetch shader and bindings serve it; the varyings' semantics match within the pair: sem.py). Not the
  vertex shader "standing in" (the lean builds' fault: a vertex output only some pixel shaders read). The shade debug and the grey test
  keep mrp_world.frag. The old builds' .gsh files are byte-identical to before.
- RAC1_WIIU_WORLD_MERGE=4 (the menu's merge row: 3 -> 4 -> 0): as 3, the items split by texture unit and the unit bound by the CPU - one
  sampler, the pixel shader's unit tree not run (uB.x 0), at the cost of more GX2 draws (an instance's packet of two units is two items,
  and instances interleave less). To compare with 3: the "[gpu-world]" ties/shrubs passes and the "[timing]" world draws.
- RAC1_WIIU_3D_SCALE=<0.25..1> (default 1; the menu's "3D resolution" row: 1 -> 0.75 -> 0.667 -> 0.5): the TV's sky, world and mobys drawn
  into a colour and depth buffer that much smaller (its own hierarchical Z), then a quad (shaders/wiiu_blit.gsh, bilinear) over the TV's
  frame before the 2D, drawn at the TV's resolution; the GamePad's copy and the shots read the TV's buffer after it. The buffers inside the
  TV's depth buffer (MEM1: the TV's 3D does not use it then; the depth cache flushed before, the colour and depth caches after) as far as
  they fit - 1280 x 720 both, 1440 x 810 the colour alone -, the rest in MEM2; RAC1_WIIU_3D_MEM1=0 both in MEM2. "[gpu]" has a "3D
  upscale" pass. (RAC1_WIIU_VPSCALE / the "Resolution" row, the frame's share, is another thing: the frame smaller inside the TV's buffer.)
- To measure on the console (bench.sh, RAC1_WIIU_GPU_TIME=2): the fast shaders against RAC1_WIIU_FASTPS=0; RAC1_WIIU_WORLD_MERGE=4
  against 3; RAC1_WIIU_3D_SCALE=0.667 (and 0.5) against 1, and RAC1_WIIU_3D_MEM1=0 against 1 at 0.667.

### The game's IOP without its mix (2026-10-28, late night)

- The console's sampling profile (the game thread, core 2, Veldin): ~10% of it in the game's IOP's sound synthesis - snd::Synth::Tick
  9.9% inclusive, Voice::Run 7.9% (4.1% self), DecodeSamples 1.5%, the envelopes ~1.7% - from vsync_passed's snd989_iop_render, every
  vsync. With the mirror on (ADR 0013, the Wii U's way) nobody hears that output: the game's IOP's pcm only feeds RAC1_AUDIO_DUMP and
  RAC1_IOP_VERIFY's comparison (snd989_mirror_vsync keeps it for that alone).
- RAC1_IOP_STATE_ONLY=1 (the Wii U's default, wiiu_main.cpp; off on the PC): with the mirror on and neither of those, vsync_passed calls
  snd989_iop_tick_state (snd::Player::TickState) in place of the render: the handlers ticked at the same samples (every 200th), and
  between two of their ticks each voice run on its own (Synth::TickState, Voice::RunState) - the decode position (the fifo's pushes and
  pops counted, no ADPCM decoded), the block headers read at the same positions (loop start, loop end / repeat: the jump, ENDX, the
  ADSR's stop at the end), the pitch counter and the envelope; a voice found dead at a sample's start dropped at that sample, as
  Tick's remove_if does (the handlers see a voice's end by its weak_ptr expiring). Not run: the samples (decode, interpolation, noise),
  the volumes' ramps, the reverb, the mix. The envelope's Step / Run made inline (envelope.h): both paths, the mirror's too.
- Exact on the PC (RAMHASH every frame, switch 0 against 1): Veldin explore with level changes (RAC1_GOTO=0@60,1@700, 1600 frames;
  0@60,1@700,2@1800, 3000 frames; forward 0@60,3@600, 2000 frames), each with the mirror on the game's thread (RAC1_IOP_MIRROR_TEST=1:
  the mirror's output, RAC1_MIRROR_DUMP, byte-identical too) and on the null device's audio thread (RAC1_AUDIO_DEVICE=null). The check
  is sensitive: RunState without the end's ADSR stop differs from game frame 20 on.
- The saving on the PC (the game thread's CPU time of the call per vsync, the 3000-frame run, 3.6 voices on average): 80-117 us the
  render, 18-32 us the state alone (handlers ~3.5 us of it), ~75% less.
- To measure on the console: RAC1_IOP_STATE_ONLY=0 against the default 1 - the "[game]" line and the "[timing]" game thread's steps;
  the sound unchanged (it is the mirror's).

### M0 / M1: GX2 counters, real-time stall threshold, moby LOD undo log, OS thread pinning, cached static level texture table (2026-10-29)

- **GX2 and effects instrumentation (`ports/wiiu/game/gx2_counters.h`, `wiiu_gfx.cpp`):**
  - Per-site `GX2DrawDone`, `GX2Invalidate`, draw-call and `DCFlushRange` byte counters (`[gx2]` log line), plus host timing slots `effects' prepare` / `effects' draws` and GPU mark `effects`.
  - Console measurement (explore, 1080p × 0.75): `GX2DrawDone` is ~0.0/frame on Veldin and ~0.2/frame on Novalis (only on ring wraps); the GPU is idle 15–35 ms/frame. The bottleneck is CPU work on Core 0 (world prep + chain reader) and Core 2 (game thread render passes).
- **Skip duplicate `mrp_lod_moby_shadow` pass on Wii U (`src/game/ovl/overlay_entry.cpp`) and `kStallMs = 250` (`src/host/orig_main.cpp`):**
  - On Wii U the real moby pass already widens and runs LOD (`mrp_lod_moby_widen_real`), making the second `mrp_lod_moby_shadow` pass redundant.
  - With `kStallMs = 50`, every Novalis host frame (>50 ms) was treated as a disc load stall and clamped to 1 game step per host frame (slow-motion). Raising `kStallMs` to 250 ms on `__WIIU__` restored real-time catch-up (3–5.7 steps per host frame).
  - Console benchmark: Veldin **52.6 → 42.6 ms** average (last window 33.4 ms, 30 fps); Novalis **87.8 → 76.7 ms** at full game speed.
- **Per-field undo log in `mrp_lod.cpp` (`patch_moby_logged` / `patch_moby_undo`):**
  - Replaced the 4 KB page-by-page EE memory journal in `mrp_lod_moby_widen_real` (when no projection override is needed) with a compact per-field undo log (`patch_widen` only).
  - Console benchmark (`m1c_novalis`): Novalis moby draw loop **20.7 → 12.7 ms**, game code **50.6 → 38.2 ms**, host frame average **76.7 → 66.1 ms**.
- **True core affinity via `PinnedThread` (`src/platform/pinned_thread.h`, `mrp_chain.cpp`, `mrp_world_renderer.cpp`):**
  - `OSSetThreadAffinity` inside a `std::thread` did not prevent worker threads from migrating across cores and stealing Core 2 from `game_thread`. `PinnedThread` creates threads via `OSCreateThread` with explicit core affinity flags on `__WIIU__` (Core 0 default for both the chain reader and the world prep worker; moving the chain reader to Core 1 at priority 16 starved the Cafe OS main thread during Novalis cutscenes).
- **Static level texture table cached across frames (`mrp_world_scene.h`, `mrp_world_scene.cpp`, `mrp_world_static.h`, `mrp_world_static.cpp`):**
  - Profiling showed 41% of the world prep worker in `memcpy`, mostly `static_draws` copying `sl.tex` (thousands of qwords) into `SceneFrame::tex` twice every frame (once for ties, once for shrubs). `SceneFrame` now keeps `sl.tex` at index 0 across frames as long as `StaticLevel::gen` matches, and both kinds share it.
  - Console benchmark (`m1f`):
    - **Novalis** (1500 frames, 11 windows): `world's worker join` dropped from **9–32 ms (`m1d`) to 2.1–25.5 ms (`m1f`)**; host frame average **62.7 ms** (steady-state gameplay windows 2–6 at **41.1–44.2 ms**).
    - **Veldin** (1200 frames, 15 windows): host frame average **42.6 → 38.3 ms**, with the second half of the run locked at **33.2–33.3 ms (30.0 fps)**.

### M1h–M1l: True 3-core overlap (`RAC1_WIIU_CHAIN_OVERLAP_GT`, `RAC1_WIIU_PREP_ON_MAIN`), cooperative `tfrag_exec_cpu_fast` drain, and `mrp_fx_wiiu_prepare` fast path (2026-10-30)

- **Zero-alloc VU1 kicks (`runtime/vu/vu1.cpp`) and O(1) `sky_prepend` (`src/game/render/mrp_world_scene.cpp`, `mrp_world_renderer.cpp`):**
  - `vu1::start_program`'s `kick_at` passes `&vu.st.mem[(addr & 0x3ffu) << 4]` directly when the kicked packet does not wrap past `0x400` qwords (>99.9% of kicks), eliminating a per-kick `std::vector<uint8_t>` heap allocation and copy.
  - `scene_from_static_k` reserves `f.packets` and `f.insts` upfront and calls `sky_prepend` while `f.packets` is still empty, eliminating the O(N) front-insertion move of 5,800+ `ScenePacket`s every frame.
- **Immediate `game_thread` launch on Core 2 (`RAC1_WIIU_CHAIN_OVERLAP_GT`, default 1, `src/host/orig_main.cpp`):**
  - Previously, `wiiu_overlap` called `mrp_chain_join()` on Core 1 before launching `game_thread::run_async()` on Core 2, leaving Core 2 idle for 8–24 ms every frame while Core 0 was reading the chain.
  - Because `mrp_chain` operates on `g_capture` (already copied before `mrp_chain_start`) and the world prep worker reads frozen `g_pages` / `g_head_in` snapshots, `game_thread` can start on Core 2 immediately after `mrp_world_prepare` and `mrp_mobys_prepare` finish their EE reads.
  - Console benchmark (`m1i_novalis`): Novalis host frame average dropped from **62.2 ms (11 windows) to 49.2 ms (15 windows)** (**-13.0 ms / -21%**), with windows 6–10 hitting **33.4–34.6 ms (30.0 fps)**.
- **World preparation on Core 1 in parallel with Core 0 `mrp_chain` (`RAC1_WIIU_PREP_ON_MAIN`, default 1, `mrp_world_renderer.cpp`, `wiiu_gfx.cpp`):**
  - Previously both `mrp_chain` and `PrepWorker` were pinned to Core 0, so world prep sat queued behind `mrp_chain` on Core 0 while Core 1 blocked inside `mrp_chain_join()`.
  - `mrp_world_wiiu_join()` is now called at the top of `wiiu_gfx_frame()` before `mrp_chain_join()` and runs `wiiu_prep_head()` + `build_triangles()` on Core 1 while Core 0 runs `mrp_chain` and Core 2 runs `game_thread`. `mrp_world_wiiu_sky_capture()` also runs `sky_capture_run()` immediately when `mrp_chain` is already idle.
  - Console benchmark (`m1j_novalis`): Novalis host frame average dropped from **49.2 ms to 45.9 ms (16 windows)**; heaviest window dropped from **76.7 ms to 64.9 ms (-11.8 ms)**; `GPU busy` average **33.8 → 28.3 ms**.
- **Zero-heap `tfrag_exec_cpu_fast` and cooperative Core 1 + Core 0 tfrag drain (`RAC1_WIIU_TFRAG_PARALLEL`, default 1, `mrp_tfrag_gpu.cpp`, `mrp_world_renderer.cpp`, `mrp_world_gx2.cpp`):**
  - `tfrag_exec_cpu_fast` uses a 2-slot static scratch array (`g_exec_scratch[2]`), fixed `Mem::q` array, pre-decoded `m.v[4][4]` view floats, and `need_xyz = false` (skipping unused `Pj.v[0..2]` + `ftoi`). `build_triangles` preserves `F.tf` sub-vector capacities, and `tfrag_frame_cpu` uses `resize` instead of zero-filling ~1.2 MB per frame (`assign`).
  - While `mrp_chain` is still busy on Core 0 (`!mrp_chain_is_idle()`), Core 1 uses its pre-`mrp_chain_join()` wait time to drain tfrag draws (`slot 1`) without contending on Core 0. As soon as `mrp_chain` finishes on Core 0 (`mrp_world_wiiu_chain_joined()`), `PrepWorker` wakes on Core 0 (`slot 0`) to drain the remaining tfrag draws in parallel while Core 1 prepares effects, 2D, and GX2 sky/world draws.
- **`mrp_fx_wiiu_prepare` fast path (`src/game/render/mrp_fx.cpp`):**
  - Reuses `static` scratch vectors across VU7 batches, computes VU7 per-vertex `wp` and `fog` once per vertex `i < b.n` instead of recomputing `M * pt` and `vu7_geinv * (Me * pt)` 3× per vertex inside `emit_vu7`, and factors sprite unprojection (`p.type == 6`) + `VP.w` dot product.
- **Console benchmark (`m1l`):**
  - **Novalis** (1500 frames, 16 windows): host frame average **44.4 ms** (down from **87.8 ms** baseline — **nearly 2× faster overall**); 5 consecutive windows at **32.5–34.2 ms (30.0 fps)**; heaviest final window dropped to **54.9 ms** (from **64.9 ms** in `m1j` and **87.8 ms** baseline); `the CPU route` for tfrag dropped from **6.56 / 4.70 ms to 5.53 / 3.87 ms**.
  - **Veldin** (1200 frames, 16 windows): host frame average dropped from **38.3 ms (`m1f`) to 35.3 ms (`m1l`)** (down from **52.6 ms** baseline); 10 of 16 windows locked at **32.8–33.5 ms (30.0 fps)** (and 1 window at **16.7 ms / 60.0 fps**) with 8–13.8 ms of swap wait headroom.

### M1m: Per-draw / per-range Pass 2 culling (`RAC1_WIIU_P2_CULL`), Pass 1 `_noat` Early-Z depth writes, and Pass 2 `LESS` depth rejection (`RAC1_WIIU_P2_LESS`) (2026-10-30)

- **Why the world GPU pass was still 27–32 ms (`m1k`/`m1l`):**
  - The game sets `TEST_1 = 0x5360b` (`ATE = 1`, `ATST = GEQUAL`, `AREF = 0x80`, `AFAIL = RGB_ONLY`) across the entire static world pass (tfrag, tie, shrub).
  - Because `has_afail` was a batch-wide boolean (`b.ate && b.atst != 1 && b.atst != 0 && b.afail != 0`), **every** tfrag range and tie/shrub draw in the batch:
    1. Ran with `discard` (`KILL_ENABLE`) in Pass 1 (`mrp_world_fast.gsh` / `mrp_tfrag_fast.gsh`), forcing Latte to defer depth writes and Hi-Z HTILE updates until after the pixel shader.
    2. Was drawn a **second time** in Pass 2 (`RGB_ONLY`, `zwrite = false`, `zfunc = LEQUAL`), shading every visible pixel twice even when 85–95% of world textures and vertex colours have `alpha >= 0x80` on every texel and vertex!
    3. Even for genuine cutout draws (`pass2 == true`), Pass 2 used `LEQUAL` (`zfunc == 3`), so every opaque interior pixel (`alpha >= 0x80`) that had already passed Pass 1 and written its depth passed the depth test again in Pass 2 and was shaded a second time.
- **`RAC1_WIIU_P2_CULL` (default 1) and `RAC1_WIIU_P2_LESS` (default 1) (`mrp_world_gx2.h`, `mrp_world_backend.inc`, `mrp_world_merge.inc`, `mrp_world_renderer.cpp`, `mrp_world_gx2.cpp`):**
  - `load_textures` inspects all sampled mip levels (`0 .. mips - 1`) of each texture (`csm1(p)` palette alpha byte `[3] >= 0x80u`) and caches `TexRef::opaque`.
  - `tie_slot_opaque` and `tfrag_entry_opaque` cache per-instance / per-tfrag vertex-alpha opacity (`(w >> 24) >= 0x80u` on `g_rcols.mirror` and `g_tcols.mirror`, recomputed only when `updates` advances); shrubs test `si.alpha >= 0x80u`.
  - `draw_needs_pass2(k, fl, vert_opq)` checks whether any fragment in a draw or tfrag range can fail the alpha test (`ATST == 5` `GEQUAL` with `AREF <= 0x7f` or `ATST == 6` `GREATER` with `AREF < 0x7f`, given `TCC`, texture opacity, and vertex-alpha opacity).
  - In `mrp_world_gx2_draw`:
    1. **Pass 1 Early-Z depth writes:** items/ranges/draws with `pass2 == false` switch to `_noat` shaders (`mrp_world_fast_noat.gsh` / `mrp_tfrag_fast_noat.gsh`) via `set_p1_at(false)`, removing `KILL_ENABLE` so Latte writes depth and updates Hi-Z HTILE **before** running the pixel shader.
    2. **Pass 2 culling:** items/ranges/draws with `pass2 == false` are skipped completely in Pass 2 (and if no item/range in a batch has `pass2 == true`, Pass 2 is skipped for the entire batch).
    3. **Pass 2 `LESS` depth rejection (`RAC1_WIIU_P2_LESS`):** for the remaining ~5–15% of cutout draws in Pass 2 (`pass2 == true`), when `!(afail == 2 && zwrite)`, `GX2SetDepthOnlyControl` uses `LESS` instead of `LEQUAL` (`pass2_less`), so hardware Early-Z/Hi-Z rejects all opaque interior pixels that already wrote their depth in Pass 1, shading only the cutout fringe pixels that failed Pass 1!
- **Console benchmark (`m1m`, 1080p × 0.75):**
  - **Veldin (`RAC1_LEVEL=0`, 19 windows):**
    - **Host frame average: `35.3 → 30.3 ms` (~33.0 fps average, down from `52.6 ms` baseline, -42.4%)**, with many windows running at **`25.4–26.1 ms` (~38–39 fps)** and **`16.7–18.8 ms` (53–60 fps)**!
    - **GPU busy average: `31.9 → 17.8 ms` (-44.2% GPU time)**; `GPU wait (the frame before)`: **`10.3 → 0.0 ms`** (`10.5 ms` GPU idle headroom per frame).
    - **World GPU pass: `26.98 → 7.62 ms` (3.5× faster)**: `tfrag pass 1` `1.39 ms`, `tfrag pass 2` `3.50 → 0.17 ms`, `ties/shrubs pass 1` `11.17 → 5.15 ms`, `ties/shrubs pass 2` `9.45 → 0.89 ms`.
    - **CPU `world draws`: `5.6 → 2.5 ms`**; GX2 draws/frame: **`1148 → 648`**.
  - **Novalis (`RAC1_LEVEL=1`, 14 windows):**
    - **GPU busy average: `38.2 → 24.1 ms` (-14.1 ms / -36.9% GPU time across all windows)**.
    - **Heaviest final gameplay window (window 14, frame 1140–1200):**
      - **Host frame ms: `54.9 → 36.1 ms` (-18.8 ms faster than `m1l`, 2.43× faster than `87.8 ms` baseline)**!
      - **GPU busy ms: `38.9 → 23.9 ms` (-15.0 ms)**, with **`GPU wait = 0.0 ms`** (was `15.7 ms`) and **`idle between frames = 12.6 ms`**.
      - **World GPU pass: `26.98 → 10.06 ms` (2.68× faster)**: `tfrag pass 1` `1.50 ms`, `tfrag pass 2` `0.03 ms`, `ties/shrubs pass 1` `5.52 ms`, `ties/shrubs pass 2` `3.01 ms`.
      - **CPU `world draws`: `6.1 → 3.6 ms`**; GX2 draws/frame: **`1710 → 1013`**.

### M1n–M1o: `gs::LocalMemory` CLUT pre-decode & `TexSnap` 16-entry ring, zero-copy `mrp_chain` `Reader::read`, ring-wrap `GX2DrawDone` elimination (`RAC1_WIIU_RING_NOWAIT`), and FTP-free `bench.sh` (2026-10-30)

- **Why `chain join` on Core 1 was still 4–13 ms and `g_fxtex` (`mrp_world_gx2.cpp:2482`) stalled with `GX2DrawDone`:**
  1. `LocalMemory::addr16`, `addr8`, and `addr4` in `runtime/gs/gs_memory.cpp` computed PS2 GS page/block/column swizzling with bit-shifts and switches on every texel access instead of using `static const` `in_page` lookup tables like `addr32`.
  2. `LocalMemory::store` called `write_pixel_raw` for `T8`, `T4`, and `CT16`/`CT16S`, which recomputed `addr*` a second time for every byte/halfword written and used `memcmp`/`memcpy` on 1–2 byte values.
  3. `LocalMemory::texture_at_kick` called `read_pixel(CT32, cbp, 1, cx, cy)` inside the inner `(y, x)` texel loop for every single texel (`w * h` times) instead of pre-decoding the 256-entry (`T8`) or 16-entry (`T4`) CLUT once per texture!
  4. `LocalMemory::snap_` (`TexSnap`) stored only a single `std::shared_ptr<const TexImage> img` per `(tex0, texa)` key. Because the game reuses the same GS VRAM `TBP` slot for multiple effect/HUD textures within a single frame (`store()` increments `writes_`), `texture_at_kick` allocated a brand-new `TexImage` with a new `TexImage::id` every time, causing `mrp_world_gx2.cpp`'s 8 MB effect texture cache (`g_fxtex`) to overflow every few frames and call `GX2DrawDone()` (`mrp_world_gx2.cpp:2482`).
  5. `Reader::read` in `src/game/render/mrp_chain.cpp` copied `t + 8` and `d` (`qwc * 16` bytes) into `buf_` on every single DMA tag even when `d == t + 16` is already contiguous inside `g_capture.bytes`.
- **`runtime/gs/gs_memory.h`, `runtime/gs/gs_memory.cpp`, `src/game/render/mrp_chain.cpp` (`m1n`):**
  - Added `static const` `in_page` lookup tables to `addr16`, `addr8`, and `addr4`, and inlined direct byte/halfword stores in `LocalMemory::store`.
  - Pre-decoded the 256-entry (`T8`) or 16-entry (`T4`) CLUT once per `texture_at_kick` call and specialized the inner texel loops by `psm`.
  - Added a 16-entry variant ring (`TexSnap::ring[16]` + `ring_hash[16]`) to `TexSnap` keyed by FNV-1a pixel hash so alternating textures sharing a `(tex0, texa)` slot reuse their existing `std::shared_ptr<const TexImage>` (and stable `TexImage::id`) across frames without reallocating or overflowing `g_fxtex`.
  - Passed `t` directly to `vif_stream` in `Reader::read` when `d == nullptr || d == t + 16`.
- **Console benchmark (`m1n_novalis`, `RAC1_LEVEL=1 RAC1_FRAMES=1500`):**
  - **`chain join` dropped by 60–100% across the entire Novalis run** (compare `m1l_novalis` vs `m1n_novalis` at identical game frames):
    - Frame 180: `chain join` **`12.1 → 4.6 ms` (-7.5 ms)**
    - Frame 300–360: `chain join` **`13.0 → 6.4 ms` (-6.6 ms)**
    - Frames 420–720 (5 steady-state 30 fps windows): `chain join` **`5.3–7.7 ms → 2.2–3.0 ms`** (nearly 3× faster), with all 5 windows locked at **32.5–33.4 ms (30.0 fps)** and `begin (swap wait)` headroom rising to **12.4–14.5 ms**!
    - Frames 840–1500 (heavy Novalis canyon vistas): `chain join` dropped from **`3.9–7.1 ms` to `0.0–0.1 ms`** (`mrp_chain` on Core 0 now finishes *before* `world's worker join` on Core 1 finishes, immediately freeing Core 0 to drain the tfrag queue in parallel!).
    - Frame 960 host frame: **`49.3 → 44.7 ms` (-4.6 ms)**; Frame 1080 host frame: **`47.5 → 41.9 ms` (-5.6 ms)**!
  - **`mrp_world_gx2.cpp:2482` (`g_fxtex` 8 MB `GX2DrawDone`) dropped to `0.00/frame` across all gameplay windows!**
- **Eliminating mid-frame ring-wrap `GX2DrawDone()` stalls (`RAC1_WIIU_RING_NOWAIT`, default 1, `mrp_world_gx2.cpp`, `mrp_moby_gx2.cpp`, `mrp_2d_gx2.cpp`) and FTP-free `bench.sh` (`m1o`):**
  - `[gx2]` counters in `m1n_novalis` showed `DrawDone sites: mrp_world_gx2.cpp:303 0.10, mrp_moby_gx2.cpp:126 0.03, mrp_world_gx2.cpp:285 0.03, mrp_2d_gx2.cpp:119 0.03` (up to `0.20` `GX2DrawDone()` calls/frame = 1 full GPU pipeline stall every 5 frames when ring buffers wrapped!).
  - Because `wiiu_gfx_frame()` already waits for the previous frame's GPU timestamp (`GX2WaitTimeStamp(g_frame_ts)`) before any draw call of the current frame, data at offset `0` of an 8–16 MB ring buffer is from 10–50 frames ago and is guaranteed to be retired on the GPU unless a *single frame* allocates more than the ring size.
  - Raised `kGeoRingBytes` from 8 MB to 16 MB and added per-frame byte tracking (`g_ring_frame_bytes`, `g_pring_frame_bytes`, `g_gring_frame_bytes`, `g_sring_frame_bytes`) so ring wraps only call `GX2DrawDone()` if a single frame's allocations exceed half the ring size.
  - Added a 1 KB patchable `g_embedded_cfg` slot in `wiiu_main.cpp` and updated `ports/wiiu/tools/bench.sh` + `wiiu-run.sh` to embed benchmark settings directly into `$out/$tag.elf` -> `$out/$tag.rpx` via `elf2rpl`, eliminating all FTP connections and SD `run.cfg` touches during benchmarks.
- **Fixing `*_fast_noat.gsh` VS/PS SPI varying semantic shift (`mrp_world_fast.glsl`, `gx2_fix_noat_pair` in `mrp_world_gx2.cpp`, `m1p`):**
  - When `m1m` (`RAC1_WIIU_P2_CULL`) began binding `mrp_world_fast_noat.gsh` and `mrp_tfrag_fast_noat.gsh` for opaque draws (`pass2 == false`) in Pass 1, terrain (`tfrag`) and rocks (`ties`) rendered with corrupted UVs and colours.
  - **Root cause:** CafeGLSL (`glslcompiler`) assigns hardware SPI semantic IDs (`0x8a..0x92`) in alphabetical order of active varyings independently in each shader stage (`vAt`, `vColor`, `vFlat`, `vFnA`, `vFnB`, `vLod`, `vQF`, `vSel`, `vStq`). `mrp_world_fast.frag` wrapped `flat in vec4 vAt;` in `#ifndef RAC1_NO_ALPHA_TEST`, whereas `mrp_world_fast.glsl` exported `flat out vec4 vAt;` unconditionally — shifting all 8 remaining pixel-shader inputs (`vColor..vStq`) down by 1 SPI slot (`0x8a..0x91` instead of `0x8b..0x92`)!
  - **Fix:** Wrapped `vAt` in `#ifndef RAC1_NO_ALPHA_TEST` in `ports/wiiu/shaders/mrp_world_fast.glsl` AND added `gx2_fix_noat_pair()` in `ports/wiiu/game/mrp_world_gx2.cpp` so even pre-existing `*_fast_noat.gsh` files on the SD card have their `spi_ps_input_cntls[0..7]` aligned in RAM at load time without requiring FTP uploads.
- **Console benchmark (`m1p`, verified with intact rendering on both `0.75` scale and `1.0` Native 1080p):**
  - **Veldin (`m1p_veldin`, `0.75` scale, 20 windows):**
    - **Host frame average: `29.3 ms` (~34.1 fps average; down from `52.6 ms` baseline, -44.3%)**, steady-state final window **`22.9 ms` (`43.7 fps` equivalent)**!
    - **GPU busy average: `17.8 ms`** (final window **`15.4 ms`**); **`GX2DrawDone = 0.0`** across all gameplay windows!
  - **Veldin at Native 1080p (`m1p_1080p_veldin`, `RAC1_WIIU_3D_SCALE=1.0`, full `1920x1080`, 17 windows):**
    - **Host frame average: `35.1 ms`**, steady-state final window **`33.3 ms` (`30.0 fps`)**!
    - **GPU busy average: `22.9 ms`** (final window **`20.8 ms`**, `GPU wait 5.2 ms` + `swap wait 12.2 ms` of headroom); **`GX2DrawDone = 0.0`**.
  - **Novalis (`m1p_novalis`, `0.75` scale, 14 windows):**
    - **Host frame average: `42.9 ms` (down from `87.8 ms` baseline, -51.1% / 2.05× faster)**, final window **`38.2 ms`**!
    - **GPU busy average: `23.9 ms`** (final window **`23.7 ms`**); **`GX2DrawDone = 0.0`** across all gameplay windows!
  - **Novalis at Native 1080p (`m1p_1080p_novalis`, `RAC1_WIIU_3D_SCALE=1.0`, full `1920x1080`, 14 windows):**
    - **Host frame average: `43.4 ms`** (within `0.5 ms` of `0.75` scale!), final window **`37.1 ms`**!
    - **GPU busy average: `29.9 ms`** (final window **`30.8 ms`**, **below the 33.3 ms 30 fps budget even at full 1920×1080 on Novalis!**): `tfrag pass 1 = 2.46 ms`, `tfrag pass 2 = 0.04 ms`, `ties/shrubs pass 1 = 7.34 ms`, `ties/shrubs pass 2 = 3.85 ms`, `3D upscale = 0.00 ms`.
    - Proves that even at **full Native 1080p (`1920x1080`)**, the Latte GPU is no longer the bottleneck on either Veldin (`22.8 ms`) or Novalis (`29.9 ms`); the remaining frame time on Novalis is 100% CPU-bound on Core 2 (`game_thread`, `31.5 ms`).


### The tfrags' programs in the vertex shader, the late sky (2026-10-30, evening)

- **RAC1_WIIU_TFRAG_VS (on by default; =0 the CPU route as before): the tfrags made by their vertex shader** (src/game/render/
  mrp_tfrag_gpu_vs.inc, ports/wiiu/shaders/mrp_tfrag_vs.vert, shaders/mrp_tfrag_vs_fast.gsh + _noat). The CPU route (tfrag_exec_cpu_fast
  + tfrag_draw_cpu_one: ~3,000-5,000 72-byte vertices a frame, 3-10 ms on cores 1 and 0 on Novalis, part of it waited for in "world's
  worker join", the rest in "world draws" - mrp_world_wiiu_tfrag_join) is gone from the frames that take it:
  - every plan of Veldin (3220) and Novalis (7028) has no serial step and writes each slot once (tfrag_level_build: "steps run in order
    0"), so a slot's final value is a closed expression: a record X(q) / M(q, P1, P2, band) (two morph levels at most) or raw, a vertex
    info the image's or a selection (MS under F: three conditions at most). tfrag_vs_build resolves them once a level into tables (one
    RGBA32UI texel each: per plan its header, the sent vertices four a texel, the slots, the leaves, the records; Veldin 441 K texels /
    6.9 MB, Novalis 982 K / 15.3 MB - their host copy dropped once uploaded; the images 4.5 / 10 MB as a texture beside the CPU's copy):
    "[mrpw tfrag] the vertex shader's tables: N plans, 0 refused";
  - per frame the CPU writes only a block (TfragVs, 64 KB at most: per draw 3 texels, per view set 17 - the VU view, the clip matrix =
    the world renderers' view-projection x the draw's world view (doubles), the program's constants -, per run 1) and one u32 per
    triangle corner (draw << 16 | closing vertex << 2 | corner: 36 KB for 3,000 triangles), in the frame's triangle slots' order, so the
    tfrag ranges' first / count are the corners'. One GX2DrawEx per range as before (its first corner as the draw's offset);
  - the vertex shader evaluates the corner's vertex as the CPU route does (the selections, the records, STQ, the GS's Q, F, RGBA, the
    run), and a triangle whose closing vertex has ADC (form 3: the three vertices' guard band flags) is collapsed (its three corners
    one point outside the view). ~1500 ALU groups in the program (fully inlined), a corner's path a fraction of it, 27 GPRs; ~9,000
    corners a frame on Novalis;
  - the lit colours: an R32UI texture, the dirty ranges each frame (wiiu_prep_tail keeps g_tcols' changes for it);
  - checked on the PC (RAC1_MRPW_TFRAG_CPU=1 RAC1_MRPW_TFRAG_VS_CHECK=1: the shader's C++ twin tfrag_vs_eval_cpu against the CPU route
    every frame; =2 also ports/wiiu/shaders/mrp_tfrag_vs.vert itself compiled by the PC's GL, run with transform feedback): Veldin 700
    frames (2.7 M corners) and Novalis with RAC1_MRP_LOD=1 600 frames (2.6 M corners, forms 0 / 3 / 4 all drawn): 0 plans refused, ADC
    decisions identical, STQ / RGBA / F / runs exact against the CPU route; positions within float noise (largest relative differences
    at vertices behind the camera, w ~ 0.1); the GLSL against the twin: the colours +-1 where C x C4 falls on an integer (the GPU's
    rounding), the GS's Q / STQ at a few hundred corners behind the camera within 1e-2 relative. The decisions can differ from the
    route's where a w lies within a few ulps of a band's threshold (the GPU rounds to nearest, the route toward zero): a morph's
    substitution a frame earlier or later.
  - expected on the console: Novalis "world's worker join" minus the tfrag drain's share and "world draws" minus
    mrp_world_wiiu_tfrag_join (together ~5-10 ms in the heavy windows), core 0 free after the chain (the drain's worker idle); the GPU's
    tfrag pass 1 plus the vertex work (~0.5-1 ms estimated). The log: "[wiiu] mrp_world_gx2: tfrag vertex shader, N frames: corners, KB
    of block, ms sending" every 120 calls; RAC1_WIIU_PREP_TIME=1 adds "[mrpw] the prepare's second half" (build_triangles, the tfrags).
  - with the prepare's tfrag drain gone, core 0 idles after the chain's read: RAC1_WIIU_PREP_ON_MAIN=0 (the whole prepare on core 0's
    worker, core 1 doing the effects' prepare and the 2D meanwhile) is worth measuring again.
- **RAC1_WIIU_SKY_LATE (on by default): the sky after the terrain's first pass.** The sky's shells (5 on Veldin / Novalis) cover the
  screen and were drawn first without a depth test: every pixel the terrain then covers was shaded and blended five times (the sky's
  GPU pass 3.8 ms on Veldin, 5.4 on Novalis at 1080p - its pixel shader's ALU, ~10-16 groups a pixel at ~32 a clock). Now
  mrp_world_gx2_draw_sky keeps its arguments and mrp_world_gx2_draw draws the sky before its first batch that is not a leading tfrag
  batch whose first pass writes the source alone (every range TfragRange::opaque - the texture's sampled levels and the vertices'
  alphas >= 0x80, draw_opaque -, blended as ALPHA_1 0x44 or not at all, the depth tested LEQUAL / LESS and written), and before such a
  batch's second pass; with the depth test on (LEQUAL, no writes) and the viewport's depth range [1, 1]: its pixels at the cleared depth
  pass only where nothing was drawn, hierarchical Z drops the covered tiles. The same picture by construction (those pixels hide what
  lies behind them; the shells blend among themselves in their order; the tfrags' second pass and the rest after the sky as before).
  The "[gpu]" line's "sky" pass is then ~0 (the sky's time inside "world"; "[gpu-world]" still sums it as the sky's). The log: "[wiiu]
  mrp_world_gx2: late sky, last 120 calls: N after the tfrags' first pass, M first".
- To measure on the console (bench.sh, RAC1_WIIU_GPU_TIME=2): the default against RAC1_WIIU_TFRAG_VS=0 and against RAC1_WIIU_SKY_LATE=0;
  the "[timing]" world's worker join / world draws, "[gpu-world]" sky / tfrag pass 1, "[gpu]" busy.

### The game thread's moby pass: what it computes, the draw loop's ops without flags, the drop shadows timed apart (2026-10-30)

- The map (Novalis 1080p, n1080: "Game: moby pass (draw loop)" 12.7 ms a host frame, the drawn game frame's only - RAC1_DRAW_LAST_STEP).
  The zone is mrp_lod_moby_widen_real(moby_pass): (1) widen_real: the VU0 saved, patch_widen through the EE journal (projection_setup /
  render_view_setup re-run on the widened tangent, the A rows put back; 1-2 pages diffed), patch_moby_logged only with the LOD slider; (2)
  moby_pass_start, spr_moby_fill: the chain's head, the VU0 programs' upload, the scratchpad's state; (3) moby_draw_loop over every live moby
  of the level's table (0x100 bytes each): the culling - draw distance (+0x32 against 0x15fff0), the zone masks (+0x37, 0x173d00), +0x34 &
  0x81, the sphere against the view rows and the guard band (MSCAL 0xa / 0xe) - writing the drawn flag +0x31 (GAME STATE: the glows, the
  updates read it), the per-class near / far counters (scratchpad 0x2600) and the texture groups' nearest depths (0x3a00 -> moby_textures:
  the mips and VRAM slots 0x19a600 / 0x199e80), the drop-shadow mobys deferred (their ground point, culling, intensity: 0x1ac600 / 0x15fffc,
  read by the drop shadows only), and per moby drawn the lighting (the light sets +0x38 / +0x39 blended by +0x3a, the point lights within
  their radius summed), the fade's colour, the model-to-view matrix, the parts (the chain's, not written with RAC1_MOBY_PASS_MRP), the
  sub-mesh records (0x1ac200: the GS path's colour fill, skipped) and the skinning record into the ring (fromSPR) - the ring is inside the
  packet buffer (sub_002a0858: the buffer's end), read by the skinning and the MRP's observer (on_draw) only; (4) moby_pass_end: the
  textures' chain, the joints (their own stage), the drop shadows (moby_shadows 0x24fc38: shadow_points - each deferred moby's pose again,
  moby_pose_matrices, and its class's shadow points into 0x1ac780 -, drop_shadows_draw - per entry the light's axes 0x160430..0x16045f, the
  tube's point pairs on the scratchpad, shadow_points_clip, shadow_quads with the VU0 program 0x190, shadow_packets per layer into the
  packet buffer, the buffer's clear / apply sprites -, the MRP's observers mrp_shadows.cpp).
  Who reads what: game logic - the drawn flags +0x31 and the culling / LOD results the update sees (the moby's own fields), the packet
  pointer 0x1611c0's positions; drawing only - the lighting, colour, matrices and parts (the record, in packet-buffer pages), the textures'
  slots, the sub-mesh records, everything the drop shadows write (0x1ac780 is the level load's uid -> spawn index map, read at the load only,
  level_load.cpp; 0x160420..0x16046c by the shadows only; the scratchpad and the VU0 registers set again by their next users).
  The old profile on the console (m1b_novalis_prof.samples.bin, before the LOD undo log; samples.py --under widen_real, core 2): the zone
  20.6% of core 2's samples, of which ~10% the journal (since removed: -8 ms), the drop shadows 6.2% (shadow_quads 2.1, shadow_segment_draw 1.6,
  shadow_points 1.2 with its poses, the observers ~0.5, the VU0 macro helpers mulbc / maddabc / maddbc / sub / fin_lanes ~2.4 out of line),
  the draw loop ~3.3% (2.3 its own code). So today's 12.7 ms is about 7 ms drop shadows, 4 ms draw loop, 1-2 ms the rest - to confirm
  with the new stage below and a sampled run.
- RAC1_WIIU_FAST_MOBY_LOOP (moby_draw.cpp, moby_draw_fast.inc; 1 the Wii U's default, 0 the PC's): the draw loop on a flag sink of its
  own (NoFlags) - the same lanes as vu0_regs.h's ops (operands and results clamped by the VU rules, masked lanes, vf0 constant, ACC written)
  without the MAC / status flags. A lane's clamp is decided on the FPU (|x| a normal number: kept; zeros, denormals, Inf / NaN: vuc's own
  integer path, a cold call), so the values are vuc's by construction. Nothing reads the flags the loop leaves (no CFC2 in it; after it the
  game never reads the status flag and every MAC read follows an op of its own - the analysis of RAC1_WIIU_FAST_POSE); the VU0's flag
  registers keep their values from before the loop. Each (op, form, mask) is one small function, called: the loop was 142 KB of code on
  the Wii U (the SSE2 shim's ops inlined: four scalar lanes, every clamp through memory - no FPR <-> GPR move on the 750 -, fin_lanes'
  flag words), 4.4 times the 750's instruction cache and run through again for every moby drawn; now 31 KB plus ~12 KB of ops.
  PC proof (the Wii U-like walk with the drop shadows ON: RAC1_NO_GRAPHICS=2 RAC1_NO_GRAPHICS_WALK=1 RAC1_MOBY_MRP=1 RAC1_WORLD_PASS_SKIP=6
  RAC1_TFRAG_PASS_OFF=1 RAC1_MOBY_PASS_MRP=1 RAC1_SKIP_SHADOWS=0 RAC1_SKIP_DRAW_LISTS=1 RAC1_DRAW_LAST_STEP=1 RAC1_VU1_LIKE_WIIU=1;
  rac1-scratch/wiiu/mobyperf/loop_check.sh): RAMHASH word for word identical =1 against =0, RAC1_GOTO=0@60,1@700 forward 1500 frames and
  Novalis explore 1200 frames (every page, every game frame).
- The drop shadows timed apart: the profiler's new game stage "Game: moby drop shadows" (kGameMobyShadows, moby_pass_end's moby_shadows),
  so "[game]" splits the moby pass. RAC1_SKIP_SHADOWS=1 (the debug menu's "Shadows") still skips them for a measure.
- samples.py --under NAME: only the samples whose call chain holds NAME (e.g. widen_real, moby_shadows): a zone's breakdown.
- To measure on the console (bench.sh; build in rac1-scratch/wiiu/mobyperf/build): Novalis 1080p with RAC1_WIIU_FAST_MOBY_LOOP=0 and =1
  ("[game]": the moby pass and the drop shadows rows); then a sampled run (RAC1_WIIU_SAMPLE=200 RAC1_WIIU_SAMPLE_AFTER=12
  RAC1_WIIU_SAMPLE_CORES=2) for samples.py --under widen_real.

### The drop shadows drawn by the GPU: the game's shadow code skipped (RAC1_WIIU_SHADOWS_GPU, 2026-10-30; to measure on the console)

- What the game draws: per deferred moby (0x1ac600, the draw loop's) the shapes of its class's shadow list (class +0x14 - lbu(+0xf) x 16:
  0x20-byte spheres on one joint, 0x30-byte capsules between two joints with a radius at each end, a negative end word moving that end along
  the segment), through its pose (moby_pose_matrices 0x2531e0 again: the joints' own matrices J), scaled by moby +0x2c / 1024 x the entry's
  word / 4096, placed by the moby's rows; each shape extruded along the shadow direction (0x1aeb80 + 16 x moby +0xbd, 1000 units) into a tube
  of ring points cut by the planes z = moby +0x88 and +0x84 (a slab of ~0.5 units around the ground: no shadow through a floor), its faces
  counted in the frame's alpha, the frame darkened by FIX 0x20 (x 0.75) where the count is positive. Ratchet: 21 capsules.
- The design: the same shapes, kept by the host (mrp_shadows_gpu.inc, mrp_shadows.cpp ds_capture: moby_shadows' new hook skips
  shadow_points / drop_shadows_draw), drawn by the GPU as shadow volumes (mrp_world_gx2.cpp mrp_world_gx2_draw_shadows): each shape's
  outline across the direction (a 12-gon around the circle; a capsule the arcs between the outer tangents of its two circles) extruded into
  a closed prism between the two planes (the original's tube), its faces split on the CPU by the side they show the camera; an RG8 mask the
  size of the 3D buffer, beside the frame's depth buffer (LEQUAL, no writes: the passes' own function, Hi-Z untouched), cleared over the
  shadows' rectangles, the faces toward the camera that pass counted in its red, the others in its green (adds alone; inside where
  red > green) - the original's z-pass sign; then a quad per rectangle over the frame: 13 point taps of the mask around the
  pixel (~2.5 px at 1080p) give the soft edge, colour x (1 - 0.25 x share), alpha kept. Between the first chain's mobys and the deferred
  ones (mrp_moby_gx2_draw's new `part`), as the game chains them: the casters are not darkened by their own shadow.
- Why not a shadow map: sampling it in the world's pixel shader adds to the pass that bounds the console (mrp_world_fast.frag, ~16 ALU
  groups), and a depth texture of the main depth buffer (D32F, no stencil on WHB's buffers) is not readable without a conversion; a
  separate decal pass with a shadow map would still need the receivers' positions (the depth). The game's own shapes are capsules and
  spheres: their volumes give the exact shape the PS2 drew, bounded by the game's slab, at the cost of a few thousand small triangles and
  the rectangles' pixels, with no change to any world shader. The direction and the slab are per caster: a baked term for the static
  world (RAC1_WIIU_SHADOWS_BAKED, Daino's follow-up: the ties' / shrubs' / tfrags' shadows once per level, in their colours or a small
  lightmap) can take the same direction table (0x1aeb80) and leave this pass for the mobys alone.
- The pose: the MRP scene's (mrp_scene.h, the joints the skinning consumed: S = B J, B the record's bind matrices +0x1c), J p = (p B^-1) S;
  a moby the scene has no pose of (not drawn - its shadow seen, the moby not -, another joint count) gets the game's moby_pose_matrices.
  RAC1_WIIU_SHADOWS_GPU_CHECK=1 (any host: the game's code runs too, the shapes compared): Veldin -> Novalis forward 1500 frames, 542
  deferred mobys (508 posed by the scene), 7964 shapes, the largest difference 0.000015 units, radii equal.
- Game state (the PC, the Wii U-like walk with the drop shadows on: rac1-scratch/wiiu/shgpu/state_check.sh): =1 against =0, Veldin ->
  Novalis forward 1500 frames and Novalis explore 1200: the hero / camera lines identical; RAMHASH differs only in the overlay's globals
  0x15f000-0x161fff and 0x1ac000-0x1adfff, the two packet buffers (0x2f4000 / 0x4b4000, 0x1c0000 each: the packets after the shadows' slot
  sit lower, the skinning ring's records hold their addresses) and the scratchpad. Word by word at game frame 400 (RAC1_RAMDUMP): outside
  the packet buffers only 0x15f410-0x15f417 (the texture block's tags, packet addresses), 0x160430-0x16046f (the shadows' axes and ring
  counts), 0x1611c0-0x1611c7 (the packet pointer and its last value), 0x1ac780.. (the shadow entries), the scratchpad 0x800-0x381b (the
  tubes' points and the faces' bytes) - none read by the game's logic.
- The PC's check of the shapes (RAC1_WIIU_SHADOWS_GPU=1 with graphics: the prisms' faces as mrp_shadows' stencil counter's, the original's
  darkening): Veldin forward game frame 300, Ratchet's shadow (body, legs, wrench) as the game's to the pixel but for the ring's polygon
  (rac1-scratch/wiiu/shgpu/crop300_g0.png / _g1.png).
- CPU (the PC; RAC1_WIIU_SHADOWS_GPU_TIME=1, any host: moby_shadows' start to its end every 120 frames that drew shadows): Veldin -> Novalis
  forward, the game's code 0.12-0.22 ms a frame, the capture 0.002-0.006 ms (x 40-60). On the console the profiler's "Game: moby drop
  shadows" stage was ~4.4 ms a host frame on Novalis: expected ~0.1-0.2 ms. The prisms' making on the render thread: 0.02-0.03 ms on the PC
  for 21-37 prisms, 900-1500 faces (Espresso: ~0.2-0.4 ms on core 1, plus ~70 KB of vertices written).
- GPU (estimate, to measure): ~1-1.5 K triangles into the R8 mask, depth-tested (thin slabs: tens of K pixels); the rectangles' clear and
  darkening (Ratchet near: ~300 x 200 px at 0.75 scale, 13 taps of an 8-bit texture); a colour / texture cache flush of the mask. Expected
  0.1-0.3 ms; the old faces' pass (draw_fx, the game's faces drawn with blending and no counter: the glitches) goes. The "[gpu]" line has
  "drop shadows" (the pass) and "mobys deferred" (the mobys after it).
- Switches: RAC1_WIIU_SHADOWS_GPU (1 the Wii U's default, 0 the PC's; 0 the game's code and draw_fx's faces as before; the debug menu's
  Picture row "Drop shadows": GPU / game's), _RING (12), _DARK (25 %), _SOFT (2.5 px at 1080p, 0 hard), _DEBUG=1 (red shadows at 60 %), 2 (the rectangles darkened whole: the darkening and the mask's reading),
  3 (the prisms' front faces white where they pass the depth test: the geometry and the depth), 4 (the mask's raw counts: red fronts / 8,
  green backs / 8, blue net / 4), _LOG
  (the captured casters), _CHECK, _TIME; the log line "[wiiu] mrp_world_gx2: drop shadows on the GPU" every 300 calls (casters, prisms, faces,
  rectangles, pixels, CPU ms). Without the new shaders on the SD (shaders/mrp_shadow_vol.gsh, mrp_shadow_dark.gsh) the game's code comes back.
- To measure on the console: bench.sh / bench_veldin_open.sh with BUILD=build-wiiu-sh and RAC1_WIIU_SHADOWS_GPU=1 against =0 ("[game]"
  drop shadows row, "[gpu]" drop shadows / mobys deferred / effects, "[timing]" mobys); shots RAC1_WIIU_SHOT=300 (Veldin forward: Ratchet
  near the start) and the open-area bench's 1200 / 1800 for the look.
- The console's first picture (2026-10-30): no shadow under Ratchet in the normal mode (Veldin's open area, game frame 1800), the red shapes
  there with RAC1_WIIU_SHADOWS_GPU_DEBUG=1; Novalis' game code 34-37 -> 27.6-29.5 ms a host frame, the "[gpu]" drop shadows 0.37-0.55 ms.
  The suspect found by reading: the counter's step 1 / 255 (an R8 mask, the backs taken away by a reverse subtract). The pixel shader's
  1 / 255 is 0.99998 counts: a colour buffer that truncates the blend's result stores 0 for one face (only pixels under two or more
  overlapping fronts counted - Ratchet's 21 capsules overlap: a partial shape, shown at 60 % in red, its 25 % under the 13 taps' average
  hardly seen) and a subtraction then takes one count too many. Now an RG8 mask: the fronts counted in red, the backs in green, adds alone,
  1.25 / 255 a face (one count rounded or truncated), inside where red > green (shaders/mrp_shadow_dark.gsh changed; mrp_shadow_vol.gsh the
  same bytes). The debug modes 2-4 above tell the passes apart if it is not that.

### The cores' timeline; what "world's worker join" is; the world's job split between cores 1 and 0 (2026-10-30, night; to measure)

- **What the join is, read off the code.** With RAC1_WIIU_PREP_ON_MAIN=1 (the default since m1j) mrp_world_wiiu_prepare only notes the
  job (wiiu_prep_dispatch: g_prep_on_main_pending) and the first mrp_world_wiiu_join of wiiu_gfx_frame *runs* it on the main thread, core 1:
  wiiu_prep_head (static_frame_build_at, tfrag_decide_at, scene_from_static_k with the sky's packets) and wiiu_prep_tail (build_triangles,
  the tfrags' vertex-shader block). It waits for nothing: no wait for the chain's reader is left in it (the sky's VU1 run is at the
  prepare, mrp_world_wiiu_sky_capture; the GS registers are the frame before's, g_regs_last; the tfrags' CPU-route drain is gone with
  RAC1_WIIU_TFRAG_VS). The worker on core 0 is not used at all. n1080_tvs (RAC1_WIIU_PREP_TIME=1, Novalis 1080p) over its last 120 calls:
  the first half 2.46 + 1.14 + 4.18 = 7.8 ms, the second 6.02 + 0.61 = 6.6 ms - 14.4 ms, the four "[timing]" windows' "world's worker join"
  averaging the same (6.8, 13.5, 19.7, 20.0): the heavy windows' 20 ms are ~10 + ~9 of the job itself, on the main thread, in series with the
  rest of the drawing. ("[mrpw] the prepare on the worker" is then printed by the main thread; its "the scene" is 0 and "the sky's run" is
  scene_from_static_k - the labels are one part off.) Core 0 idles once the chain is read ("chain join" 0.0-0.4: the reader done long before).
- **The critical paths.** Main thread (Novalis' heavy windows, ~50 ms): the prepare ~7.3 (chain start 1.5, mobys' 3.6, world's 2.1) + the job
  ~20 + effects' prepare / 2D build ~5.3 + swap wait ~3.3 + draws ~13. Game thread: the prepare (it waits for it: the update reads after it)
  + its steps (30-40 ms) ~ 44 ms. Taking the job's second half off core 1 brings the main thread to ~40-42, under the game thread's ~44:
  the host frame is then bound by the steps + the serial prepare (expected ~44-46 instead of ~50 in Novalis' heavy windows; on Veldin's open
  view the GPU and the swap waits are already ~15-25 ms of the frame - little to gain there, the GPU bound).
- **The timers ("[core0]" lines, src/platform/core_timeline.h; the timing on - RAC1_WIIU_TIMING=1 or the debug menu, so every bench).**
  OSGetTime marks (an atomic store each, no lock, no allocation) by the main thread, the game thread, the chain's reader and the world's job;
  a cycle runs from a prepare's start (orig_main.cpp, after the frame before's mrp_chain_join: rac1_wiiu_cycle) to the next, its marks taken
  as offsets from its start, averaged over 30 cycles (a count in brackets when not every cycle had the mark; "-" none). Three lines:
  - `[core0] 30 cycles, 50.1 ms each, ms from the prepare's start: main: chain kicked A, prepared B, drawing C, world join D->E, chain join
    F->G, fx/2D built H, GPU wait done I, swap wait done J, draws' world join K->L, TV drawn M, frame done N, game thread join O->P |
    game thread Q->R | chain reader S->T | world job U->V: first half ->W, the chain waited X->Y, second half Z->V`
  - `[core0] the world job at the drawing's join: not started 30, ... at the draws' join: done 30, ...; the chain still reading at the
    drawing's join n, the draws' n, the chain join n; its first half on cores 0/1/2 a/b/c, its second a/b/c` - what the job was doing when
    the main thread reached each join (not started, in its first half, between its halves, waiting for the chain, in its second half, done).
  - `[core0] the chain reader, ms a cycle: the walk (VIF, GIF, primitives) a, VU1 programs in the effects' ranges b (n runs), elsewhere c
    (n runs), their kicks d, textures e, GS uploads f; programs by time (start, in a range, ms, runs a cycle): 0x..r 1.23/40.0, ...` - the
    reader's time by kind, exclusive (mrp_chain.cpp: the program runs - their kicks apart -, texture_at_kick / picture_ct32, the IMAGE
    transfers into the GS memory, the rest the walk), and its ten heaviest programs (inclusive, "r": run inside an effects' / program 7's
    range, its kicks kept; without it: run for the VU1 / GS state alone, its kicks dropped).
  The PC: every call is an empty inline (core_timeline.h's #else), its behaviour unchanged.
- **RAC1_WIIU_PREP_SPLIT=1 (off by default; with RAC1_WIIU_PREP_ON_MAIN=1):** the first half on the main thread at the drawing's start as
  before (core 0 is reading the chain then), the second half handed to the worker on core 0, which waits for the chain's reader to be idle
  (mrp_chain_wait_idle: the "chain waited" span) and runs it beside the main thread's chain join, effects' prepare, 2D build, GPU and swap
  waits; draw_frame's mrp_world_wiiu_join waits for it, or runs it itself if the worker has not taken it yet (an atomic claim: whichever
  thread gets it). The tfrags' CPU route (RAC1_WIIU_TFRAG_VS=0) runs whole where the second half runs (the cooperative drain's slots are the
  main thread's and the worker's). Expected (Novalis heavy): "world's worker join" ~20 -> ~10 (the first half), its rest moved to the draws'
  join only when the worker is late; host frame ~50 -> ~44-46.
- **RAC1_WIIU_WJOIN_LATE=1 (off; only with RAC1_WIIU_PREP_ON_MAIN=0):** the whole job on core 0's worker as before m1j, but not waited for at
  the drawing's start: the main thread joins the chain, prepares the effects and the 2D, waits for the GPU and the swap first, and joins the
  world before its draws. With the chain and the job in series on core 0 this wins only if the chain is short - the timeline says.
- **To measure** (BUILD=build-wiiu-c0): the default (the timeline alone, RAC1_WIIU_PREP_TIME=1 for the halves' parts), RAC1_WIIU_PREP_SPLIT=1,
  and RAC1_WIIU_PREP_ON_MAIN=0 RAC1_WIIU_WJOIN_LATE=1, on Novalis and on Veldin's open view; then decide the default from "[timing]" and
  "[core0]". The chain reader's line says whether the reader's own time is worth cutting (the programs run outside the ranges only for
  their state - the comment at Reader::mscal - are the candidates; not skipped here: the in-range programs read the VU1 state they leave).
- **Proposals (not applied).** (a) The game thread waits for the whole prepare (~7.3 ms, the game's EE memory read for the drawing):
  the mobys' prepare (build_frame, 3.6 ms) on core 2 beside the main thread's world prepare and chain start, the steps started there once
  the main thread's reads are done (all three only read EE memory; the chain's reader reads mrp_moby_active() meanwhile - to check), or the chain's capture copy (1.5 ms) on core 0 with a
  "capture done" join before run_async: up to ~3-5 ms off the game thread's path, which bounds the frame once the split is in. No EE write,
  but they change who reads what when - to decide. (b) The first half is splittable too: static_frame_build_at and tfrag_decide_at are
  independent (2.5 + 1.1 ms): the tfrags' decisions on core 0 when the chain is done early. (c) build_triangles by packet ranges on two
  cores (6 ms on Novalis' heavy windows) - more invasive (its batches' order).

### The chain reader on core 0: the ports' lookups, the VU1 programs' lane rules on the FPU, program 7's batches from their input (2026-10-31; to measure)

- **What the effects' VU1 programs feed on the Wii U, read off the code.** c0_n_base (Novalis 1080p): the reader ~17.8 ms of a ~37 ms
  cycle, the main thread's chain join waiting for it; programs 0xe 5.87 ms / 35 runs, 0xc 3.17 / 8, 0x0 1.70 / 7, 0x14 0.66 / 2, their
  kicks 4.05, walk 3.84, GS uploads 2.31, textures 1.44.
  - 0xc / 0xe are program 7 (shadows_vu1.cpp: a strip; 0xe clips each triangle crossing the guard band into a fan); 0x0 is the start-up
    of program 7 or of the particle program (twelve / sixteen LQs - nothing to time); 0x14 the particle program's chunk.
  - The particles' kicks are the effects themselves: mrp_fx_wiiu_prepare takes each kicked vertex back to the world through the
    producer's projection (kMrpFxParticles); the 2D's build leaves them out ("fx" in the 2D's line). Their output is needed.
  - Program 7 on the Wii U: orig_main.cpp's Wii U walk gave the vu7 ranges (mrp_fx_chain_ranges) but never vu7_hash / vu7_kind, so
    Reader::before never kept a batch: Novalis' water ran whole, its kicks made into primitives (fx = the prim lists' kind, Novalis'
    callbacks' MrpFxScope), a texture looked up per triangle, and mrp_fx_wiiu_prepare unprojected every corner - while its own program-7
    path (the batches drawn from their input, as the PC's build_vu7) was there, never reached. The other levels' seas (no prim-list
    scope) went to the 2D as projected 3D. What the PC needs from a kept batch's run: its strip's ADC flags (Vu7Batch::kicked) and the
    GS state its kicks leave.
  - The 0x0 runs at 0.24 ms each (and 0xc at 0.4) were not the programs: vu1.cpp's start_program looks a port up by hashing every
    registered port's range of the micro memory whenever the memo is stale (any MPG makes it stale) - fnv1a64 a byte at a time, 64-bit
    multiplies on a 32-bit CPU, 69 KB for each of program 7's starts (particles, mobys, the six world programs registered before it).
    The PC measures ~467 KB hashed a frame on Novalis (all threads).
- **What changed** (each behind its switch; the PC's defaults unchanged):
  - RAC1_VU1_LOOKUP (vu1.cpp; Wii U default 1, PC 0): each port keeps the last four micro images of its range it hashed and their
    answer; an image met again is compared (memcmp), not hashed - the same answer by construction. The memo's 64-bit generation read
    once a start (a library call on the 32-bit CPU, read 32 times before). Counts: vu1::lookup_stats().
  - RAC1_VU1_FAST (particles_vu1.cpp, shadows_vu1.cpp; Wii U default 1, PC 0): the two programs' instructions moved into
    particles_vu1_body.inc / shadows_vu1_body.inc and compiled twice, `exact` (the port as it was) and `fast`: vu1_fast.inc decides the
    VU rules on the FPU (a normal finite value kept, zeros / denormals / infinities / NaNs through vuc itself; FTOI, CLIP and MAX / MINI
    by float compares where the bits order as the values, the rest the exact helpers) - no float -> integer trip through memory per
    operand and result (three a lane op before). =2: both run from the same state, everything compared.
  - RAC1_WIIU_SEAS_BATCH (orig_main.cpp; default 1): the Wii U walk sets vu7_hash / vu7_kind as the PC does: program 7's batches in the
    draw lists' ranges kept as the program finds them and drawn by mrp_fx_wiiu_prepare's program-7 path; their kicks no longer made
    into primitives. A rendering change on the console (the PC's way): Novalis' water from its input instead of its unprojected kicks,
    the other levels' seas (Rilgar, Pokitaru, Gaspar's lava, Batalia, Aridia's sand) by mrp_fx with depth instead of the 2D - to look at.
  - RAC1_MRP_VU7_KICK (mrp_chain.cpp Reader::vu7_kick; default 1 everywhere): a kept batch's strip packet (one PACKED tag ST, RGBAQ,
    XYZF2, EOP) read without making its vertices: every vertex's ADC flag straight from its XYZF2 word, then only its last vertices
    through gif() (the registers, the current vertex and the primitive queue they leave) - the state gif() leaves over the whole packet.
  - The primitives without an allocation each (mrp_chain.cpp new_prim, RAC1_MRP_CHAIN_POOL=0 off): frame.prims' elements made again
    from one reading to the next (their vertex vectors keep their memory), the extras kept aside.
  - texture_at_kick's last answer (gs_memory.cpp, RAC1_GS_TEX_MEMO=0 off): the same key at the same memory generation returns the same
    image without the map's search (what the lookup's `s.gen == gen_` branch returns).
  - The "[core0]" chain reader line ends with "| a cycle: ports' lookups N (ranges hashed N, KB; images compared N), textures from the
    memo N, program 7's kicks without their vertices N (vertices); RAC1_VU1_FAST .., RAC1_VU1_LOOKUP ..".
- **Exactness (the PC, headless MRP: its reader runs the same code with the effects' and program 7's ranges, batches kept):**
  - RAC1_MRP_CHAIN_HASH=<file> (testing): a line per reading, a hash of everything it made (primitives with state, vertices field by
    field, textures' ids and sizes, mips, marks; program 7's batches with their kicked flags and vertices; the GS registers at its end
    and at regs_tag). Every switch above off against every one on: identical on Novalis explore 1500 frames (1479 readings compared,
    RAMHASH identical too), Novalis GOTO=0@60,1@700 forward 1500, Veldin GOTO=0@60 forward 1500, Veldin explore 1200, Kerwan GOTO=3@60
    forward 1500 (1140-1440 readings each).
  - RAC1_VU1_FAST=2 (the fast rules against the exact port, every register, Q / I, the flags, XTOP, the data memory, every kicked
    packet): 0 starts differ on all the runs above (program 7: 60,000+ starts on Novalis explore, 4.5 M qwords kicked; particles 5,000+).
  - RAC1_VU1_PORT_CHECK=1 (new, vu1.cpp: the port against the VU interpreter running the uploaded microcode, kicks and data memory,
    the fast port on): 70,000 starts on Novalis, 0 differ but 11 program 7 0xe starts in the first frame after the boot (also with the
    exact port, RAC1_VU1_FAST=0: the registers' state before any start-up; not from this change).
  - RAC1_MRP_FX_WIIU_CHECK=1 (new, mrp_fx.cpp: mrp_fx_wiiu_prepare's program-7 vertices against build_vu7's, same frame): the same
    triangles in the same order, world points, S / T / Q and colours exact, fog within 1 - Novalis 1500 frames (2.3 M vertices) and
    levels 0, 2-18 600 frames each.
- **Cost on the PC** (Novalis explore, the reading's wall time a frame, RAC1_MRP_CHAIN_HASH's lines): 1.12 ms -> 0.64 ms with all on;
  the lookups alone ~-0.6 to -0.8 (x86's 64-bit multiply is cheap: on the console far more); the fast rules ~0 on x86 (its float/int
  moves are cheap); the texture memo and the kicks' path -0.1..-0.3 each (noisy).
- **Expected on the console** (c0_n_base's cycle, ~17.8 ms): the lookups -3..-5 ms (the 0x0 runs ~0.1 ms, 0xc ~0.5); program 7 not
  made into primitives + the kicks' path -3..-4 (kicks 4.05 -> <1, textures 1.44 -> ~0.4); the fast rules on what is left of 0xe / 0xc
  / 0x14 -1.5..-3; the pool ~-0.3. The reader ~18 -> ~7-9 ms, under the world job's first half: the chain join should drop to ~0.
  The main thread's effects' prepare (3.2-3.5 ms) should shrink too (a batch's vertices once instead of every triangle's corners
  unprojected in doubles).
- **To measure** (BUILD=build-wiiu-ch): the defaults; RAC1_WIIU_SEAS_BATCH=0 (the water as before, the rest on); RAC1_VU1_FAST=0
  RAC1_VU1_LOOKUP=0 (the reader's VU1 as before). And to look at: Novalis' water, Rilgar's / Pokitaru's seas, Gaspar's lava (now drawn by
  mrp_fx from their input on the console).
- **Proposals (not applied; rendering only, no game state).** (a) A kept batch's 0xc / 0xe run skipped altogether: its strip's ADC flags
  computed on the host (the clip matrix's transform and the w lane with the VU rules: ~1/3 of the program's arithmetic, no clipper, no
  kick), vf31 and the clip flags set as the run leaves them. Not exact on the reader's own state: 0xe clip-tests one vertex past its
  batch (the memory after it - the previous runs' in-place output, which a skipped run never writes), whose flags reach only the next
  batch's kicked[1] - a flag no drawing reads (mrp_fx reads kicked[2..]). ~1-2 ms more if 0xe still shows after this round. (b) The
  particles drawn from their records (the chunks the reader UNPACKs) instead of the program's kicks taken back to the world: no 0x14 run,
  no kicks, a cheaper effects' prepare - not bit-exact (the projection's float noise gone), a look to compare.

### The effects' prepare on core 0, their vertices read in place, the sky after a depth pre-pass, the ties' packets replayed (2026-10-31; to measure)

- **What n_split says (Novalis 1080p, RAC1_WIIU_PREP_SPLIT=1):** the main thread does ~24 ms of work a host frame (prepare 5, the world job's
  first half 6.5, effects' prepare 3.5, draws 8.2, GamePad 0.4), core 0 ~14 (the chain reader 9.6, the second half 4.7); the frame is 33.3 by
  the vsync. The GPU's "busy" 30.2 is not its work: ties/shrubs pass 1 7.54 with a CP gap of 7.53 means the GPU drew them as fast as the CPU
  sent them. Its own work is ~17 ms, the sky's 5.4 of it (7.39 less its gap): "late sky, last 120 calls: 0 after the tfrags' first pass, 120
  first" - on Novalis the late sky never applied (the one tfrag batch interleaves opaque and other ranges: on the PC's Novalis explore ~93% of
  the tfrag triangles are in opaque ranges, ~20% in the leading ones).
- **RAC1_WIIU_FX_WORKER (default 1; =0 as before):** mrp_fx_wiiu_prepare handed, after the 2D's build, to the world's preparation worker on
  core 0 (mrp_world_wiiu_side_post: a side job taken once the worker is free - after the second half when that one is its; the world's joins
  never wait for it), waited for before the effects' draws (draw_frame: mrp_world_wiiu_side_join, which runs it on the main thread if the
  worker has not taken it). Expected: main thread -3.5 ms; core 0 +3.5 (chain 9.6 + second half ~4 + effects ~3.5, before ~20 ms of a cycle).
  The timeline's new line: "[core0] the effects' prepare A->B on cores 0/1/2 ..; the main thread's wait for it before the effects' draws C->D".
- **RAC1_WIIU_FX_DIRECT (default 1):** the prepare's vertices in two 64-byte aligned vectors used in turn (the one the frame before last drew:
  its GPU work waited for before this frame's prepare), flushed by the core that wrote them; mrp_world_gx2_draw_fx binds them where they are
  (GPU attribute cache invalidated) - no copy into the geometry ring (PC Novalis explore: 2-6 K vertices, 150-450 KB a frame).
- **RAC1_WIIU_SKY_PREPASS (default 1, with the late sky):** when the frame's first tfrag batch mixes opaque ranges with others, the depth of
  its opaque ranges that write every pixel (TfragRange::opaque && !pass2) is drawn first - colour off, LEQUAL, with exactly the shaders their
  first pass uses (set_p1_at) -, then the sky tested against it, then the batch as before (the opaque ranges pass again at their own depth,
  LEQUAL). Only for a batch tested GEQUAL->LEQUAL with depth writes and blended as none / ALPHA_1 0x44. Same picture except: a range drawn
  before an opaque one, in front of it, writing its depth, with partly transparent pixels (an alpha-tested cut-out's edge) shows the clear
  colour through those pixels instead of the sky (the PS2 showed the sky there though the terrain is behind - an ordering artifact either
  way): to look at on Novalis' fences / foliage over the ground. Expected: the sky's 5.4 ms times the sky's share of the screen, plus the
  pre-pass (~0.5 ms: the tfrag vertex shader again, depth only); "[gpu-world]" tfrag pass 1 shows 2 passes a frame (the pre-pass counted
  there); log: "late sky, ...; N of the late ones after the depth pre-pass (M ranges a call, RAC1_WIIU_SKY_PREPASS)".
- **RAC1_MRPW_PKT_MEMO (Wii U default 1, PC 0; =2 checks every replay against the full one):** gpu_packet (build_triangles, the second half
  on core 0) keeps a packet's runs - keys, units, batch keys, draw records but the instance and pass-2 flag - when the GS registers its keys
  read before writing them come out as they went in and no epoch opened; the next draw of the same packet (static_frame_build emits a
  packet for every instance of its class in a row) with the registers untouched since (GsFront::gen) appends them without the A+D writes,
  key_now, the units' search and batch_key. Also vp * V kept per view ((vp * V) * R as before: the same doubles). PC: Novalis explore 1500
  (RAC1_MRP_LOD=1, the host's ties / shrubs) 431 K of 768 K packets replayed, 0 differing; Veldin forward 749 K / 1175 K, Kerwan 328 K / 576 K,
  0 differing; MRP_LOD shots identical with =0 / =1. Expected: the second half 4.7 -> ~3 ms (half its packets skip the per-run GS work).
- **The first half:** static_frame_build's per-class lists and static_draws' map kept between frames (no allocation per class: malloc is
  shared with the game thread), the scene packets made in place. RAC1_WIIU_PREP_TIME=1 now prints "[mrpw] the prepare's first half":
  the frame, the tfrag decisions, scene_from_static_k and its parts (reset, sky packets, tfrag draws, ties', shrubs') with the counts -
  the old line called scene_from_static_k "the sky's run". On the PC's Novalis the scene is ~550 packets a frame, so its 4.2 ms on the console
  (n1080_tvs) is not the packets' count: the next bench with RAC1_WIIU_PREP_TIME=1 says which part.
- **To measure** (BUILD=build-wiiu-mt): the defaults with RAC1_WIIU_PREP_TIME=1; RAC1_WIIU_FX_WORKER=0; RAC1_WIIU_SKY_PREPASS=0;
  RAC1_MRPW_PKT_MEMO=0; on Novalis and Veldin open.
- **Proposals (not applied).** (a) The opaque ties / shrubs in the sky's pre-pass too (their merged draws, depth only): the sky only where it
  shows. (b) The sky's pixel shader killing pixels whose alpha is 0 (exact for ALPHA_1 0x44: Cd), saving the ROP on the clouds' clear parts -
  a new pair. (c) Option (a) of the brief - static instance matrices in a texture, clip / rows in mrp_world.vert - first needs the first
  half's parts: if static_frame_build's instance_rows / EePages reads dominate, it pays; if scene_from_static_k's reset or the sky's
  packets do, a cheaper fix is there. (d) At 60 fps core 0 would be ~17 ms (chain 9.6 + tail + effects): the tfrag decisions (1.1) could
  stay on the main thread and the effects' doubles become floats.

### The tfrags' vertex shader: indexed, one view set, its GPR share - and what "tfrag pass 1" measures (2026-10-31)

- **RAC1_WIIU_TFRAG_IDX (default 1; =0 a corner each as before):** the vertex shader's tfrags drawn indexed (GX2DrawIndexedEx, u32 indices
  after the attribute words in the geometry ring): a sent vertex's word once (draw << 16 | vertex << 2 | 3: corner 3, the vertex itself,
  no triangle's ADC), its strip's triangles indexing it - the post-transform cache shares it. Form 3's draws (ADC: the triangle dropped
  when one of its three vertices is outside the guard band, a triangle's decision) keep their three corners each, indexed in place: no
  vertex kill flag nor NaN trick needed. The ranges' first / count are the indices' (the same triangle slots). Veldin open: 10,938 corners
  -> 5,444 vertices shaded a frame; Novalis ~8,000 -> ~4,300 (Frame::tvs_index, mrp_world_backend.inc FrameView::tvs_index).
- **RAC1_WIIU_TFRAG_ONESET (default 1; =0 the generic pairs only):** the frame's block starts with a header texel (the draws' first texel,
  the view sets' count) and the view sets from texel 1; shaders/mrp_tfrag_vs1_fast[_noat].gsh (mrp_tfrag_vs.vert built RAC1_TVS_ONE_SET)
  read the set at constant indices - the constant cache instead of a fetch per use: 27 -> 19 GPRs, 103 -> 63 fetches in the program. Used
  by the frames whose draws share one view set (every frame of Veldin / Novalis seen: "N frames by the one-set pairs" on the log line);
  the others take the generic pairs.
- **RAC1_WIIU_TFRAG_VS_GPRS (default 128; 0 GX2's split), RAC1_WIIU_VS_GPRS / RAC1_WIIU_MOBY_VS_GPRS (default 0):** GX2SetShaderMode gives
  the vertex shaders 48 of the 248 GPRs: a 24-27 GPR vertex shader (the ties', the tfrags') runs one wavefront a SIMD at a time, its fetches'
  latency never hidden. gx2_shader_mode (mrp_world_gx2.cpp) gives n to the vertex shaders, 248 - n to the pixel shaders, per path (the
  world's set_mode keyed by mode and path). The tfrags' vertex shader takes 128; the ties' / shrubs' pass is pixel-bound (128: 7.5 ->
  7.9 ms, 184: 13 ms - the pixel shaders starved) and the mobys' did not move: both left at GX2's.
- Checked on the PC (Veldin GOTO=0@60 forward 720 frames, RAC1_MRPW_TFRAG_CPU=1): RAC1_MRPW_TFRAG_VS_CHECK=1 / 2 (the twin against the CPU
  route, the GLSL by the PC's GL against the twin) with RAC1_WIIU_TFRAG_IDX=1 and 0: the same counts (2,273,976 corners, ADC 2 dropped / 0
  differing, positions past 1e-4 8704, STQ / RGBA / F / runs 0) - the GL test now runs over the attribute words (1.02 M for 2.27 M);
  =3 the RAC1_TVS_ONE_SET build in the GL test: the same numbers. pc_check.sh / gt_check.sh green.
- **Measured** (build-wiiu-mt, averages over the windows after the first two; tfrag / ties "pass 1"):

  | run | tfrag p1 | ties p1 | GPU busy | host frame | GPU wait |
  | :--- | :--- | :--- | :--- | :--- | :--- |
  | Veldin open, all three off | 7.91 | 7.52 | 32.39 | 32.74 | 14.12 |
  | + RAC1_WIIU_TFRAG_IDX alone (earlier build) | 6.94 | 7.52 | 31.94 | 33.58 | - |
  | + RAC1_WIIU_TFRAG_ONESET, GPRs at GX2's | 5.68 | 7.54 | 30.62 | 32.18 | 10.82 |
  | defaults (+ tfrag VS GPRs 128) | 5.13 | 7.57 | 29.48 | 29.81 | 11.16 |
  | Novalis 1500, all three off | 5.98 | 5.23 | 29.53 | 31.64 | 11.99 |
  | Novalis, defaults | 4.87 | 5.28 | 28.36 | 30.78 | 10.67 |

  The host frame varies a lot from run to run (realtime input): the GPU columns are the ones to read.
- **What "tfrag pass 1" is now (the experiments, Veldin open, RAC1_WIIU_PIXELS_OFF=1, testing builds through RAC1_WIIU_TFRAG_VS_TEST=<name>:
  the one-set pairs loaded from shaders/<name>.gsh):** the real shader 5.26 ms; a vertex shader with no fetch at all (a constant position
  from aCorner) 4.97; the leaf's selections skipped 5.50 (pixels on: the same as the real one); no tfrag draw at all (RAC1_WIIU_TFRAG_STAGE=2)
  4.99. So ~5 ms of the segment is not the tfrags' work: it is the first world segment, from the depth clear's mark to the tfrags' first
  pass, and the GPU waits there for the CPU (the frame starts on an idle GPU - GX2WaitTimeStamp on the frame before - and gets the world's
  commands as the main thread makes them). The tfrags' own vertex work is now ~0.3 ms of it (pixels off), from ~3 ms before. With the
  sky drawn first (RAC1_WIIU_SKY_LATE=0) the wait goes into the "sky pass 1" segment and the tfrag pass shows its own time: 3.47 ms (all
  three off) -> 2.00 (defaults), one pass (no depth pre-pass).
- Tried and dropped: the plans' tables a texel a sent vertex with the next fetches resolved in it (form 3 / 4: the raw record's qword;
  form 0 without selections: the leaf's qword and record) - two dependent fetches less a vertex, no measurable change, +1.8 MB (Veldin) /
  +4.3 MB (Novalis) of tables.
- **To decide (not changed):** RAC1_WIIU_SKY_LATE=0 with the vertex shader cheap: Veldin open GPU busy 29.48 -> 27.60, host 29.81 -> 28.08,
  GPU wait 11.16 -> 8.65 (the sky's full screen drawn while the GPU would wait anyway, no depth pre-pass); Novalis GPU busy 28.36 -> 28.91,
  host 30.78 -> 31.43 (worse). The picture of SKY_LATE=0 is the one before the late sky (no pre-pass artifact).
- **Next (proposals):** the frame's start - the GPU idle from the depth clear until the world's first commands: issue the world's first
  draws sooner (flush after the tfrags' pre-pass, not only every 64 draws; the tfrag block / corners copied before the sky's deferral), or
  wait on the frame before last instead of the frame before (the rings sized for two frames) so the GPU always has queued work. Ties /
  shrubs: their pass 1 is ~4.6 ms of pixels and ~2.9 of the rest at 1080p (RAC1_WIIU_PIXELS_OFF: 2.90): their vertex shader is not the
  cost.
- Shots (Veldin open, RAC1_REALTIME=0, game frame 1200): defaults against all three off differ only where two runs of the same build
  differ too (Ratchet's pose, the particles, blinking lights - the run's own timing); outside 4 px of those, 3,795 pixels, all at Ratchet
  and the effects near him, none on the terrain.

### Two frames in flight: the GPU draws frame N while the main thread makes N + 1 (2026-11-01)

- **Why:** wiiu_gfx_frame waited (GX2WaitTimeStamp) for the frame before's GPU work before this frame wrote anything the GPU reads, then
  sent this frame's commands as it made them (the world's ~4 ms, the mobys' ~2.7): the GPU started each frame idle and drew as fast as the
  CPU sent - the ~5 ms of "tfrag pass 1" that is no tfrag work, and "GPU wait (the frame before)" ~11 ms on the main thread. CPU and GPU
  took turns.
- **RAC1_WIIU_FRAMES_IN_FLIGHT (wiiu_main.cpp sets 2; unset or 1: as before):** with 2 only the frame before last is waited for
  (g_frame_ts_prev), so the frame before may still be drawing while this one's commands are made and queue behind it. Everything the GPU
  reads that the CPU writes per frame, and what keeps it safe with two frames in flight:
  - the backends' rings (the world's blocks / plan / tfrag geometry / sky, the mobys' blocks, the 2D's vertices and blocks): already
    rings of 2-16 MB with a frame using tens to hundreds of KB. Gx2RingUse (mrp_world_gx2.h) counts a host frame's bytes and the frame
    before's (once a host frame: g_gx2_frame); at a wrap the CPU waits (GX2DrawDone) when this frame's and - two in flight - the frame
    before's bytes pass half the ring, as the one frame did before. 0 waits on both levels (the [gx2] DrawDone line).
  - the effects' vertices read in place (RAC1_WIIU_FX_DIRECT, mrp_fx.cpp): three vectors in turn (two with 1 in flight), the prepare -
    before the GPU wait, on core 0 - writing the one the frame two before drew.
  - in-place rewrites of data a frame drew from: the tfrag vertex shader's colours' texture (written / made again when the colours change),
    the host sky's runs (sky_host_bake: "runs' values written again", ~5 per 120 calls on Novalis), the GamePad debug menu's quads (when
    its text changes: the stats every 30 host frames) - each waits for the frame before first (gx2_wait_frame_before; ~50 waits per 300
    host frames on Veldin, ~25 on Novalis).
  - the 2D's texture pool (a texture refilled or freed when the GPU may still sample it): GX2DrawDone when it was last used within the last
    4 calls (2 with one frame in flight).
  - unchanged and safe as they were: the GPU's own targets (colour / depth, the scaled 3D, the shadow mask, the 2D's render targets: the GPU
    runs its commands in order), the level's data (made with a GX2DrawDone), cols_sync (GX2DrawDone), the blit quads (8 slots, up to 3
    a frame), the GPU's timing marks (4 sets, the one two frames old read after the wait).
  - one frame in flight kept for a movie (its texture written in place every frame), the GS replica's 2D (its own ring) and without the
    overlap (RAC1_WIIU_OVERLAP=0).
- **Measured** (build-wiiu-mt, averages over the windows after the first two; the same build with RAC1_WIIU_FRAMES_IN_FLIGHT=1 / 2):

  | run | tfrag p1 | GPU busy | host frame | GPU wait | swap wait |
  | :--- | :--- | :--- | :--- | :--- | :--- |
  | Veldin open, 1 | 5.17 | 30.09 | 32.21 | 10.65 | 0 |
  | Veldin open, 2 | 3.32 | 28.34 | 29.52 | 1.59 | 2-7 |
  | Veldin open, defaults (2), again | 3.41 | 28.53 | 29.79 | 1.65 | |
  | Novalis 1500, 1 | 4.74 | 28.38 | 31.35 | 10.89 | 0 |
  | Novalis 1500, 2 | 4.07 | 27.79 | 30.27 | 2.41 | 0-9 |
  | Novalis 1500, defaults (2), again | 3.61 | 27.70 | 30.33 | 2.34 | |

  Veldin open's heavy windows 31-33 ms -> 28-30. Every window's host frame now equals its GPU busy time: the GPU holds the frame, the main
  thread waits at the swap (the vsync) instead. With the late sky off (RAC1_WIIU_SKY_LATE=0) and two in flight: Veldin 29.47 (the same),
  Novalis 31.63 (worse): the late sky stays.
- **Memory:** the third effects' vector - malloc's used bytes +0.4 MB on Veldin, +1.0 MB on Novalis (the rings unchanged); malloc free
  ~6-7 MB on Veldin open, 15-23 MB on Novalis at the logged frames.
- **Shots** (RAC1_REALTIME=0, 1 against 2): Veldin open game frames 1200 and 1800, Novalis 900: the pixels differing are Ratchet's pose,
  the particles / sparkles, the birds, the blinking lights and the smoke - as between two runs of one build (Veldin 1200: 18,196 pixels
  for 1 against 2, 21,895 between two runs of the earlier build); nothing on the terrain, the ties, the sky, no stray triangles.
- **What is left of the frame's start:** the GPU still waits ~2 ms in the tfrag segment (its CP gap 1.9 on Veldin): the main thread waits
  ~3.6 ms at the draws' world join for the world job's second half on core 0 (the [core0] line: main at the draws 16.9, the second half
  done 19.6), and the world's first commands come after it. The swap wait (2-7 ms) is the CPU's slack now: the next gains are the GPU's own
  work (ties / shrubs pass 1 ~7.6 ms on Veldin open, the mobys ~5.3, the sky ~3.8).
- One frame more between the input and the screen (~30 ms) with two in flight.

### The GPU's pixels at 1080p: the mobys' fast pixel shader, the sky's shells, and what bounds a pixel on Latte (2026-11-01)

- **Where the sky's 5 ms went (Veldin open):** a new line, "[gpu-sky]" (RAC1_WIIU_GPU_TIME=2: the sky's pass-1 marks by their order in the
  frame), and "sky info" lines on the log (per sky batch, once a level and again when the count changes: host shell / CPU path, corners,
  blending, test, texture word, its texture's alphas, the vertices' alphas, a CPU batch's box in NDC). Veldin's sky is 11 host shells and
  ~10 small CPU batches (the stars: 32 x 32 sprites, a few ms of nothing); the time is the shells': the dome (shell 0: untextured, every
  vertex's alpha 0x80, ALPHA_1 0x44 - opaque) 1.37 ms, the stars' shell (97% of its texels alpha 0) 1.08, the clouds' shells (textures
  half clear, half below 0x80) 0.23 + 0.80 + 0.93 + 0.53; the rest ~0. Novalis: the dome 1.25 and four cloud shells 0.28 / 1.28 / 0.68 / 0.24.
- **What does not bound them:** the textures' tiling (RAC1_WIIU_SKY_TILED: the shells' textures copied tiled by the GPU - no change, they
  are magnified; default 0), the blending (RAC1_WIIU_SKY_KILL0, testing: the textured shells' pixels of alpha 0 discarded - their (Cs - Cd)
  As + Cd leaves Cd - no change; default 0), the pixel shader's ALU alone (the noat build, 16 -> 10 ALU groups: -10%).
- **What does: the pixel shader's inputs.** RAC1_WIIU_SKY_UTERMS: a shell is one draw a batch, its terms (what mrp_world_fast.glsl's
  fast_terms makes per vertex: vLod, vFnA, vFnB, vAt, vSel - five flat vec4s) the same for the whole draw: written by the CPU
  (fast_terms_cpu, the same float values) into a block of the pair's own (SkyTerms; shaders/mrp_sky_host_fast_noat_ut.gsh,
  mrp_sky_host_fast_noat_notex_ut.gsh - mrp_world_fast.frag built RAC1_UNIFORM_TERMS) - the pixel's inputs 9 -> 4 (3 untextured), the ALU
  the same: every shell -30..40% (the dome 1.04 -> 0.63, the stars 0.96 -> 0.67, the clouds 0.71 -> 0.49, 0.83 -> 0.57, 0.47 -> 0.33), the
  sky 3.12 -> 2.10 ms on Veldin open. On Latte a pixel's rate follows the count of its interpolated inputs (flat ones too) more than its
  ALU: the fast pixel shaders' flat terms (moved there from the pixels' decoding, 2026-10-28) cost as inputs. gx2_pair_variant accepts a
  block the main shaders lack when it is named SkyTerms / MobyTerms (the caller binds it at the pair's own place).
- **The sky's other switches (exact by construction):** RAC1_WIIU_SKY_OPAQUE - an untextured batch whose vertices' alphas are all 0x80 and up
  under ALPHA_1 0x44 (the dome) drawn with the blending off (As 1: Cs 1 + Cd 0 is Cs); RAC1_WIIU_SKY_NOTEX - an untextured host shell by the
  pair built RAC1_NO_TEX (no sample: t' = ta, what t tm + ta is with tm 0; shaders/mrp_sky_host_fast_noat_notex.gsh): the two together the
  dome 1.37 -> 1.04, Novalis' 1.25 -> 0.95; RAC1_WIIU_SKY_ATMODE - a batch keyed with the alpha test whose draws all have the test's mode
  off (draw_flags bit 3: ATE, AFAIL FB_ONLY, no depth write - the shader never discards) by the noat pair: the textured shells -10%.
- **The mobys' fast pixel shader (RAC1_WIIU_MOBY_FAST):** mrp_moby.frag decoded the draw's texture word per pixel (TFX / TCC / the level's
  L / K / MXL / LCM / the levels, by branches; a textureSize per pixel): 55-73 ALU groups, 28-33 CF instructions. The pairs
  shaders/mrp_moby_fast.gsh / mrp_moby_fast_noat.gsh (mrp_moby.vert built RAC1_FAST: moby_fast_terms from pCmd.y / z and the batch's
  TME / ATST / AREF the CPU puts in MobyDraw's pCmd.w; mrp_moby_fast.frag): 20-27 ALU groups, 6 CF. Its arithmetic in mrp_moby.frag's
  order (o = (t' (A + B c)) S + C c.a: the product t c first, then x 255 / 128 - the same values), the levels as mrp_moby.frag's (mode 0 the
  GS's, mode 1 by the derivatives - the bound array's size from the batch's block, uC.y / z, instead of textureSize), the alpha test as
  mrp_world_fast.frag's one comparison, the discard's rule (pass, AFAIL) worked out by the CPU into uC.w. The main shaders' .gsh are
  byte-identical to before. RAC1_WIIU_MOBY_UTERMS (prepared, **not run on the console yet**, default 0): the same pairs built
  RAC1_UNIFORM_TERMS (mrp_moby_fast_ut.gsh, mrp_moby_fast_noat_ut.gsh) taking the draw's terms from a block (MobyTerms, moby_terms_cpu,
  one a draw command kept for the frame) - three inputs a pixel instead of eight, as the sky's shells above.
- **Measured** (build-wiiu-mt, averages over the windows after the first two; each row adds its switch to the row before; ms):

  | run | sky | tfrag p1 | ties p1 / p2 | mobys + deferred | effects | GPU busy | host |
  | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
  | Veldin open, before | 3.77 | 3.41 | 7.59 / 1.22 | 5.28 + 1.89 | 1.54 | 28.65 | 29.91 |
  | + SKY_TILED | 3.78 | 3.47 | 7.61 / 1.22 | 5.27 + 1.88 | 1.48 | 28.59 | 29.86 |
  | + MOBY_FAST | 3.72 | 4.60 | 7.54 / 1.23 | 3.33 + 1.39 | 1.35 | 27.02 | 28.34 |
  | + SKY_OPAQUE + SKY_NOTEX | 3.42 | 5.09 | 7.52 / 1.23 | 3.31 + 1.39 | 1.50 | 27.31 | 28.70 |
  | + SKY_ATMODE | 3.12 | 5.44 | 7.51 / 1.23 | 3.31 + 1.39 | 1.37 | 27.22 | 28.65 |
  | + SKY_KILL0 (testing) | 3.15 | 5.60 | 7.51 / 1.23 | 3.31 + 1.40 | 1.45 | 26.94 | 29.22 |
  | + WORLD_MERGE=4 (instead of KILL0) | 3.05 | 5.99 | 7.34 / 1.09 | 3.24 + 1.42 | 1.46 | 26.87 | 29.35 |
  | + SKY_UTERMS (instead of KILL0) | 2.10 | 6.25 | 7.49 / 1.23 | 3.30 + 1.40 | 1.44 | 27.06 | 29.13 |
  | Novalis 1500, before | 1.50 | 4.04 | 5.22 / 2.42 | 2.06 + 3.96 | 3.35 | 27.75 | 30.34 |
  | + SKY_TILED | 1.51 | 4.05 | 5.23 / 2.42 | 2.08 + 3.96 | 3.39 | 27.87 | 30.43 |
  | + MOBY_FAST | 1.53 | 4.15 | 5.28 / 2.44 | 1.32 + 2.37 | 3.36 | 25.71 | 28.59 |
  | + SKY_OPAQUE + SKY_NOTEX | 1.32 | 4.27 | 5.19 / 2.45 | 1.32 + 2.37 | 3.37 | 25.69 | 28.54 |
  | + SKY_ATMODE | 1.29 | 4.25 | 5.28 / 2.44 | 1.33 + 2.36 | 3.37 | 25.58 | 28.54 |
  | + SKY_KILL0 (testing) | 1.29 | 4.24 | 5.28 / 2.45 | 1.31 + 2.38 | 3.30 | 25.49 | 28.43 |
  | + WORLD_MERGE=4 (instead of KILL0) | 1.27 | 4.37 | 4.57 / 1.99 | 1.22 + 2.44 | 3.08 | 24.37 | 27.67 |

  Novalis with SKY_UTERMS not measured: the console stopped answering after the Veldin run (it had exited normally). Shots of the new
  switches not compared yet (same reason): **all the new switches are default 0** until they are - the picture should be identical by
  construction (the same values in the same order; the dome's blending off where As is 1). The measured set:
  `RAC1_WIIU_MOBY_FAST=1 RAC1_WIIU_SKY_OPAQUE=1 RAC1_WIIU_SKY_NOTEX=1 RAC1_WIIU_SKY_ATMODE=1 RAC1_WIIU_SKY_UTERMS=1`; the .gsh files on the
  SD already (mrp_moby_fast*, mrp_sky_host_fast_noat_notex*, mrp_sky_host_fast_noat_kz, mrp_sky_host_fast_noat_ut; not yet
  mrp_moby_fast_ut / mrp_moby_fast_noat_ut).
- **Where the gains went:** the GPU's own work fell ~4.2 ms on Veldin open (mobys -2.5, sky -1.7) and ~2.5 on Novalis (mobys -2.3, sky
  -0.2), but "GPU busy" only ~1.6 / 2.2: the frame's start now waits for the CPU's commands longer - "tfrag pass 1"'s CP gap on Veldin open 1.13 (before) -> 2.21
  (mobys) -> 3.83 ms (all), the segment itself 3.41 -> 6.25. Novalis' host frame (28.5) is now ~3 ms above its GPU busy (25.6): the CPU
  holds it. The next GPU gains count only once the world's first commands come sooner (the proposals of 2026-10-31 above: the world job's
  second half before the draws, or the world's draws issued in two parts).
- **RAC1_WIIU_WORLD_MERGE=4** (the existing switch: the items split by texture unit, no unit tree in the pixel shader): ties pass 1 / 2
  -0.2 / -0.1 ms on Veldin open, -0.7 / -0.5 on Novalis, but 523 -> 1865 GX2 draws and "world draws" on the CPU 4.4 -> 7.2 ms on Veldin
  open: the host frame worse there (29.35 against 28.65), better on Novalis (27.67 against 28.54). Not changed (mode 3 stays).
- **Not done:** the opaque ties / shrubs in the sky's depth pre-pass (proposal (a) of 2026-10-31) - their pass 1's non-pixel part (~2.9 ms
  with the pixels off) would be paid twice for at most ~2 ms of sky; a position-only vertex shader for the pre-pass would have to give
  the very same depths as the full one (else holes where the colour pass's LEQUAL fails). With the sky's shells cheaper (above) it pays
  even less.
- **Next (proposals, by what this found):**
  - the shots (Veldin open 1200 / 1800, Novalis 900 / 1400, RAC1_REALTIME=0) of the measured set against the defaults, then the defaults
    on; RAC1_WIIU_MOBY_UTERMS measured (expected: the mobys' pixels another -30..40%, as the shells');
  - the same for the world's fast pairs: the tfrags' and the ties' / shrubs' pixel shader reads nine inputs (vStq, vQF, vColor, vFlat and
    the five flat terms). A tfrag range or a lone draw has one texture word: its terms could come from a block as the shells'; the merged
    ties' draws change word per triangle - their five flat vec4s packed into one or two (the texture function's A / B / C and the alpha
    test's r / m / n / th from small codes decoded by a few ALU groups: ALU is cheaper than inputs here). Ties pass 1's pixels are ~4.6 ms
    on Veldin open;
  - the effects (Novalis 3.35 ms) draw with the tfrags' fast pairs (nine inputs) and linear textures (fx_texture): the same two ideas;
  - the mobys' vertex shader: 341 ALU groups, 246 CF instructions (42 branches, 27 fetch clauses: joint_row's bounds, matrix_of's cases) -
    the mobys' non-pixel time (~2 ms on Veldin open) is likely there.
