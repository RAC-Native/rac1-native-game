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
| 4 | this frame prepared: the view, the GS chain captured, the world's scene started, the moby draw lists | the GS chain worker; the MRP scene worker (the scene, then its triangles: `RAC1_MRPW_BUILD_THREAD`) |
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

## Switches to compare

| Variable | Off gives |
|---|---|
| `RAC1_GAME_THREAD=0` | the game frames on the main thread before the drawing |
| `RAC1_MRPW_BUILD_THREAD=0` | the MRP world's triangles built on the main thread at the drawing |
| `RAC1_VU0_SIMD=0` | the pose's loops op by op on the VU0's state |
| `RAC1_AO_DEPTH_COPY=1` | the ambient occlusion reading a copy of the depth (Vulkan) |
| `RAC1_SHADOW_CLUSTERS=1` | one rectangle around every moby over a cascade's static cache |
| `RAC1_SHADOW_FAR_EVERY=0` | the far cascades' mobys as before (the farthest every other frame, the middle one every frame) |
| `RAC1_MRPW_THREAD=0`, `RAC1_GS_THREAD=0`, `RAC1_JOBS=1` | the scene, the GS chain, the job pool on the main thread |

Each gives the same picture and the same game state: only the time changes (`RAC1_SHADOW_FAR_EVERY` also when a far cascade's
moby shadows are drawn).

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
