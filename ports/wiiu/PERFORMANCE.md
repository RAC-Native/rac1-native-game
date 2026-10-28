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
