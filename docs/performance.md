# Performance

The game's own update runs on one thread at a time, as on the PS2 (see
[Architecture](architecture.md#threads-and-the-cores)). A host frame is the game frames it runs, the preparation of the
picture (what it reads of the game's state) and its drawing. When the main thread's part of a host frame takes longer than a
sixtieth of a second, the next host frame runs two game frames and shows one: the game keeps its speed, the picture its rate
halves for that frame. This page says where that time goes, what runs beside it, and how a change to it is measured and
proved.

## A host frame

With the pipelined frames (on by default in a window: the debug menu's Display tab, `RAC1_PIPELINE`) the frame prepared at
the end of one host frame is drawn in the next, one frame of latency:

| | The main thread | Beside it |
|---|---|---|
| 1 | the window's events and the pads; the game frames' count | |
| 2 | the pipelined frame of the last host frame drawn (from what its preparation took) | the game frames, on the game thread (`RAC1_GAME_THREAD`) |
| 3 | waits for the game thread; runs what the update asked of it (the pause menu's picture store) | |
| 4 | this frame prepared: the view, the frame's chain captured, the world's scene started, the moby draw lists | the MRP chain worker; the MRP scene worker (the scene, then its triangles: `RAC1_MRPW_BUILD_THREAD`) |
| 5 | the frame cap, the present | the workers go on |

Without the game thread (`RAC1_GAME_THREAD=0`), row 2 runs the game frames first and then the drawing, both on the main
thread: the host frame costs their sum. With it, the host frame costs the update (on the game thread) plus the preparation,
the drawing hidden beside the update; a host frame whose drawing is longer than the update waits for the drawing instead.

What does not overlap: a frame of a blocking loop (the screen fades, a movie, the Vendor's screens), the full-screen menu, a
level's exit and load, the boot, a host frame with the debug menu open, and every deterministic headless run (those have no
pipelined frame).

## The game's update

The profiler's game stages (`Game: ...`, `Host: LOD ...`) split the update. The largest on a level full of animated mobys is
the joints and skinning (the moby pass's `0x251490`: each animated moby's pose `0x250fe0`, its skinning `0x1ee858` and the
skinning's VU0 program), run twice when the LOD slider or the widened view re-run the moby pass. The pose keeps its VU0
registers and flags in SIMD registers through its four hot loops (`moby_joints.cpp`, `RAC1_VU0_SIMD=0` for the op-by-op form):
the same ops in the same order, each operand clamped and each result clamped and flagged as the VU0 does, no FMA, the
thread's rounding (toward zero, no denormals). On a CPU with AVX2 (chosen when the game starts, whatever the build's target;
`RAC1_VU0_SIMD=1` keeps the 128-bit form) two of a joint's four matrix chains run side by side in a 256-bit register: the
chains share nothing but ACC, which each one's first op overwrites, and the flags, whose sticky bits are an OR. About a
quarter less time for the joints and skinning stage (Rilgar 1.9 to 1.4 ms a game frame, Kerwan 1.5 to 1.1).

The loading screen's flight between the planets draws its ship and planets with the MRP (the `flight/` assets), but until
November 2026 it did not skip the VU0 skinning (`moby_skin_all`), which only fills the moby chain for the VU1 path: it was
nearly all of the flight's frame. The skip now holds there too, the skinning alone (`RAC1_MRP_SKIP_FLIGHT`; the sub-mesh fill and
the texture slots, cheap there, still run): headless, the flight's `Game: moby joints / skinning` 2.0 to 0.02 ms a frame, the
main thread's CPU 2.6 to 0.8 ms (helpers not inline). The pictures are the same. RAMHASH is not: what the skinning wrote is
gone, so the frame's packets (the packet ring) and the scratchpad differ during the flight, and on 2 of the 19 levels some of
the ring's pages stay different afterwards (the level's own mobys, skipped as always in play, leave those bytes unwritten: the
flight's skinned vertices before, older bytes now). Nothing but the GS / VU1 path reads those bytes; every other page is the
same at every frame.

## The generated code's helpers and the world culls

Profiled with a sampler in November 2026, the transliterated (generated) code was 30 to 43 % of the main thread's CPU, and most
of that was not its own work but calls to small helpers GCC kept out of line at `-O2`: the memory accessors (`ee_read` /
`ee_write`, the routines' `lw_` / `sw_` / `lq` / `sq`) and the VU0 macro ops (`vu0_nf.h`). Two thirds of it was three routines,
the world passes' culls (the profiler's `Game: world passes`).

- **The helpers inline** (CMake `RAC1_INLINE_HELPERS`, off by default): the accessors' and the VU0 ops' fast paths are forced
  inline (`runtime/kernel/hot_inline.h`), their rare paths (a null address, a flagged op, a DMA register) out of line and cold,
  and `vu0::running()` an inline load. The same C++, so the same game state (RAMHASH on the 19 levels, the reference images).
  Measured headless (`forward`, the profiler's medians, three interleaved runs per build on a shared machine): `Game: world
  passes` 15 to 37 % less (Veldin 1.10 to 0.93 ms a frame, Rilgar 0.67 to 0.54, Nebula G34 0.83 to 0.65, Pokitaru 0.66 to
  0.53, Hoven 1.22 to 0.77), but the game code as a whole within the noise (-5 % to +1 %): the program's code grows from
  37 to 95 MB, and a full build with three jobs from about 10 to 22 minutes.
  Off by default for now, as the whole game code does not gain enough for the size and the build time; on with
  `-DRAC1_INLINE_HELPERS=ON`. On a target with a small instruction cache more inlining measured slower.
  Measured again on top of the hand-written culls, lighting, shadows and sky (November 2026, the same method, the walk after
  the level's arrival): the game code -11 to +13 % (Veldin 1.77 to 1.92 ms, Kerwan 3.23 to 3.40, Rilgar 2.21 to 2.13, Nebula
  G34 2.44 to 2.75, Hoven 2.85 to 2.54), i.e. no gain beyond the noise; the program's code 37.3 to 95.5 MB (the file 170 to
  250 MB), a full build with three jobs 8.0 to 20.4 minutes (21 to 59 minutes of CPU). Still off.
- **The three culls by hand** (`world_cull_tfrag.cpp`, `world_cull_tie.cpp`, `world_cull_shrub.cpp`; `RAC1_WORLD_CULL_FAST`):
  the same arithmetic and the same memory written, organised by what the routines do (the view test, the LOD and its morph,
  the chain's records), the VU0 registers in a local copy (`vu0_local.h`) written back at the end, the scratchpad as a byte
  view. The transliterations stay as the reference (`world_*_cull_tr`) and as the fallback (a VU0 program running,
  `RAC1_PERF_VU0_NF=0`). Proved by oracle batch 480 against each level's original on dumps of seven levels, by
  `RAC1_WORLD_CULL_FAST=2` (both run at every call and compared: memory, VU0 registers and flags, the visible lists) over the
  19 levels, and by RAMHASH. On top of the helpers inline, `Game: world passes` 10 to 24 % less (Veldin 1.13 to 0.93 ms, Rilgar 0.74
  to 0.56, Nebula G34 0.82 to 0.73, Pokitaru 0.73 to 0.59, Hoven 1.14 to 0.95); with the helpers not inline
  (`RAC1_INLINE_HELPERS=OFF`) 30 to 45 % less (Veldin 1.39 to 0.91, Rilgar 1.06 to 0.67, Nebula G34 1.19 to 0.83, Pokitaru
  1.00 to 0.66, Hoven 1.97 to 1.08) and the game code 12 to 26 % less. The stage also holds the passes' packets and the LOD
  slider's re-run of each pass. In the sampler the culls went from 19-30 % of the main thread to 14-21 % (helpers inline),
  then 10-16 %: they still clamp every operand of every op as the VU0 does.
- **The tfrag lighting by hand** (`world_lighting_tfrag.cpp`; `RAC1_WORLD_LIGHTING_FAST`): 0x295350, which relights the world
  pieces with lights each frame (and every piece at the level's load), in the same form: the same reads and writes in the same
  order, the MMI unpacks done on words, the VU0 registers in a local copy; as in the transliteration, only the point lights'
  range test (an op into vf0) makes flags. Proved by oracle batch 481 on the dumps of seven levels (the frame's own relight list
  and random ones), batch 101, `RAC1_WORLD_LIGHTING_FAST=2` over the 19 levels (6589 calls, 0 differing) and RAMHASH. It
  matters on Nebula G34, where a flickering light relights 20-30 pieces a frame: `Game: world lighting / pass ends` 0.51 to
  0.20 ms a frame (helpers not inline; three interleaved runs); elsewhere it runs rarely and the stage stays at 0.01-0.02 ms.
- **The drop shadows and the sky by hand** (`moby_shadows_hand.cpp`, `RAC1_DROP_SHADOWS_FAST`; `sky_hand.cpp`, `RAC1_SKY_FAST`):
  the drop shadows' six VU0 routines (the mobys' shadow points, the tubes' point and segment sections, their clip, the quads,
  the packets) and five of the sky's (its parts' vertices to the screen, their textured and coloured triangles' packets, the
  spheres' test, the sprites), in the same form as the culls. The shadows' transliteration reads the MAC flags after ordinary
  ops, so it runs the flagged VU0 ops: the local copy makes every op's flags too (`vu0l::VuF`, as `LocalFlags` keeps them),
  and is written back and read again around the sine / cosine VU0 programs. Proved by oracle batches 482 (shadows, through
  `shadow_points` and `drop_shadows_draw` on the frame's deferred mobys) and 483 (sky) on the dumps of seven levels, batch 79,
  the check modes over the 19 levels (one call in ten, `RAC1_HAND_CHECK_EVERY=10`: 1,321 to 69,574 calls a routine, 0
  differing), the reference images and RAMHASH. Gains are modest: the routines no longer call the helpers, but still clamp
  every operand and, for the shadows, make every flag. In the sampler (Veldin) `shadow_quads` went from 2.5 to 1.8 % of the
  main thread, `shadow_segment_draw` 2.8 to 2.2 %, `shadow_packets` 0.8 to 0.5 %, the sky 6.0 to 4.6 % (Nebula G34 4.7 to
  3.7, Pokitaru 1.9 to 1.4, Hoven 1.6 to 1.4). Measured headless (`forward`, the profiler's medians over the walk after the
  level's arrival, three interleaved runs, a shared machine): `Game: moby pass` 9 to 14 % less (Veldin 0.42 to 0.38 ms,
  Nebula G34 0.60 to 0.52, Pokitaru 0.44 to 0.40, Hoven 0.47 to 0.43; Rilgar, with few shadows, the same); `Game: world
  passes` within the noise but on Nebula G34 (1.00 to 0.91).
- **Not rewritten: the tie and shrub lighting** (0x297728, 0x288908). Measured first: with the tfrag lighting done, the stage
  that holds them and the passes' ends is 0.01-0.03 ms a frame (Nebula G34's 0.08-0.33 is the tfrag lighting); in the sampler
  the tie lighting is 0.2-0.5 % of the main thread (1.7 % on Hoven, a third of it its VU0 micro program, which a rewrite
  would not change), the shrub lighting 0.2 % or less, and neither shows at a level's load. At most 0.01-0.03 ms to win.

## The frame's chain

The MRP chain worker reads each frame's chain beside the main thread (`mrp_chain.cpp`: the 2D, the effects' VU1 programs, the
texture uploads). It took the GS replica's place in the default renderer, which walked the same chain and also assembled the
kicks of VU1 program 7 that nobody drew and the effects of the real ranges a widened view's substitute stood in for; the uploads
are now written a transfer at a time (`gs_memory.cpp`). The chain worker's busy time a host frame, headless at 5120 x 1440 with
the lighting at High, the HUD spread and the effects' substitutes (32:9), pipelined with the game thread
(`RAC1_REALTIME=1`), the last 400 frames of a forward walk: Rilgar 3.8 to 2.4 ms (and 4.2 to 3.0), Gaspar 6.2 to 5.3 (5.6 to 4.7);
the main thread's wait for it Rilgar 0.44 to 0.22 ms, Gaspar 0.95 to 0.12. What is left is VU1 program 7 and the particle program,
in the substitute (its 3D) and in the real range (the VU1 state it leaves, which the next program reads): on Gaspar 3.3 and 1.6 ms
of the 7.5 a chain took. At 4:3 there is no substitute: a chain takes 1 to 2.5 ms.

## The GPU

With a wide window and the modern lighting the frame is bound by the GPU, not the CPU (a 5053 x 1371 window, about 7 million
pixels, LOD 3, Vulkan, the lighting at High with GTAO, shadows and sun rays, on a Radeon 890M: 16 ms of GPU a frame). The
profiler's GPU stages split it: the world (ties, tfrags, shrubs, the sky) about 5 ms, the ambient occlusion 5, the sun's cascades
2.2, the mobys 1.6. What was done, each step measured alone (headless, Rilgar, `RAC1_SIM_INPUT=forward`, the median of frames
300 to 900; base and new runs taken in turn, twice):

| Stage | What changed | 5053 x 1371 | 2528 x 686 |
|---|---|---|---|
| Ambient occlusion (Vulkan) | the passes read the 3D's depth where it is, no copy ([Lighting](lighting.md#ambient-occlusion)) | Rilgar 4.09 to 3.13 ms, Kerwan 5.04 to 3.87, Veldin 4.52 to 3.74 (GTAO 1/2: 2.8 to 1.7) | Rilgar 1.18 to 0.88, Kerwan 1.55 to 1.26, Veldin 1.25 to 1.05 |
| Sun cascades | the mobys over a cascade's static cache in up to four rectangles; the far cascades' mobys every other frame, in turn ([Lighting](lighting.md#the-static-shadow-cache)) | Rilgar 2.09 to 1.28, Kerwan 2.36 to 0.63, Veldin 1.94 to 1.23 | the same (the maps do not depend on the window); OpenGL: Rilgar 3.0 to 2.0, Kerwan 2.6 to 1.2, Veldin 2.3 to 1.6 |

The whole GPU frame at 5053 x 1371: Rilgar 14.1 to 13.0 ms, Kerwan 15.6 to 12.3, Veldin 16.1 to 14.3. Both leave the picture as it
was (byte for byte, but where a far cascade's moby shadow is a frame late: at most 0.03 % of the pixels on the frames compared) and
the game's state as it was (`RAC1_RAMHASH`, 2000 frames on Rilgar, Kerwan and Veldin).

**The world: no depth pre-pass.** Every world batch blends ((Cs - Cd) As + Cd, the GS's ABE) and writes depth, so a pre-pass
that keeps the picture exact needs three steps (the depth of the fragments whose alpha is whole, then a stencil mark where a
fragment that does blend lies in front of it, then the depth put back there): built for Vulkan, it gave the same picture byte
for byte, and cost more than it saved (5053 x 1371: 6.3 to 8.8 ms). Its upper bound - a depth-only pre-pass of every world
fragment, the cheapest possible, the shading then run once per pixel - saved nothing measurable (6.3 to 6.2 ms): the world's
fragments are not drawn over each other much, its cost is about 2.2 ms that does not depend on the pixels (the vertices, the
tfrags' compute pass, the sky) and 0.6 ms per million pixels of shading. Front-to-back order cannot do better than that bound and
changes the blending's order. Not done.

**What else was tried.** A temporal GTAO (one of the three slices a frame, the others reprojected from the last frame): about
5 % less, the passes being bound by their memory traffic, not their taps. The occlusion's depth as a float copy made by a pass
of its own: no faster than reading it in place.

### GPU, round 3

Daino's profile from Rilgar (5120 x 1440, Vulkan, every effect on: Lighting High, Dynamic Shadows, Secondary Lights All, GTAO 1/2,
Surface Detail High, Ambient Particles, LOD 3) put the GPU at 16.7 ms: the world 9.4, the occlusion 2.0, the sun's cascades 1.9, the
debug overlay 1.1, the mobys 0.8, the frame capture 0.6. Measured again headless with his options at 5120 x 1440 (Rilgar and Kerwan,
`RAC1_SIM_INPUT=forward`, the median of frames 1500 to 2400, each stage also compared frame by frame against the base run's with
another stage as the clock's reference: the GPU's clock moved the stages by up to a fifth from run to run).

**Where the world's time goes.** Not in the lighting: with the lighting Original the world costs about as much per pixel (0.6 ms per
million pixels both ways at 2560 x 720 and 5120 x 1440); the point lights off, or every light walked on every pixel, or the emissive
surfaces and materials off moved it by 0 to 10 %. With a fragment shader that only writes a constant the world still cost 3.3 of its
5.2 ms (the vertices, the raster, the blending of two passes), and the ties and shrubs drawn by the GPU path are about 3.9 of it (about
1.1 of that whatever the window's size: their roughly 5 500 draw commands, each drawn twice). The alpha-tested batches (nearly every
tie and shrub: the GS's AFAIL RGB_ONLY) are drawn in two passes, the passing pixels then the failing ones.

| Change | Vulkan | OpenGL |
|---|---|---|
| The lighting's second pass of an alpha-tested batch tested LESS instead of LEQUAL (it writes no depth): the pixels the first pass drew are no longer shaded again (`RAC1_MRPW_P2_LESS=0` for the old way) | world Rilgar 5.3 to 4.7-5.0 ms, Kerwan 12.5 to 10.7-11.1 | world Rilgar 5.4 to 4.9-5.0 |
| The frame capture (the last frame shown, for the pause menu and the fades) copied only when read (`RAC1_LAZY_CAPTURE=0`) | 0.6 ms to none | (still every frame) 0.5 |
| The mobys' parts outside a sun cascade's square not drawn into it (`RAC1_SHADOW_PART_CULL=0`) | cascades Rilgar 1.7 to 1.5, Kerwan 2.2 to 2.0 | cascades Rilgar 2.2 to 1.9-2.0 |

The whole GPU frame on Vulkan: Rilgar 11.9 / 12.6 to 10.8 / 11.5 ms (two pairs of runs), Kerwan 18.9 / 18.7 to 16.9 / 16.3. The
second change and the third leave every picture as it was (byte for byte: Rilgar, Kerwan, Veldin, Batalia at 2560 x 1440 and 5120 x
1440, OpenGL and Vulkan, the pause menu's frames, the Vendor's screens) and the game's state as it was (`RAC1_RAMHASH`). The first is
not exact: a failing pixel lying exactly on the depth another surface wrote (coplanar faces) is no longer drawn over it: 0 to 51 pixels
of the 7.4 million in those shots, by at most 26 levels of 255, single scattered pixels on foliage; the original lighting keeps
LEQUAL. The stored picture of the pause menu (in EE memory) carries those pixels too.

**Kept on purpose, to review.** Daino chose to keep the strict pass on by default for its gain (27 October 2026),
knowing it is not exact. If it ever shows more than scattered pixels (holes in foliage, flicker on decals or on coplanar
faces, a visible difference in the pause menu's picture), it becomes an option or is reviewed: `RAC1_MRPW_P2_LESS=0`
gives the exact picture to compare.

The ambient occlusion's half resolution is now offered for every mode (SSAO 1/2, SSAO+ 1/2, HBAO 1/2 beside GTAO 1/2: about 45 %
less on Vulkan, 10 to 35 % on OpenGL, whose depth copy does not shrink), the full resolution staying the default
([Lighting](lighting.md#ambient-occlusion)).

**Tried and dropped.** The alpha test decided before the lighting in the world's shader (a fragment the pass discards then costs its
texel, not its lights): the same picture, no gain beyond the noise (0 to 5 %). The early fragment tests for the second pass: no gain
(the driver already tests early where nothing is written). Strict tests without the early fragment test gave the same gain as with it.
Light culling per tile or cluster beyond the existing 32 x 18 tiles: the lights cost too little per pixel to pay. The half
resolution's occlusion blurred at its own size with a depth-aware upsample of its own: 6 % less than GTAO 1/2's on Vulkan, more on
OpenGL, a slightly different look. The debug menu's Vulkan ImGui pass copies the whole window twice to flip it; copying only the
windows' rectangles cost more with the menu open (a copy and its barriers per window): left as it was. The depth pre-pass was
measured before (above).

## Found while profiling a low-end target

Changes found while profiling the port on a much slower machine, where these costs showed; each is on by default, gives the
same picture and game state as before (the reference images, `RAC1_RAMHASH` over a four-level walk, and the modern lighting with
Surface Detail, Water and Grass at High on Veldin, Novalis, Rilgar and Pokitaru, OpenGL and Vulkan, all identical), and has a
`RAC1_PERF_*=0` switch to compare.

- **The world's alpha test** (`RAC1_PERF_OPAQUE`, `mrp_world_renderer.cpp`): a world draw whose pixels cannot fail the alpha test
  (see [Rendering](rendering.md)) is drawn in its batch's first pass with early depth testing and left out of the second pass. The
  batch's draws keep their order (a batch is split into runs of draws that can fail and draws that cannot). `RAC1_PERF_OPAQUE=2`
  draws each run held opaque again counting its failing pixels: 0 in all the runs above. The world's GPU time, headless,
  `RAC1_SIM_INPUT=forward`, the last 600 frames: Novalis 0.97 to 0.81 ms (OpenGL) and 1.24 to 0.88 (Vulkan), Pokitaru 1.85 to
  1.36 and 1.94 to 1.37; the Vulkan world stage's CPU time Pokitaru 0.96 to 0.50 ms (half the second-pass draws gone).
- **The chain reader's GS memory** (`RAC1_PERF_GS_MEM`, `gs_memory.cpp`, `mrp_chain.cpp`): in-page tables for the 16, 8 and 4-bit
  addresses (as the 32-bit one had), the address made once in the 8 / 4 / 16-bit stores, an indexed texture's CLUT decoded once per
  texture instead of per texel, the last 16 images of a texture key kept (an effect's or the HUD's textures taking turns at one
  address keep their image id, so the GPU texture made from it), the DMA tags' data read in place. The chain worker's busy time:
  Novalis 0.99 to 0.45 ms a frame, Pokitaru 1.53 to 0.85.
- **The VU1 port lookup** (`RAC1_PERF_VU1_LOOKUP`, `vu1.cpp`): after each MPG the first start of a program hashed every registered
  port's range holding it (up to ~69 KB for program 7's starts, ~470 KB a frame on Novalis); each port now keeps the last four
  images of its range with their answer and compares instead. The chain worker: Novalis 0.77 to 0.45 ms, Pokitaru 1.05 to 0.85.
- Smaller ones, below the noise of a shared machine here (< 0.1 ms): the XGKICK's packet read in place in VU1 memory
  (`RAC1_PERF_XGKICK`), the static level's texture table kept in the scene's table instead of copied three times a frame
  (`RAC1_PERF_TEXTABLE`), the tfrag lists' storage kept (`RAC1_PERF_TFRAG_ALLOC`), program 7's per-vertex work done once per vertex
  instead of per triangle corner and the effects' Q from the w row alone (`RAC1_PERF_VU7`), and a tie / shrub packet repeated for
  the next instance reusing the previous one's runs when the GS state it reads is unchanged (`RAC1_PERF_PKT_MEMO`; `=2` compares
  every reuse with the full replay: 0 differences over 3.2 million reuses).

All together (headless, Radeon 890M, a shared machine: two runs each, the last 600 frames of a forward walk): the chain worker
Veldin 0.46 to 0.20 ms, Novalis 1.17-1.33 to 0.45-0.54, Rilgar 1.07-1.25 to 0.56, Pokitaru 1.74-2.0 to 0.91-1.04; the world's GPU
time Veldin 1.56 to 1.45 ms, Novalis 1.03 to 0.83, Rilgar 1.85 to 1.40, Pokitaru 1.94 to 1.34 (OpenGL; Vulkan alike).

### Second round

Found the same way, each with a `RAC1_PERF_*=0` switch: the game's own code where a host form gives the same values (proved by
`RAC1_RAMHASH` on the levels that run it and by the oracle batches of the hand-ported functions), and the sky drawn after the
terrain.

- **The palette blend** (`RAC1_PERF_PALETTE_BLEND`, `src/game/ovl/palette_blend.cpp`, hooked by `tools/codegen/hooks.glue` into the
  six overlays' copies of the routine: Kerwan 0x23c428, Rilgar 0x277b20, Umbris 0x2762e0, Batalia 0x257d40, Gaspar 0x26d240, Oltanis
  0x255ce0, NTSC-U): its texel loop weighted both palettes' entries (VMULbc) for each of 4096 texels; each entry's product is now
  made once (256 per palette) and the texels add the two they index, leaving the VU0 registers as the loop does (not its ops' flags,
  which nothing reads after). `RAC1_RAMHASH` identical on and off on levels 3, 5, 7, 8, 9 and 14 (3000 frames each, the routine
  running on all six). Kerwan, the routine's call (the palettes' DMA and conversion included): 0.124 to 0.035 ms, about one call a
  frame. The PAL build runs the loop as transliterated. No oracle batch: the routine's scratchpad DMA is not in the interpreter.
- **The VU clamp once per operand** (`RAC1_PERF_VU_CLAMP`, `ovl_1e_1f.cpp` `segment_collide_v`'s triangle test, `ovl_24_25.cpp`
  `moby_transform_core`): the hand ports' `q*` / `vmul` / `vmadd` helpers clamp both operands and the result of every op by the VU
  rules; where every operand is already a VU result, a small integer's float or a value clamped as it is read, `vuc(x)` is `x`, so
  only the result is clamped. Exact by construction; `RAC1_RAMHASH` identical (levels 0, 1, 3, 5, 8) and the oracle batches 38, 41,
  43, 89 (`moby_update_transform`, `_kept`, `segment_collide`) and the level batches that run them pass.
- **The moby collision mesh cache** (`RAC1_PERF_COLL_MESH`, off by default; `ovl_1e_1f.cpp` `segment_collide_v`): a moby's collision
  vertices turned into the world (three VU0 multiply-adds each) depend only on its rows', its scale's bits and the vertex words, so they
  are kept per moby with those bits and a copy of the words and used again while all are the same (the outcodes, which depend on the
  segment, are still made per call). Exact (`RAC1_RAMHASH` identical on levels 0, 1, 3, 5, 8 and the four-level chain with it on), and
  95% reuse on Kerwan, but the moby part of the segment test runs about four times a frame here: 5.2 to 3.1 microseconds a use, about
  0.01 ms a frame. Left off; worth it only where the game thread is much slower.

- Smaller ones (`RAC1_PERF_COPIES`): the EE memory journal appends a saved 4 KB page as it is instead of zero-filling it first and
  copying over it (`ee_memory.cpp` `journal_touch`), and the MRP moby observer builds its ~650-byte record in place in the frame's
  list (`mrp_scene.cpp` `on_draw`). Below the noise here (the journal now saves few pages, the observer sees a moby or two a frame);
  the same picture and game state.
- **The Euler conversion cache** (`RAC1_PERF_EULER_CACHE`, off by default; `vecmath.cpp` `euler_to_mat3_vu`): its rows depend only on
  the three angles' bits, so a 512-entry table per thread returns them again for a moby whose rotation did not change. Exact
  (`RAC1_RAMHASH` identical with it on, levels 0, 1, 3, 5, 8 and the chain), but on this PC a conversion is already about 0.1
  microseconds: Kerwan ~400 calls a frame, 0.112 to 0.096 microseconds each (the timer included), a few microseconds a frame. Left off.
- **VU0 ops without their flags** (`RAC1_PERF_VU0_NF`, `src/game/ovl/vu0_nf.h`, `tools/codegen/vu0_nf_pass.py` run by `generate_all.py`):
  a `vu0_regs.h` op builds the MAC and status flags of every result (the larger part of its work). The pass gives `vu0nf`'s ops -
  the same lanes, clamps and ACC, no flags - to every generated file whose CFC2 reads of the status / MAC flags all follow, in
  straight-line code, an op into vf0 (those ops keep their flags: `vu0nf` sends a vf0 destination to `vu0m`): 176 of the generated
  files (the world's lighting, the levels' program-7 surfaces, `sky_ee`, `world_cull`, the transliterated classes). A build
  defining `RAC1_VU0_EXACT` keeps `vu0m`'s ops. `RAC1_RAMHASH` identical on and off on every level 0-18 (2000 frames each) and the
  chain; the oracle batches 79, 101, 102, 110, 113, 126 (L1), 112, 138, 190, 192 (L3), 140 (L5), 180 pass. The game code a frame
  (main thread, the last 600 of 1860 frames, three runs each): Veldin 1.25-1.26 to 1.17-1.19 ms, Kerwan 1.82-1.83 to 1.72-1.83,
  Rilgar 1.43-1.46 to 1.35-1.51.
- **The sky after the terrain** (`RAC1_PERF_SKY_LATE`, off by default; `mrp_world_renderer.cpp`, OpenGL and Vulkan): the sky's shells
  come first in the frame and cover the screen, so every pixel the terrain then covers is shaded and blended for nothing. With it on,
  the frame's leading tfrag batches whose first pass should write the source alone (every run held opaque by `RAC1_PERF_OPAQUE`,
  blended as ALPHA 0x44 or not at all, the depth tested and written) are drawn first, then the sky with the depth test LEQUAL at depth
  1.0 (the depth range [1, 1]: it passes only where nothing was drawn), then the rest as before. Used on most frames (Veldin 1536 of
  1800, 6 leading batches; Novalis, Kerwan, Eudora alike). Not exact: the opaque state comes from the tfrag entries' colour words, but
  the vertex alphas the tfrag programs make from them can end just below 0x80 (a run held opaque still never fails the alpha test,
  GEQUAL 0x60, but its As can be just below 1), and there a trace of what lay behind shows through - the sky before, the cleared
  colour now: on Veldin's reference frame 51 pixels on the terrain's edge at the horizon, one level of one channel (the reference
  md5s change with it on). Nor measurably faster on this GPU (headless, the last 600 of 1860 frames, world GPU ms off / on): 1920x1080
  Veldin 1.28 / 1.20 (OpenGL), 1.22 / 1.31 (Vulkan), Novalis 1.30 / 1.01, 1.03 / 1.03, Kerwan 1.68 / 1.70, 1.69 / 1.71, Eudora 1.38 /
  1.37, 1.30 / 1.38; 5120x1440 (Fill Screen) Veldin 3.04-3.28 / 2.93-3.11 and 3.30-3.34 / 3.29-3.35, Novalis 2.62-2.83 / 2.62-2.80
  and 2.72-2.74 / 2.72-2.75, Kerwan 3.06 / 3.07 and 3.30 / 3.29, Eudora 3.58 / 3.57 and 3.89 / 3.94. Left off, for a GPU where the
  sky's fill rate costs.

## Switches to compare

These are testing variables: they are honoured in a headless run, or in a window started with `--debug-tools`
(see [Environment variables](env-vars.md#when-a-variable-is-honoured)).

| Variable | Off gives |
|---|---|
| `RAC1_GAME_THREAD=0` | the game frames on the main thread before the drawing |
| `RAC1_MRPW_BUILD_THREAD=0` | the MRP world's triangles built on the main thread at the drawing |
| `RAC1_VU0_SIMD=0` | the pose's loops op by op on the VU0's state |
| `RAC1_AO_DEPTH_COPY=1` | the ambient occlusion reading a copy of the depth (Vulkan) |
| `RAC1_SHADOW_CLUSTERS=1` | one rectangle around every moby over a cascade's static cache |
| `RAC1_SHADOW_FAR_EVERY=0` | the far cascades' mobys as before (the farthest every other frame, the middle one every frame) |
| `RAC1_SHADOW_PART_CULL=0` | every moby part within the shadow distance drawn into every sun cascade |
| `RAC1_LAZY_CAPTURE=0` | Vulkan: the last frame shown copied every frame |
| `RAC1_MRPW_P2_LESS=0` | the lighting's second pass of an alpha-tested batch tested LEQUAL (the original's picture: see above) |
| `RAC1_PERF_OPAQUE=0` | every world draw in both alpha-test passes, with the discard's late depth test |
| `RAC1_PERF_GS_MEM=0`, `RAC1_PERF_VU1_LOOKUP=0`, `RAC1_PERF_XGKICK=0` | the chain reader's GS memory, VU1 port lookups and kicks as before |
| `RAC1_PERF_TEXTABLE=0`, `RAC1_PERF_TFRAG_ALLOC=0`, `RAC1_PERF_VU7=0`, `RAC1_PERF_PKT_MEMO=0` | the scene's and the effects' preparation as before |
| `RAC1_PERF_PALETTE_BLEND=0` | the six levels' palette blend as the game's loop |
| `RAC1_PERF_VU_CLAMP=0` | the collision triangle test and the moby transform clamping every operand at each op |
| `RAC1_PERF_COLL_MESH=1` | the collision segment test's moby meshes kept between calls (off by default) |
| `RAC1_PERF_COPIES=0` | the EE journal's pages zero-filled then copied, the moby observer's record built aside and copied |
| `RAC1_PERF_EULER_CACHE=1` | the Euler conversion's rows kept per angles (off by default) |
| `RAC1_PERF_VU0_NF=0` | the generated code's VU0 ops with their flags |
| `RAC1_WORLD_CULL_FAST=0` | the world passes' three culls as transliterated (`=2`: both, compared at every call) |
| `RAC1_DROP_SHADOWS_FAST=0`, `RAC1_SKY_FAST=0` | the drop shadows' and the sky's routines as transliterated (`=2`: both, compared; `RAC1_HAND_CHECK_EVERY` to check one call in n) |
| `RAC1_PERF_SKY_LATE=1` | the sky after the leading opaque tfrag batches (off by default: not exact) |
| `RAC1_MRPW_THREAD=0`, `RAC1_GS_THREAD=0`, `RAC1_JOBS=1` | the scene, the frame's chain's reading (the MRP's chain reader, or the GS replica), the job pool on the main thread |

Each gives the same picture and the same game state: only the time changes (`RAC1_SHADOW_FAR_EVERY` also when a far cascade's
moby shadows are drawn; `RAC1_MRPW_P2_LESS` a few pixels).

## Measuring

The profiler's CSV (`RAC1_PROFILER_CSV=<file>`) has, per host frame, its period, the game frames it ran (`steps`), the main
thread's CPU time, every stage's CPU and GPU time, and the workers' busy and CPU time (the game thread's: `busy: Game thread`;
its stages are counted in the stage columns, the main thread's wait for it in `Wait: game thread`). Two ways to use it
headless (the window's size and the Options as a player has them: a copy of `graphics_options.txt` in a directory given by
`XDG_CONFIG_HOME`, with `RAC1_ADVANCED=1`):

- **Throughput**: a deterministic headless run with `RAC1_PIPELINE=1` (and `RAC1_GAME_THREAD=2` for the game thread) runs one
  game frame per host frame as fast as it goes: the period is the cost of a game frame, the same game frames in every run.
- **As a player**: `RAC1_REALTIME=1 RAC1_FPS_CAP=0 RAC1_AUDIO_DEVICE=null RAC1_PIPELINE=1` steps the game by the wall clock
  and paces as a window does: the `steps` column's distribution is how often a host frame ran two game frames.

The GPU's stages (`gpu: ...` columns) are best compared as medians of a deterministic run's frames (a hitch in one frame
does not move them), and between runs taken in turn: the GPU is shared with every other program drawing on the machine, and a
stage's time moved by a third between two runs an hour apart.

`taskset -c 0-3,12-15` keeps a run on the fast cores of a CPU with two kinds (the Ryzen AI 9 HX 470's four 5.3 GHz cores and
their second threads); another program's load on the same cores shows in every number.

The game thread's report at the end of a run (`[game-thread]`) gives its jobs' mean start and join latencies: both hand-offs
are polled (the main thread for the job's end; the game thread for its next job when the jobs come less than 4 ms apart), as a
sleeping thread's wake-up took milliseconds on a loaded machine.

## Proving a change

A change of the host's work around the game is proved to leave the game as it was by `RAC1_RAMHASH` (EE RAM and scratchpad,
page by page, every game frame) against the build before it, on long runs over several levels with a player's Options (the LOD
re-runs, the modern lighting), and by the reference images. The game thread's own check: `RAC1_GAME_THREAD=2 RAC1_PIPELINE=1`
in a deterministic headless run gives the hashes of `RAC1_GAME_THREAD=0`. A change of game code adds its oracle batch (the
pose's loops: batch 470's `joints_simd`, and batch 79 / 93 against the original).

Thread safety is checked with a ThreadSanitizer build, kept apart from the game's build:

```sh
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS=-fsanitize=thread \
      -DCMAKE_C_FLAGS=-fsanitize=thread -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
cmake --build build-tsan --target rac1-native -j 3
TSAN_OPTIONS=log_path=tsan RAC1_GAME_THREAD=2 RAC1_PIPELINE=1 RAC1_HEADLESS=1 RAC1_FRAMES=1100 \
      ./build-tsan/rac1-native --debug-tools --level 5
```

The coroutine's stack switches are told to ThreadSanitizer as fibers (`src/platform/platform_posix.cpp`). Its reports from the
GL driver's and GLib's own threads are not ours. `RAC1_DRAW_MEM_CHECK=1` names any read of the game's memory a pipelined
frame's drawing makes outside the copy taken at its preparation.

Memory errors and leaks are checked the same way with an AddressSanitizer + UndefinedBehaviorSanitizer build
(`-fsanitize=address,undefined -fno-omit-frame-pointer`, `-O1`). The game ends with `std::_Exit`, which skips
LeakSanitizer's check at exit, so such a build runs it just before (`__lsan_do_leak_check` in `main`). The generated
sources compiled with the sanitizers take more than half an hour each and over 10 GB with three jobs; a compiler launcher
that leaves `-fsanitize` out for the files under `assets/generated/` keeps the build to a few minutes (they are not
ours to fix). On 2026-10-27 a run through five levels, the three reference runs and the options-on runs on OpenGL and
Vulkan reported no memory error and only the drivers' and Wayland's own allocations (under 1.5 KB) as leaks.
