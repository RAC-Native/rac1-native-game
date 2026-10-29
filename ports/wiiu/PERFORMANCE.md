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

