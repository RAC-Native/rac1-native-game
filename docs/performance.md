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
  A flat guest mapping (EE address = host base + address, no range test: what OpenRAC does over a 4 GB reservation) was
  weighed rather than built: the accessors' fast path is already a compare and an add on two globals the L1 cache holds, and
  forcing it inline - which removes the calls, a bigger saving - gave nothing beyond the noise. A counting build sees about
  770,000 accesses a frame on Kerwan (260,000 writes, 510,000 reads, of them 200,000 of the scratchpad); a cycle or two saved on
  each is at most 0.2 ms at best, unmeasurable here, while the null, scratchpad, DMA-register and journal cases would need guard
  pages and a fault handler.
- **The accessors declared inline, not forced** (CMake `RAC1_ACCESSORS_INLINE`, on by default; November 2026): what lies
  between the two. `ee_read` / `ee_write` were templates not declared `inline`, with a `throw` in their body: that, not their
  size, kept them out of line. They are now declared `inline` with the null address's error out of line and cold - their
  body is the pointer and the copy - and the compiler chooses per call site; and a quadword's load or store (`lq` / `sq`,
  `ee_regs.h`) is one 16-byte access, not two doublewords (its address is aligned: one page). The same C++, so the same game
  state: `RAC1_RAMHASH` identical to the build before on the 19 levels at 4:3 and widened to 16:9 with LOD High on the game
  thread (the scene hash too), the reference images, the oracle's reference batches. Measured on top of the float helpers'
  raw forms (below; the same method, three runs of each taken in turn): the game's code Veldin 0.91 to 0.79 ms a frame,
  Kerwan 1.52 to 1.33, Rilgar 0.90 to 0.82, Blarg Station 1.05 to 0.96, Hoven 1.16 to 0.99 (9 to 15 % less); `Game: moby
  joints / skinning` 20 to 37 % less, `Game: world passes` 16 to 20 %, `Game: moby updates` 5 to 13 %. The program's code
  38.2 to 39.7 MB (4 % more), a full build with four jobs 8.5 minutes as before. `-DRAC1_ACCESSORS_INLINE=OFF` gives the
  accessors as they were. Not done: the `is_hw` test (a hardware register's address) left out of `lw_` / `sw_` where the
  address is a constant or relative to the stack pointer (35,000 and 15,000 of their 152,000 sites: a change of the
  generators), and the stack frames' spills (a third of the generated code's 254,000 access sites are stack-relative,
  21,000 of them the prologues' saves of registers that are locals of the routine).
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

## The float helpers' raw forms

The helpers that apply the PS2's float rules did so by hand at every operation: the EE FPU's `fpu_add` / `fpu_sub` /
`fpu_mul` (`ovl_common.h`: one call for each of the generated code's 31,000 COP1 adds, subtracts and multiplies) through
doubles with three classifications, the VU0 macro ops with a clamp of every operand and of the result. But the thread they
run on is in the EE's float mode already (`runtime/ee/ps2_fpu.h`: toward zero, flush-to-zero, denormals-are-zero), and there
the host's own operation gives those rules' result: a denormal operand reads as the signed zero, a denormal result comes
back as the signed zero, a result past the range as the signed largest float. Only an operand with exponent 255 differs - a
number on the PS2, an infinity or a NaN on the host - and the host's result then has exponent 255 too, whatever the other
operand. So a helper does the plain operation, and takes its rule-by-rule form (the code it had, out of line) only when the
result has exponent 255. With `RAC1_FPU_NEAREST=1` the thread is not in that mode, and the rule-by-rule forms stay.

**The EE FPU** (`RAC1_PERF_FPU_RAW`; `=0`: the rule-by-rule forms, `=2`: those, each compared with what the raw form would
give, the account at the end of the run): `fpu_add`, `fpu_sub` and `fpu_mul` are `s op t` and a test of the result's
exponent. `fpu_div` is as it was (the division's own cost, and its rounding to nearest). Proved:

- `tests/fp/fp_raw_equiv.cpp` (CMake `RAC1_FP_TEST`; no disc, no generated code): each helper run with the raw form off and
  on, on the same operands, the result compared bit for bit - random words mixed with the edges (exponents 0, 1 to 3, 252 to
  255 and around 127, mantissas zero, one bit, all ones; pairs made to cancel or to land at either end of the range): 540
  million cases of the three, 0 differing. With the flush-to-zero bit cleared the same test reports differences at once.
- `=2` in the game: 245.7 million operations compared over the 19 levels (2000 frames each at 4:3, 900 widened to 16:9 with
  LOD High on the game thread), 0 differing, 0 left to the rules, none on a thread outside the EE's mode; in the
  oracle's batches the same (batch 63: 4,952 compared, 2 left to the rules).
- `RAC1_RAMHASH` identical to the build before on the 19 levels, at 4:3 and widened; the reference images; the oracle's
  batches (63, 79, 93, 101, 118, 125, 141, 470 on Veldin; 480 to 483 on their seven levels; 112, 138, 140, 91, 120, 44 and 50
  on their own) pass with it on, off and at `=2`.

Measured headless (4:3, `RAC1_PIPELINE=1`, forward, the profiler's medians over the last 600 of 1860 frames, three runs of
each taken in turn on the fast cores), against the same build with it off: the game's code within the noise on Veldin
(0.901 to 0.907 ms a frame) and Hoven (1.189 to 1.184), 2 to 5 % less on Kerwan (1.544 to 1.518), Rilgar (0.950 to 0.925)
and Blarg Station (1.107 to 1.053). A frame's FPU operations are few beside its VU0's (below): on Kerwan 21 million against
296 million in 2000 frames.

**The VU0 macro ops** (`RAC1_PERF_VU0_RAW`, the same values): the adds, subtracts, multiplies and multiply-adds in every
form - `vu0_macro.h`'s lane ops, `vu0_regs.h`'s ops with their flags (on the VU0's state and on a `LocalFlags`), `vu0_nf.h`'s
without, `vu0_local.h`'s on a routine's local copy - clamped every operand and the result, about nine SSE instructions each
around one `mulps` or `addps`. They now compute the lanes as they are, and compute them again by the rules, out of line,
only when a lane the op writes has exponent 255. An op that makes flags makes the zero and the sign bits from the raw
result: in the thread's mode the rules never set the underflow or the overflow bit either (their operands clamped, no result
is a denormal or has exponent 255). Divisions, square roots, conversions, `VOPMULA`'s lane loop are as they were. Proved:

- `tests/fp/fp_raw_equiv.cpp`: each op run with the raw form off and on from the same VU0 state - every lane mask, vf0 as a
  destination, an operand that is the destination, ACC near the opposite of the product - the registers, ACC, Q and the
  flags compared: 2.7 billion lane results with the FPU's, 0 differing; rounding to nearest, the flags differ at once.
- `=2` in the game: 8.4 billion ops compared over the same runs, 0 differing, 8,207 left to the rules, none on
  a thread outside the mode; in the oracle (batch 79: 12.4 million compared, 53 left to the rules).
- `RAC1_RAMHASH` identical to the build before on the 19 levels at 4:3 and widened with LOD High on the game thread (there
  `RAC1_MRP_SCENE_HASH` identical and `RAC1_MRP_LOD_VERIFY=1` without a difference too); the check modes of the routines
  written by hand, which compare their local-copy ops with the transliterations' (`RAC1_WORLD_CULL_FAST=2`,
  `RAC1_PERF_MOBY_DRAW=2`, `RAC1_DROP_SHADOWS_FAST=2`, `RAC1_SKY_FAST=2`, `RAC1_WORLD_LIGHTING_FAST=2`,
  `RAC1_PERF_POSE_MEMO=2`): 0 differing on the 19 levels; the oracle's batches as above; the reference images.
- Not run on AArch64 (no cross compiler or emulator on the development machine): the forms use only intrinsics the NEON
  shim (`runtime/vu/vu_simd_shim.h`) already has, and `tests/fp/fp_raw_equiv.cpp` builds there as it is.

**The pose's loops** (`moby_joints.cpp`, the same switch): the four hot loops keep their registers in SIMD locals and clamped
operands and results themselves. Each step - a listed vector, a joint's matrix, a bind matrix; the rotations' loop whole - is
now written once for two policies and run raw when nothing it reads from memory, or from the registers the loop found, has
exponent 255 (the record and the parent's matrix are read before the joint's first op: no op of a joint writes memory), by
the rules otherwise. Oracle batch 470's new `joints_raw` runs the loops per-op, SIMD and AVX2 with the raw forms on against
the per-op form by the rules: 8000 cases, a third of them with such an input, 0 differing (with one finiteness test taken
out, 1311 fail). `=2` runs the loops by the rules.

Measured as above, the build before -> the FPU's and the VU0's raw forms, ms a game frame:

| | Veldin | Kerwan | Rilgar | Blarg Station | Hoven |
|---|---|---|---|---|---|
| The game's code | 1.13 -> 0.91 | 1.89 -> 1.52 | 1.16 -> 0.93 | 1.38 -> 1.05 | 1.52 -> 1.18 |
| `Game: world passes` | 0.43 -> 0.27 | 0.64 -> 0.37 | 0.36 -> 0.21 | 0.48 -> 0.32 | 0.62 -> 0.35 |
| `Game: moby pass (draw loop)` | 0.23 -> 0.19 | 0.26 -> 0.21 | 0.11 -> 0.08 | 0.27 -> 0.21 | 0.22 -> 0.19 |
| `Game: moby joints / skinning` | 0.088 -> 0.072 | 0.139 -> 0.108 | 0.106 -> 0.083 | 0.049 -> 0.039 | 0.135 -> 0.104 |
| The main thread's CPU | 1.73 -> 1.53 | 3.19 -> 2.81 | 1.93 -> 1.72 | 2.30 -> 2.00 | 2.44 -> 2.12 |

The game's code a fifth less (19.5 to 23.9 %), the world passes 37 to 43 % less. The same binary with both switches at 0 is
7 to 12 % slower than the build before (the rule-by-rule forms are a call further away now): the figures are against the
build before, not against `=0`.

## The widened view's and the LOD slider's shadow runs

With the view wider than the game's 4:3 frustum (16:9 and wider, the sides filled) or the LOD slider off Original, the moby pass,
the effects block, the draw lists and the sky run a second time each frame as a shadow inside a journal of EE memory
(`mrp_lod.cpp`), and the MRP draws what the shadow saw. On Kerwan's plaza at 16:9 with LOD High (game thread, the profiler's
means): the moby pass's shadow 1.1-1.5 ms a frame (282 mobys drawn against the real pass's 48: the slider's distances), the
others 0.45 (the effects 0.27, the sky 0.08); with LOD Original 0.50 and 0.50 (57 mobys). The world's tfrag / tie / shrub
shadows do not run: the static world culls itself.

The journal saved each 4 KB page at its first write and put it back after: 668 pages a frame there (2.7 MB copied twice), two
thirds of them the shadow runs' own room above the game's 32 MB (their packets, packet ring and work area), which nothing reads
after the run. That room is now left out of the journal (`ee_journal_unsaved`; `RAC1_MRP_LOD_ROOM_JOURNAL=1`: as before): 227
pages a frame, the moby shadow 1.53 to 1.20 ms, the game thread 6.7 to 6.1 ms (three interleaved runs, the sampler's scene).
Exact: `RAC1_MRP_LOD_VERIFY=1` (EE RAM below the room and the scratchpad compared after each shadow run: 2999 runs, 0
differing), RAMHASH on the 4-level route with the view widened and LOD High identical (and identical to the 4:3 one), the
widened pictures identical (OpenGL, Vulkan).

The MRP scene worker (the bottleneck there: 3.8-4.6 ms busy, the main thread waiting 2.4-3.2 ms for it) spent 7 % of its time in
`vu1::code_hash`: after every MPG in the frame's chain the world walker asked which world program the micro memory holds
(`program_of`, mrp_world_check.cpp), hashing up to six programs' code a byte at a time. The last eight images met are now kept
with their answer and compared (`memcmp`) instead (`RAC1_PERF_VU1_LOOKUP=0`: as before): the hash gone from the profile, the
worker 3.77 to 3.67 ms and the wait 2.60 to 2.36 ms (three interleaved runs). Exact by construction; the reference images,
RAMHASH and the widened Kerwan picture identical.

The same worker spent 4 % in `tie_vert_opaque` (RAC1_PERF_OPAQUE's per-instance test of a tie's vertex alpha): its table of
every tie's lowest alpha was made again whenever the resident colours were updated, which is every frame; it is now made again
only when their colours changed (the mirror's count of changed instances; `RAC1_PERF_OPAQUE_KEEP=0`: as before): the worker
4.04 to 3.76 ms, the wait 2.70 to 2.45 ms (three interleaved runs). The same table, so the same draws; the pictures of
Kerwan, Rilgar and Hoven widened identical, the reference images and RAMHASH too.

And 7-10 % in `WorldIndex::find_tfrag` and its map: the walker looked up each tfrag draw's entry (a hash of the image VU1 memory
holds, a map search) though, with the GPU path making the tfrags from the host's decisions, that draw only returns; the lookup is
now made only where it is used (the CPU path, the checks; `RAC1_PERF_TFRAG_FIND=0`: always): the worker 2.71 to 2.60 ms on a
quieter machine (three interleaved runs). The same pictures (Kerwan, Rilgar, Hoven widened, the references), RAMHASH.

Two cheaper forms were measured and left (Kerwan, the same scene, a counting build): a finer journal - of the pages it still saves
(about 230 a frame), 75 % change at all, and of their 4 KB 46 % in 256-byte blocks, 23 % in 64-byte ones: blocks of 256 bytes
would copy half as much, about 0.08 ms a frame at best, against a seen-flag table 16 times larger on every journaled write; and
guard pages (the journal's write test replaced by a page fault at a page's first write): a fault and an mprotect cost more
(2-5 us) than copying the page (under 1 us), so it would be slower. The test itself (two byte loads on each of the ~150,000
writes a frame inside the shadow runs) is a few hundredths of a millisecond.

### The second moby pass: the pose memo, the observers' reads, the distances outside the journal

Measured on Kerwan's plaza at 16:9 widened (game thread, LOD High, main's build, sampler): the moby pass's shadow drew 282
mobys a frame against the real pass's 48, and its time went to the poses 0x250fe0 (33 %), the draw loop (29 %), the MRP's
observers of the pass (17 %), the scene's decode (10 %) and the journal (5 %; 12 % at LOD Original). Three changes, each exact
by construction and each behind a switch with a check mode:

- **The pose memo** (`render/mrp_pose_memo.cpp`, `RAC1_PERF_POSE_MEMO`). Most of the mobys LOD High adds are far and not
  animating: their pose is the same frame after frame (a gdb count over Kerwan, Rilgar and Veldin: 95 % of the second pass's
  posed mobys, 40-89 % of its posed joints). The memo keeps, per moby and model, every byte the pose reads (the job's 0x40
  bytes, what its toSPR chain brings - the joints' rest data and keyframes -, the blend channels with their keys, the
  additive layers, the bind matrices) and the joint matrices it left; when those bytes are the same again, the matrices go on
  the scratchpad and the pose is not run. A key of the job alone would not do: 0.1-0.5 % of those poses differ through the
  additive layers (a head turning). It works only inside the second pass, whose memory, scratchpad and VU0 are put back after
  it, and only for a record whose skinning the MRP skips (the skinning would read the rest of the pose's state); the MRP reads
  only the matrices. `=2` runs the pose on every hit too and compares: 628,935 hits over the 19 levels widened at LOD High,
  0 differing. Hits on the 19 levels' walks: 40-88 % of the joints at LOD High (Umbris 8 %, Drek's fleet 23 %), 0-58 % at LOD Original.
- **The observers' reads** (`RAC1_PERF_SCENE_READS`): the draw loop's and the pose's observers (`mrp_scene.cpp`) copied the
  record a word at a time, the vectors a float at a time and the joint matrices 16 floats a joint through the out-of-line
  accessors; each range is now one pointer and a memcpy (the same bytes; `=2` reads both ways and compares: 24.6 million
  ranges over the 19 levels, 0 differing). The real pass gains too.
- **The distances outside the journal** (`RAC1_PERF_LOD_PATCH`): `patch_moby` wrote +0x32 / +0x72 of every moby through the
  journal, so every page of the moby list was saved and put back each frame - at LOD Original too, where it writes the same
  values. It now writes none at a scale of 1 (`=2` checks that none would change: 0 over the 19 levels), and otherwise writes
  the changed fields straight into RAM and puts them back after the journal's restore (a page the pass writes is saved with
  the patched value, restored to it, then the undo puts the original back). The drawn flags (+0x31) the pass left are read
  straight from RAM as well.

On the same scene (five interleaved runs each, the profiler's means; a shared machine):

| ms a frame | LOD High: before | all three | without the memo | LOD Original: before | all three |
|---|---|---|---|---|---|
| the moby pass's shadow (its own timer) | 2.16 | **1.52** | 1.91 | 0.68 | **0.54** |
| profiler `moby joints / skinning` (both passes) | 1.34 | 0.55 | 1.19 | 0.32 | 0.25 |
| game thread busy | 7.48 | **6.87** | 7.19 | 5.54 | **5.22** |

The memo is about 0.4 ms of it at LOD High, the reads and the journal about 0.25. Left in the shadow: the draw loop (now the
largest part), the decode, the misses' poses (the near, animated mobys).

Proved: `RAC1_MRP_SCENE_HASH` (every drawn moby scene hashed: its mobys, models, matrices, records, poses, parts) identical
with the three on and off on the 19 levels widened, at LOD High and at LOD Original (900 frames each); `RAC1_MRP_LOD_VERIFY=1`
on all of those runs (the memory after each shadow run: 0 differing); the check modes above; RAMHASH on the 4-level route
widened at LOD High identical to the 4:3 one (7abc4c2e, 733,729 lines); the reference images unchanged.

The draw loop 0x2559c0, both passes (`moby_draw_hand.cpp`, `RAC1_PERF_MOBY_DRAW`): the same code as the transliteration, op
for op and access for access (generated from it), with the VU0 registers and flags as a local copy (`vu0_local.h`'s `VuF`, every
op's flags kept as the transliteration's `LocalFlags` keeps them) and the memory accessors' fast paths inline (a read by pointer
into the RAM or the scratchpad; a write the same, inside a journal only on a page it saved already; anything else through
`ee_read` / `ee_write`). The transliteration stays as the reference (`moby_draw_loop_tr`) and the fallback. In the sampler
(Kerwan widened, LOD High) the loop's samples fell 15 % (909 to 782, the second pass's 668 to 554): the accessor calls are gone,
what is left is the loop's own VU0 arithmetic, every operand clamped and every op's flags made. On the bench (five interleaved
runs): the second pass 1.08 to 1.01 ms, the real pass's `moby pass (draw loop)` 0.39 to 0.36; at LOD Original within the noise.
Proved: `RAC1_PERF_MOBY_DRAW=2` (both forms at every call of the real pass, compared: the 32 MB of RAM, the scratchpad, the VU0
registers and flags, the DMA registers, the ring position returned and every call of the MRP's observer with the record it is
handed) over the 19 levels widened at LOD High and at 4:3: 17,076 calls each, 0 differing; `RAC1_MRP_SCENE_HASH` identical with
it on and off on the 19 levels widened at LOD High and Original (the second pass, which runs it unchecked inside its journal),
`RAC1_MRP_LOD_VERIFY=1` on those runs (0 differing); RAMHASH and the reference images unchanged.

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

## Drawing without the chains

The MRP takes the mobys from observers on the ported moby pass, but the world, the sky, the effects and the 2D it still gets by
reading back the DMA / VIF / GIF chains the ported `render_frame` builds. The first steps of replacing those reads with the data's
sources, each behind a `RAC1_PERF_*` switch whose `=2` runs both ways and compares them field by field:

- **The workers' parts in the profiler.** The CSV's `part:` columns (and a line each in `RAC1_PROFILER_DUMP`'s summary) split the
  workers' busy time: the chain reader's texture uploads into the GS memory model and its VU1 programs; the scene worker's host
  decisions (the resident colours, `static_frame_build`, the casters, `tfrag_decide`), its capture, its walk (or the direct world's
  build) and `build_triangles`. `src/profiler/perf_switch.h` holds what the switches share: the value, and a dual mode's account
  (comparisons, differences, the first few described, the line at the end of a run).
- **The upload memo** (`RAC1_PERF_UPLOAD_MEMO`, `runtime/gs/gs_memory.cpp`). Every frame the chain reader wrote every texture upload
  into the 4 MB model pixel by pixel, swizzled, comparing each pixel only after computing its address: 40-55 % of the chain worker.
  Most uploads write the same bytes as the frame before. Each piece of a transfer (its BITBLTBUF / TRXPOS / TRXREG, where in the
  transfer it starts, its size) now keeps the bytes it wrote last, the range of the memory its pixels cover and the write log's number
  after it; the same piece with the same bytes, its range unchanged since (`changed_since`, the log the texture snapshots already
  use), would write every byte over itself, so it is skipped and only the transfer's position advanced. Exact by construction:
  `=2` writes every piece and checks the skipped ones changed nothing: 0 of the 515,625 pieces it skipped over the 19 levels at 16:9,
  32:9 and 32:9 with LOD High.
- **The direct world** (`RAC1_PERF_DIRECT_WORLD`, `mrp_world_scene.h`). When the host makes every tie, shrub and tfrag draw (the LOD
  slider off Original or a widened view: `mrp_world_static.h`, `mrp_tfrag_gpu.h`), the scene worker still copied the world passes'
  chain ranges and walked them on its private VU1 - VIF unpacks, set-up programs, a packet match at every draw start - only to
  throw the draws away. What the host's draws take from those ranges is little: the pass constants the chain leaves in VU1 memory
  (the tfrag view at 5..8 and constants 0x290..0x29d, the tie A alpha-test state 0x3cb..0x3cd, the shrub A fog vf29 / vf30 after
  its constants start) and the GS registers the passes' DIRECT packets set (TEX0 from 0x1c2f00, the shrubs' TEST and CLAMP, the
  far variants' stream's TEX registers). The direct world reads them where the chain's packets put them, in place in EE memory at
  the capture: a scan of the ranges' tags and VIF codes that skips their data (nothing copied), applies only the UNPACKs that
  reach those constants and runs only the constants' starts, on a private VU1, and notes the DIRECT register writes in chain order.
  The walk then stops where the world's ranges begin (it still walks the frame's set-up and the sky), and the host's draws are made
  where the walk would have left each pass's range, with the state that pass's chain left. The game builds every chain byte as
  before (RAMHASH identical). `=2` builds the walker's scene too, draws it, and compares the two every frame - the packets in
  order, the register writes before each, and through their indices the views, the instances (rows, morph, F, alpha, colours),
  the texture qwords, the tfrag draws with their view and constants, the GIF and sky writes: 0 differing frames over the 19 levels
  at 16:9, 32:9 and 32:9 with LOD High (6515 frames each, 5.8 / 9.0 / 23.3 million packets).
  - At the game's own 4:3 Original the host does not decide the world (the frame is walked as before): `=3` makes it decide there
    too and draws directly, which is not the same picture (the host's draws are the widened view's kind - the guard band's GPU path
    for what the game would clip, the host's far variants - not the decode of the game's chain): Veldin's frame 300 at 1440 x 1080,
    1282 pixels differ (405 by more than 8 levels). `=4` compares it with the walk (every frame differs there).
  - The CPU tfrag fallback (`RAC1_TFRAG_CPU`, no GPU path) now gets the host's decisions too: each decided entry drawn by the CPU
    reference on the VU1 memory its chain would have left (the entry's image for its LOD from the level's data, the view and
    constants the scan read, the lit colours from the resident mirror), instead of the tfrag shadow run and its walk. The same
    pictures as before (Kerwan 16:9, Veldin 32:9, frames 300 and 600); against the walker's draws at 4:3 Original (`=4`
    with `RAC1_PERF_DIRECT_WORLD_CMP=tfrag`) every vertex, colour and register write is the same but for the textures' place in
    VRAM (TEX0's TBP0 / CBP, MIPTBP): the images keep the level load's, the chain the frame's own placement, which the world's
    textures are not found by (CLAMP's MINV index names them).

Measured as the analysis was (headless, OpenGL, `RAC1_PIPELINE=1 RAC1_GAME_THREAD=0`, forward input, 1500 frames, the medians
of frames 600-1450; Graphics Original, LOD Original but where said, HUD 4:3; 1920 x 1080 at 16:9, 5120 x 1440 Fill Screen widened).
The before is the same build with `RAC1_PERF_UPLOAD_MEMO=0 RAC1_PERF_DIRECT_WORLD=0`, two interleaved runs each on a shared machine
(other builds and games ran beside it: the frame periods are not comparable, the workers' parts are), ms a frame, before -> after:

| | Veldin | Kerwan | Rilgar | Hoven |
|---|---|---|---|---|
| Chain worker: uploads, 16:9 | 0.65-0.91 -> 0.27-0.30 | 0.74-0.76 -> 0.18-0.19 | 0.73-0.87 -> 0.56-0.73 | 1.36-1.50 -> 1.17-1.40 |
| Chain worker: uploads, widened | 0.73-0.83 -> 0.42-0.45 | 0.81-1.04 -> 0.19-0.27 | 0.83-1.01 -> 0.55-0.64 | 1.55-1.62 -> 1.56 |
| Scene worker: walk, 16:9 | 0.44-0.49 -> 0.14-0.15 | 1.14-1.17 -> 0.21-0.23 | 0.79-0.91 -> 0.11-0.16 | 0.69-0.77 -> 0.16 |
| Scene worker: walk, widened | 0.74-0.80 -> 0.20-0.27 | 1.65-2.03 -> 0.35-0.39 | 0.79-1.13 -> 0.26-0.27 | 0.94-0.98 -> 0.23-0.26 |
| Scene worker busy, 16:9 | 1.45-1.59 -> 1.30-1.31 | 3.22-3.37 -> 2.44-2.67 | 2.28-2.69 -> 1.20-1.85 | 2.33-2.80 -> 1.84-1.88 |
| Scene worker busy, widened | 2.61-2.88 -> 1.74-2.18 | 4.87-5.44 -> 3.34-3.70 | 2.55-3.41 -> 2.28-2.37 | 3.42-3.52 -> 2.51-2.80 |

Kerwan at 4:3 Original: the uploads 0.45-0.57 -> 0.10-0.12 (the walk unchanged: the host does not decide there); with `=3` the
scene worker 5.77-7.33 -> 2.46-2.79 ms (its walk 3.69-4.99 -> 0.22-0.25). Kerwan widened with LOD High: the uploads 0.88-1.36 ->
0.22-0.27, the walk 1.82-2.51 -> 0.68-0.77, the scene worker 5.56-6.85 -> 4.56-5.19. Both are exact: the reference images and RAMHASH are unchanged, the four-level RAMHASH route widened with LOD High is identical
with both switches at 0 and at 1, and the pictures of Veldin, Kerwan, Rilgar and Hoven at 16:9 and widened (OpenGL and Vulkan),
Kerwan with LOD High and Kerwan and Rilgar at 4:3 are identical (frames 300 and 600; but for a few HUD pixels at the top of
Veldin's frame 600 with OpenGL, 4 to 15, which differ from run to run with main's own build too). The capture costs what it did (the scan,
0.5 ms a frame on Kerwan at 16:9, replaces the copy of the ranges), so does the main thread's wait for it. Hoven's uploads change
from frame to frame, so the memo skips little there.

The other steps of that plan (the sky from its asset and the per-frame state the sky pass reads, the particles from the pool, the
quads and prim lists from their producers' arguments, program 7's batches decoded on the EE side, not building the chains the MRP
no longer reads) are not done. The 2D stays on the chain.

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
  Since November 2026 the first pass keeps the late depth test (the discard's), on both renderers: with the early test, on a
  Radeon 890M (radeonsi and RADV), a few pixels of foliage over a wall came out differently from one run to the next (Veldin
  at 16:9 and 32:9, frame 600: two or three pictures in ten runs; AMD_DEBUG=nohyperz made it stable), the late test gives
  the picture of `RAC1_PERF_OPAQUE=0` every run. Only the second pass is left out now.
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

## Smaller ones, found reading the code

Found in November 2026 by reading the code against these pages, not from a profile. Each is exact - the same pictures and the
same game state - behind a switch, and on by default but where said. The measures are the profiler's medians, three runs of
each taken in turn on the fast cores, a switch off against all of them on: at 16:9 widened with LOD High (Vulkan, the game
thread, `RAC1_PIPELINE=1`, the last 1500 of 2400 frames of a forward walk) on Kerwan and Rilgar unless said.

- **A level's code address as Veldin's** (`RAC1_PERF_CODE_MEMO`, `ovl_reloc.cpp`): every `switch` of the transliterated code,
  and the dispatch of the camera's, the menus' and the sound's callbacks, translates a level's code address
  (`ovl_code_to_veldin`); for a jump table's label, which is not a function's start, that is a map search, a set search and
  an upper bound. The answers are kept by address in a 1024-slot table, emptied when the translation's maps change. 13 to
  45 translations a frame (Kerwan the most), all but a few dozen of a run answered from the table; `=2` compares each with
  the search: 840,467 over the 19 levels, 0 differing. A few microseconds a frame: under what a frame's time shows.
- **`vu0::running()` inline in every build** (`runtime/vu/vu0.h`; it was inline only with `RAC1_INLINE_HELPERS`): `vmacro()`,
  which the transliterated code calls before a VU0 macro op (a running micro program ends first), asked it through a call
  into another library: 1,000 times a frame on Veldin, 3,500 on Kerwan, 8,200 on Rilgar, 16,400 on Blarg Station. It is a
  load and a test. No switch: the same expression. Not done: `vmacro()` once per run of macro ops in `mips2cpp.py` and
  `asm2fn.py`, as `asm2cpp.py` emits it - the calls are in 34 of the generated files (1,953 sites), not in all of them, and
  with the test inline a call left out saves about a nanosecond: at most some microseconds a frame, for a change of the
  generated code to prove on every level.
- **The tfrags' opacity table** (`RAC1_PERF_OPAQUE_KEEP_TFRAG`, `mrp_world_renderer.cpp` `tfrag_entry_opaque`): the table of
  the tfrag entries whose colours are all opaque (`RAC1_PERF_OPAQUE`'s test) was keyed on the colour mirror's update count,
  one a frame, so it was made again over every entry every frame - the fault the ties' table had (`RAC1_PERF_OPAQUE_KEEP`).
  It is keyed on the count of entries whose colours changed (and on the mirror's size and its resets at a level's arrival).
  The same table, so the same draws: the pictures at 16:9 widened with LOD High (OpenGL, Vulkan) and with every enhancement
  on are those of the build before. The scene worker's `triangles` part: Kerwan 0.52 to 0.49 ms a frame, Rilgar 0.44 to 0.41.
- **The reflections' cull boxes** (`RAC1_PERF_REFLECT_BOX`, `mrp_world_renderer.cpp` `gpu_packet`): for every tie and shrub
  packet of the frame the worker made the instance's box for Water Reflections' views - a 4 x 4 product in doubles and eight
  corners through it, each with a division - whether or not such a view would be drawn. Its one reader is those views' cull
  (`reflect_cull_prepare`). It is made only when the row is on with Surface Detail, without which `mrp_reflect_pass` draws
  none; an instance without a box is never left out of a view, as before the views culled. The scene worker's `triangles`
  part: Kerwan 0.68 to 0.49 ms a frame, Rilgar 0.73 to 0.41; the worker busy 1.56 to 1.36 and 1.39 to 1.06.
- **The resident colours on Vulkan** (`RAC1_PERF_RCOL_KEEP`, `mrp_world_renderer.cpp` `upload_resident_colours`): the Vulkan
  branch copied the whole mirror of the ties' and shrubs' colours (64 and 24 words an instance) into the frame's ring every
  frame, where OpenGL sent the changed ranges alone. It now keeps a buffer per frame in flight and writes each with the
  ranges changed since that buffer was last written (this upload's, and the one before's): the tfrag colours' scheme. `=2`
  compares the buffer with the mirror after every upload: 17,076 uploads over the 19 levels, 0 differing (3 of a
  walk's 900 are the whole table). The main thread's `World` stage: Kerwan 0.23 to 0.14 ms a frame, Rilgar 0.11 to 0.07.
- **The moby scenes' storage** (`RAC1_PERF_SCENE_KEEP`, `mrp_scene.cpp`): the shadow run's scene - the larger of the two,
  some 0.8 MB at LOD High - was a new `MrpFrame` every frame, its vectors grown again from nothing on the game thread, and
  the one before freed. The shadow's scene, the real pass's and the one put aside now take turns in three frames that keep
  their vectors' storage. `RAC1_MRP_SCENE_HASH` identical on the 19 levels. `Host: LOD / wide view moby re-run`: Kerwan 0.63
  to 0.57 ms a frame, Rilgar 0.57 to 0.53; the game thread busy 3.05 to 2.97 and 2.44 to 2.38.
- **The upload memo's pieces keep up with the write log** (`RAC1_PERF_UPLOAD_MEMO_KEEP`, `runtime/gs/gs_memory.cpp`): a piece of
  a texture upload the memo skips kept the write log's number of its one write. Every later check of it then walked the
  log back to that number - through up to 8192 entries, for every piece that never changes, every frame - and once the log
  had forgotten that number the piece was written again. A skipped piece now takes the log's number at the skip (the
  number before an entry still open, which may yet grow). `RAC1_PERF_UPLOAD_MEMO=2`, which writes every piece and so always
  moved the number up, checks the skips as before: 893,233 skipped pieces over the 19 levels, 0 differing. The chain
  worker's `texture uploads` part: Kerwan 0.097 to 0.070 ms a frame; Hoven, whose uploads change every frame, 0.79 to 0.78
  (within the noise). The pieces written again at the log's floor turned out few (28 in 7000 frames on Kerwan): the gain is
  the walk of the log.
- **The chain's hash** (`RAC1_PERF_CHAIN_HASH`, `orig_main.cpp` `chain_bytes_hash`): every frame's preparation hashes the whole
  chain (some hundreds of KB) for the moby skips' memo, which a later frame reads only if it walks that chain again (a fade,
  a frame not drawn). The hash took a word at a time into one sum, each multiply waiting for the last; it takes four words
  into four sums. It stays a hash of the chain's bytes at the time of the note: a count of the frames built instead was
  weighed and left, since a chain rebuilt with the same bytes must still be found, and each level runs its own copy of
  the routines that start a chain. `Chain capture / scene start`: Kerwan 0.29 to 0.24 ms a frame, Rilgar 0.23 to 0.19.
- **The draw's copy of the game's memory** (`RAC1_PERF_DRAW_MEM`, `orig_main.cpp` `draw_memory_take`): a pipelined frame's
  preparation copied about 1 MB (the executable's and the overlay's data, the moby array) for the drawing to read beside
  the next update. Its readers are the modern lighting and the ambient particles (`mrp_lighting.cpp`, `mrp_particles.cpp`,
  through `ee_copy.h`); with both off - the original graphics - nothing reads it, and it is not taken.
  `RAC1_DRAW_MEM_CHECK=1` names any read of the game's memory the drawing makes outside the copy: none over the 19 levels
  widened with LOD High on the game thread. The main thread's `Prepare` stage: Kerwan 0.074 to 0.017 ms a frame, Rilgar
  0.075 to 0.015.
- **The launcher's progress file off the game's thread** (`RAC1_PERF_PROGRESS_THREAD`, `src/host/progress.cpp`):
  `progress.json` was written - a temporary file, then a rename - by the thread running the game frame, once a minute and
  at each planet: on a slow disc or with a scanner watching the folder, a late frame. The text is made there and handed to
  a writer thread; the end of the run waits for the last write. The file is the same (a headless run with `RAC1_PROGRESS`
  set, both ways). Not measured: on this machine the write is under a millisecond.
- **The tfrag compute's timing, once** (`RAC1_TFRAG_TIME_ONCE`, `mrp_tfrag_gpu.cpp`; Vulkan): at every level's arrival the first
  512 tfrag draws are dispatched in chunks of 64, each a submit of its own waited for and timed (a GPU too slow for them
  gets the tfrags from the CPU). A device a level of the session has timed at under half the limit, on a full sample, is not
  timed again: on the route through four levels one level is timed, not four (here 2.5 to 3.2 ms for 1500 draws against a
  limit of 12). The same vertices either way; `RAC1_RAMHASH` and the pictures unchanged. Eight waited submits less over a
  later level's first frames.
- **The join's sleep** (`RAC1_GT_JOIN_SLEEP=1`, off by default; `runtime/ee/game_thread.cpp` `finish`): when the drawing ends
  before the update, the main thread polls for the game thread's job to end, spinning on a core for the whole of the
  update's lead (2 to 4 ms a frame on Kerwan). With the switch it sleeps first - woken by the job's end or a request -
  until a millisecond before the shortest of the last eight jobs would end, then polls. Unpaced (the throughput run above,
  Kerwan, three runs of each taken in turn) the main thread's CPU time fell from 3.51 to 2.95 ms a frame, but the host
  frame grew from 4.31 to 4.48 ms and the game thread's update from 3.27 to 3.42: the wake-up, or the core the main thread
  gave up. Left off: its place is a paced frame on a machine with few cores, where it has not been measured. The job
  pool's thread count (`job_pool.cpp`: half the logical CPUs less two, which does not count the game thread) was not
  changed: nothing to measure it with on a 24-thread machine.
- **The moby pass's shadow run after the real pass** (`RAC1_PERF_POSE_ORDER=1`, off by default; `overlay_entry.cpp`,
  `mrp_lod.cpp`, `mrp_pose_memo.cpp`): the shadow run comes before the real pass and the pose memo works only inside it, so
  an animated moby both passes draw is posed twice a frame. With the switch the real pass runs first and keeps each pose's
  key and matrices (it skips nothing), and the shadow run after it finds them: on Kerwan the memo's misses fall from 4963
  to 2212 in 900 frames, `Host: LOD / wide view moby re-run` from 0.57 to 0.54 ms a frame (Rilgar 0.53 to 0.51). Off,
  because the shadow's scene is not the same: `RAC1_RAMHASH` is identical on the 19 levels (LOD High and Original),
  `RAC1_MRP_LOD_VERIFY=1` finds no difference, `RAC1_PERF_POSE_MEMO=2` none, and of the scene's hash the mobys, their
  matrices, the joints, the parts and the unpacks are identical - but the mobys' records differ on every level, in 20 of
  their 157 words: the w lanes of vectors the pass writes on x y z, and six words at the record's head, which hold what the
  VU0 registers held before the pass - after the real pass, no longer what they held before it. The 16 pictures compared
  (Veldin, Kerwan, Rilgar, Hoven at 16:9 with LOD High, OpenGL and Vulkan, frames 300 and 600) are the same; whether
  anything ever reads those words was not established. `RAC1_MRP_SCENE_HASH` now prints the scene's kinds of data apart.
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
| `RAC1_PERF_OPAQUE_KEEP=0` | the ties' lowest-alpha table made again every frame |
| `RAC1_PERF_TFRAG_FIND=0` | the world walker's lookup of every tfrag draw's entry, also where the host makes the tfrags |
| `RAC1_PERF_GS_MEM=0`, `RAC1_PERF_VU1_LOOKUP=0`, `RAC1_PERF_XGKICK=0` | the chain reader's GS memory, VU1 port lookups (and the world walker's program lookup) and kicks as before |
| `RAC1_PERF_TEXTABLE=0`, `RAC1_PERF_TFRAG_ALLOC=0`, `RAC1_PERF_VU7=0`, `RAC1_PERF_PKT_MEMO=0` | the scene's and the effects' preparation as before |
| `RAC1_PERF_PALETTE_BLEND=0` | the six levels' palette blend as the game's loop |
| `RAC1_PERF_VU_CLAMP=0` | the collision triangle test and the moby transform clamping every operand at each op |
| `RAC1_PERF_COLL_MESH=1` | the collision segment test's moby meshes kept between calls (off by default) |
| `RAC1_PERF_COPIES=0` | the EE journal's pages zero-filled then copied, the moby observer's record built aside and copied |
| `RAC1_MRP_LOD_ROOM_JOURNAL=1` | the widened view's / LOD slider's shadow runs' own room saved and put back by the journal |
| `RAC1_PERF_POSE_MEMO=0` | the second moby pass poses every moby (`=2`: the memo's hits checked against the pose) |
| `RAC1_PERF_SCENE_READS=0` | the MRP moby scene's observers read a word at a time (`=2`: both ways, compared) |
| `RAC1_PERF_LOD_PATCH=0` | the second moby pass's distances written through the journal, every moby (`=2`: the scale-1 identity checked) |
| `RAC1_PERF_EULER_CACHE=1` | the Euler conversion's rows kept per angles (off by default) |
| `RAC1_PERF_VU0_NF=0` | the generated code's VU0 ops with their flags |
| `RAC1_WORLD_CULL_FAST=0` | the world passes' three culls as transliterated (`=2`: both, compared at every call) |
| `RAC1_PERF_MOBY_DRAW=0` | the moby draw loop as transliterated (`=2`: both, compared at every call of the real pass) |
| `RAC1_DROP_SHADOWS_FAST=0`, `RAC1_SKY_FAST=0` | the drop shadows' and the sky's routines as transliterated (`=2`: both, compared; `RAC1_HAND_CHECK_EVERY` to check one call in n) |
| `RAC1_PERF_UPLOAD_MEMO=0` | every texture upload's piece written into the GS memory model, as before (`=2`: both, checked) |
| `RAC1_PERF_DIRECT_WORLD=0` | the world passes' chain ranges copied and walked when the host makes the world (`=2`: both, compared; `=3`: the direct world at 4:3 Original too) |
| `RAC1_PERF_SKY_LATE=1` | the sky after the leading opaque tfrag batches (off by default: not exact) |
| `RAC1_PERF_FPU_RAW=0` | the EE FPU helpers by the PS2 rules at every operation, through doubles (`=2`: those, compared with the raw forms) |
| `RAC1_PERF_VU0_RAW=0` | the VU0 macro ops clamping every operand and result at every op, the pose's loops too (`=2`: those, the ops compared with the raw forms) |
| `RAC1_PERF_CODE_MEMO=0` | a level's code address translated to Veldin's by searching the maps at every call (`=2`: both, compared) |
| `RAC1_PERF_OPAQUE_KEEP_TFRAG=0` | the tfrag entries' opacity table made again every frame |
| `RAC1_PERF_REFLECT_BOX=0` | the reflections' per-instance cull boxes made for every packet, whatever the options |
| `RAC1_PERF_RCOL_KEEP=0` | Vulkan: the whole mirror of the resident colours copied every frame (`=2`: the kept buffers checked against it) |
| `RAC1_PERF_SCENE_KEEP=0` | the moby scene of the shadow run a new frame every time, its vectors grown from nothing |
| `RAC1_PERF_UPLOAD_MEMO_KEEP=0` | a skipped upload piece keeps the write log's number of its last write (its checks walk the log back to it) |
| `RAC1_PERF_CHAIN_HASH=0` | the frame's chain hashed a word at a time into one sum |
| `RAC1_PERF_DRAW_MEM=0` | the drawing's copy of the game's memory taken every frame, whatever the options |
| `RAC1_PERF_PROGRESS_THREAD=0` | `progress.json` written by the thread that runs the game frame |
| `RAC1_TFRAG_TIME_ONCE=0` | Vulkan: the tfrag compute's first dispatches timed, in waited chunks, at every level's arrival |
| `RAC1_GT_JOIN_SLEEP=1` | the main thread sleeps for most of its wait for the game thread, then polls (off by default: a longer frame unpaced) |
| `RAC1_PERF_POSE_ORDER=1` | the moby pass's shadow run after the real pass, which fills the pose memo for it (off by default: the scene's records differ) |
| `RAC1_MRPW_THREAD=0`, `RAC1_GS_THREAD=0`, `RAC1_JOBS=1` | the scene, the frame's chain's reading (the MRP's chain reader, or the GS replica), the job pool on the main thread |

Each gives the same picture and the same game state: only the time changes (`RAC1_SHADOW_FAR_EVERY` also when a far cascade's
moby shadows are drawn; `RAC1_MRPW_P2_LESS` a few pixels).

## Measuring

The profiler's CSV (`RAC1_PROFILER_CSV=<file>`) has, per host frame, its period, the game frames it ran (`steps`), the pictures it presented (`pictures`: k with the frame extrapolation, whose pictures a second the Profiler tab and its overlay show beside the fps, which count host frames), the preparation's whole time (`prepare_total_ms`: the `Prepare` stage with the stages inside it - the chain's capture, the wait for the scene's capture, the moby lists -, since a stage's own column leaves out the stages it holds), the main
thread's CPU time, every stage's CPU and GPU time, and the workers' busy and CPU time (the game thread's: `busy: Game thread`;
its stages are counted in the stage columns, the main thread's wait for it in `Wait: game thread`), and the workers' parts (`part:`
columns: the chain reader's uploads and VU1 programs, the scene worker's decisions, capture, walk and triangles). Two ways to use it
headless (the window's size and the Options as a player has them: a copy of `graphics_options.txt` in a directory given by
`XDG_CONFIG_HOME`, with `RAC1_ADVANCED=1`):

- **Throughput**: a deterministic headless run with `RAC1_PIPELINE=1` (and `RAC1_GAME_THREAD=2` for the game thread) runs one
  game frame per host frame as fast as it goes: the period is the cost of a game frame, the same game frames in every run.
- **As a player**: `RAC1_REALTIME=1 RAC1_FPS_CAP=0 RAC1_AUDIO_DEVICE=null RAC1_PIPELINE=1` steps the game by the wall clock
  and paces as a window does: the `steps` column's distribution is how often a host frame ran two game frames.

Three stages were outside every other one and are now their own (November 2026): `Frame begin (Vulkan)` (the frame's fence,
its pools, the clears: it was in `Draw (other)`), `Wait: swapchain image` inside it (`vkAcquireNextImageKHR`: with FIFO and a
full queue the main thread waits there a refresh or more; a headless run has no swapchain and shows none) and `Materials:
frame capture` (Surface Detail's copy of the frame's colour and depth, CPU and GPU: it was in no stage). The dump
(`RAC1_PROFILER_DUMP`) says how many host frames ran none, one, two and three or more game frames - in a paced run
(`RAC1_REALTIME=1`), how often the picture's rate halved or a frame was shown twice - and the preparation's whole time. The
end of a run with `RAC1_PERF_OPAQUE` lists, per level, the GPU path's draw commands without the alpha test, with it but held
opaque, and those that still take the second pass (`[perf] opaque draws, level N`).

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
