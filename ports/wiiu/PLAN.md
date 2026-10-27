# Wii U port: plan

Step 0 (feasibility), step 1 (common-code groundwork), then step 2 (headless prototype on the
console) and step 3 (GX2 "MRP-lite"). Effort is in working days; the counts come from an audit of
the tree at 3ad2ecb (grep counts, approximate).

## Step 0: is Espresso fast enough? (before step 1)

A profile captured on the PC (Ryzen AI 9 HX 470, `rac1_profile_20261026_180449.csv`, 300 frames)
shows the game's own code at a median of ~9.9 ms per game frame (p90 13.5 ms), of which the moby
joints / skinning (VU0) ~5.6 ms, the world passes ~2.9 ms, the moby pass ~1.6 ms. One Espresso core
(PowerPC 750, 1.24 GHz, no SIMD beyond paired singles) is very roughly 10-15x slower than one of
those cores, which would put the game code far above the 12-13 ms target. The profile's settings are
unknown (and the profiler costs something), so this is a red flag, not a verdict:

1. Measure the game code on the PC properly: a release build, `RAC1_PROFILER_CSV`, a few levels, LOD
   and widened view off, profiler overhead noted.
2. Measure the PC-to-Espresso factor with code of this kind: a small benchmark run on both (the
   project's own vecmath and a VU-program-like loop, built for PPC with wut), timed on the console.
3. Decide: game-code ms on the PC x factor, against ~12-13 ms. Skinning and the VU programs are the
   big items, so also estimate what a paired-single version of them could save.

Effort: 1-2 days, once the console launches homebrew reliably.

## Step 1: common-code groundwork (useful for Android / macOS too)

Every change is invisible on the PC: the reference images (shot_600, `RAC1_MRP=0`, boot) and
`RAC1_RAMHASH` stay identical before and after each piece.

### 1a. EE memory on a big-endian host (3-5 days)

The accessors (`ee_read` / `ee_write`, `runtime/kernel/ee_memory.h`; `lw`/`sw`/`lh`/`ld`/`lq`...
on top of them) are one choke point covering ~19,400 call sites, including all the generated code.
What bypasses them: ~3,000 `V()` / `M()` references (host `Vec4&` / `Mat4&` straight into EE RAM),
~135 direct pointer casts, ~25 struct reads, ~17 memcpys, the scratchpad overlay, the interpreter's
own load/store.

Proposed layout (instead of byte-swapping every access): EE memory, scratchpad and VU memory hold
host-native 32-bit words. On a little-endian host this is exactly today's layout (no change on the
PC); on a big-endian host a byte is at `addr ^ 3`, a halfword at `addr ^ 2`, a doubleword's two words
are in address order. Then the 32-bit and float accesses (the bulk), `V()`, `M()` and the float and
int32 casts work unchanged and cost nothing; what needs work is the byte and halfword accesses
(an XOR), the 64/128-bit ones, the ~70 int16/byte pointer casts, and bulk byte copies into EE memory
(disc, level loader, files: swap per word). VU memory is already word-native today. RAMHASH hashes
the canonical little-endian bytes, so the hashes compare across machines.

### 1b. Format parsers and 989snd (2-3 days)

~213 `memcpy`-into-host-int field reads (`src/formats`, `runtime/kernel/disc.cpp`, `runtime/level`),
the `get<T>` / `table_rows<T>` helpers, the `#pragma pack` structs (6 files), ~23
`reinterpret_cast` reads in the renderers; 989snd's `BinaryReader::read<T>` (115 uses, structs
included), its LE-assuming unions, ADPCM reads. Through explicit little-endian readers
(`src/io/byte_reader.h` exists). The setup's caches (`world_mrp`, `moby_mrp`) get a defined byte order
or are rebuilt for the console at setup (step 3).

### 1c. VU / VIF / GIF / GS edges (1-2 days)

VIF unpack (16 memcpys), XGKICK tags, 64-bit reads of VU memory, micro-program upload, `gs_emu.cpp`
(19 memcpys): swap where bytes from EE memory become words.

### 1d. SIMD and x86-only code (2-3 days)

`runtime/vu/vu_native_fast.h` (base of every generated `*_fast` VU program) and
`src/game/ovl/vu0_regs.h` use SSE2 directly; `vu0.cpp` uses `rdtsc`. A portable 4-lane layer (GCC
vector extensions, scalar on PPC; port/android has a NEON shim to align with), then the generators
(`vu2cpp_fast.py`) regenerate against it. Same results bit for bit.

### 1e. Float rules on PowerPC (1-2 days)

`ps2fpu::enter_ee_rounding`: FPSCR RN=01 (toward zero) and NI=1 (non-IEEE: denormals to zero);
`NearestScope` RN=00; the job pool's `FloatEnv` and every thread that runs game math (the world walker)
set FPSCR. `-ffp-contract=off` stays mandatory (GCC fuses `fmadds` otherwise); no `ps_madd`.
To verify: NI mode's handling of denormal operands on the 750, and `sqrt_nearest` (the 750 has no
`fsqrt`: newlib's software sqrt under RN=01 must truncate correctly).

### 1f. The coroutine on PowerPC (1 day)

`ctx_switch.h` has x86-64 / AArch64 switches and falls back to `ucontext` (not in devkitPPC's newlib):
a `coroutine_ppc.S` saving r14-r31, f14-f31, CR, LR and FPSCR.

### 1g. A "lite" build (1-2 days)

One CMake option for weak machines: debug menu and ImGui out (`RAC1_DEVTOOLS` exists), profiler and
sampling profiler as no-op stubs, the EE interpreter and the setup library out, Vulkan stubbed
(`vk::is_active()` constexpr false), the LOD / widened-view re-runs compiled out (3 call sites), movies
optional. The GS replica stays (2D); its remaining 3D-related VU1 work is a later refactor.

### 1h. MRP: scene preparation vs GPU backend (deferred to step 3)

Scene building (`mrp_scene`, `mrp_world_scene`, `mrp_world_static`) makes no GL/VK calls already; the
backends interleave GL and VK in the same files, with `#version 460` shaders, SSBOs, multi-draw-indirect
and a tfrag compute shader. A GX2 backend is a third backend written against the scene structs; the
CPU preparation still inside the renderer files (world walker thread, per-moby `part_pre`) moves to the
scene side then.

### Verification without the console (1-2 days, in parallel)

A big-endian Linux build (powerpc cross compiler) of the lite headless game run under `qemu-ppc`:
RAMHASH frame by frame against the PC. Slow, but it proves 1a-1f before touching the console.

**Step 1 total: ~12-18 days**, plus the reviews and playtests on the PC.

## Progress (branch port/wiiu)

- 1d: `runtime/vu/vu_simd_shim.h`, the SSE2 subset as GCC generic vectors for hosts without SSE2, defined by
  the lanes' values (endian-proof). Checked against real SSE2 on the PC: the same results bit for bit on 200,000
  random cases (every intrinsic the two headers use). `vu0.cpp`'s `rdtsc` behind a `prof_ticks()` helper. No
  regeneration of the generated code needed (it uses no intrinsic directly).
- 1e: `ps2_fpu.h` on 32-bit PowerPC: `mtfsfi 7,5` (NI = 1, RN = toward zero), `NearestScope` through FPSCR RN; the
  job pool's `FloatEnv` carries the whole FPSCR.
- 1f: `src/platform/coroutine_ppc.S` (r14-r31, f14-f31, CR, LR, FPSCR) and its frame in `ctx_switch.h`.
- `ports/wiiu/fputest/`: the float rules and the coroutine, the same hashes expected on the PC and the console
  (PC: add 99ea71e4 mul b8b53831 div 51f61c66 sqrt 09edfac3 denorm 7fb2bb3c vendor 3f8ccccb, coroutine same).
  On the console (2026-10-26): add, mul, div, sqrt, vendor and the coroutine match the PC exactly (newlib's `sqrtf`
  is fine). Denormals differ: in NI mode the 750 flushes denormal *results* but uses denormal *operands* as they
  are (x86's DAZ, and the PS2, read them as zero): `den+den` gives 0x00800000, `1 - den` 0xbf7fffff
  (`denorm_cases.h`). The game makes no denormals (its results are flushed); one in memory comes only from an
  integer read as a float. The VU programs and `fpu_in` zero them in software already; whether plain C++ in the
  ports meets one is for the RAMHASH comparison to show (then fix there, not at the load: lwc1 / swc1 move bits).
- wiiload works (2026-10-26): `ports/wiiu/tools/wiiu-run.sh` must not probe port 4299 (the plugin serves one
  connection at a time and waits on the probe's), and the app must give the screen back on ProcUI's release
  (WHBLogConsole does) or the return to the menu stalls.
- PC reference images unchanged (shot_600 6faa80a7..., MRP=0 b0b9aa45..., boot 3c83c728...).
- 1a (started): the EE memory as host-native words (`ee_memory.h`: swizzled bytes / halfwords, doublewords' low word
  first, records as words or through `le_fix`); `ee_ref` for scalar references (the hand-written ports and the code
  generator); `ee_copy_in` / `ee_copy_out` / `ee_zero` / `ee_bytes_view`; the disc, the base ELF, the boot module, the
  level loader and the level / TOC parsers read the PS2's bytes as little-endian (`src/io/host_endian.h`). Still to do:
  the GS replica and VIF edges (packets read from EE memory as 64-bit tags), 989snd and the IOP side, the renderers'
  byte reads (not needed without graphics), and whatever the RAMHASH comparison on the console finds.
- `RAC1_NO_GRAPHICS=1` (orig_main): the game's update alone, no window or GL / Vulkan context, the renderers at the
  faithful path. On the PC its RAMHASH equals `RAC1_MRP=0 RAC1_WORLD_FAITHFUL=1`'s frame by frame (600 frames of
  `RAC1_GOTO=0@60 RAC1_SIM_INPUT=forward`). The Wii U build runs this mode.

## Feasibility, first numbers (2026-10-26)

`RAC1_NO_GRAPHICS=1` on the PC (Ryzen AI 9 HX 470, one core at up to 5.3 GHz), Veldin, `forward`: the game's steps
take **10-14 ms per frame** (the 60-frame averages after the level's start: 11.5, 13.6, 12.6, 14.4, 12.5, 10.7,
10.3), audio (989snd's tick) included. The sampling profiler (main thread): `moby_skin_all` ~66% (the joints'
pose on the VU0 macro ops, `moby_vu0_skin_fast` 26% of it), the world passes ~12% (tie / shrub culling), VU0 runs
~7%, the rest the game's logic. One Espresso core is very roughly 10-15x slower than this core, so the game's code
as it is would need on the order of 100-200 ms per frame there: far from the ~12-13 ms target.

What would have to change for the port to be realistic: the skinning (two thirds of the time) rewritten for the
750's paired singles and without the per-access EE memory checks, the world culling likewise; even then a 60 fps
game on one Espresso core looks out of reach, 30 fps hard. The console's own measurement (step 2) will give the
real factor; this is the number to decide on.

## Status, 2026-10-27 (night)

- **The game runs on Wii U code (in Cemu):** `ports/wiiu/game` builds the whole engine in `RAC1_NO_GRAPHICS` mode as
  a .rpx (19.5 MB). In Cemu it starts, loads Veldin, runs 120 frames with `RAC1_SIM_INPUT=forward` and a
  `RAC1_GOTO=0@60` level change, loads the sound bank, plays the music streams' commands, and returns.
- **Game state:** the hero's and the camera's positions at frames 60 and 120 equal the PC's (both rounding to
  nearest: Cemu cannot run the EE rounding mode). RAMHASH: frames 0 and 1 identical page for page; from frame 2 a
  few pages differ in the mobys' skinning output (vertex colours a few units off, written by `moby_skin` through the
  scratchpad). Not newlib's sqrtf: an exact integer sqrtf (`RAC1_WIIU_EXACT_SQRT`) leaves them as they are. Still
  open: Cemu's float emulation, or a real difference (a float-to-integer conversion out of range, which x86 and
  PowerPC answer differently; a sub-word read missed). The console's RAMHASH tells which.
- **Cemu workflow** (debugging only): `ports/wiiu/tools/cemu_rpx.py` (conditional traps made nops), RAC1_FPU_NEAREST=1,
  `--enable-gdbstub` + `powerpc-eabi-gdb` for backtraces.
- **Found and fixed on the way:** code over 32 MB loops the static constructors (VU1 programs left out); wut's
  recursive_mutex is not recursive (989snd); elf2rpl takes no TLS; freopen(stdout) writes nothing (own devoptab);
  the sound commands' records, 989snd's reader / samples / opcodes / names, the stream's ADPCM words, the WAD /
  level core / overlay parsers, VIF halfword and byte elements, VU1 MPG words, the drop shadows' unaligned
  doubleword, RAMHASH on big-endian.

Next: the console run (timings: `/home/daino/progetti/rac1-scratch/wiiu/console_run.cfg`), then RAMHASH on the console
against the PC (EE rounding on both), then the remaining edges (GS replica packets, the sky decoder).

## First console run (2026-10-27, morning)

The unpatched build on the console (Aroma, wiiload), `console_run.cfg`: Veldin, the forward walk, a level change at
frame 60, 600 frames, EE rounding (`mtfsfi`), no RAMHASH.
- The six ISO blocks hash as on the PC; the game's main returned 0 after 127 s, back to the menu by itself.
- Game code (the "update" of the `[orig] frame` lines: the average of the last 60 frames): 195 ms (frame 60), 172-304 ms in play (240-600), 8-45 ms on the
  loading screen. About 212 ms/frame over the whole run; the PC does the same frames in about 6 ms.
- Hero and camera at frames 60, 300, 600 equal the PC's to 0.01 (some print one hundredth higher: to check with a
  RAMHASH run whether it is only the printing).
- So step 2's target (12-13 ms on one core) is off by ~20x: the next thing is a profile of where the time goes.

### The profile (RAC1_PROFILER_DUMP=60) and the RAMHASH on the console (same day)

Game code per frame in play, console vs PC (`[profiler]` sub-stages, averages over 60 frames):

| sub-stage | Wii U | PC |
|---|---|---|
| moby joints / skinning (VU0) | 175-225 ms | 3.3 ms |
| world passes (tfrag, tie, shrub) | 48-56 ms | 0.6 ms |
| moby pass (draw loop) | 14-21 ms | 0.15 ms |
| the game logic (moby updates, hero, camera, the rest) | 5-7 ms | 0.25 ms |

The game itself fits the budget; over 95% is the rendering preparation done by the VU0 replica (generic vectors: no
SIMD GCC can use on Espresso). Step 3 should move the skinning to the GPU (GX2 vertex shader from the joint matrices)
and the world / moby passes into the MRP-lite preparation, after checking the game never reads their output back.

RAMHASH: the console's 300 frames (the level change at 60 included) are identical page for page to the PC's run with
`TZ=UTC`, after two fixes:
- a function-local `thread_local` became an automatic variable under `-Dthread_local=` (989snd's Rand() restarted
  each call: sounds ended early, the game's channel records differed from frame 33). Written `static thread_local` now.
- the sky decoder read EE memory as raw bytes and its numbers in the host's order: "invalid sky" on the console, and
  sky_init skipped its writes into the game's state (page 0x15e000 from frame 60).
The one difference left was the headless fixed clock (1700000000) read as local time: CET on the PC, UTC under newlib
(0x15ee98's hour byte). Compare with a PC run under `TZ=UTC`. The skinning differences seen in Cemu do not happen on the
console: they were Cemu's.

### Without the GS path's work (RAC1_NO_GRAPHICS=2, same day)

On the PC the skip changes no game state (600 frames: hero and camera identical, the skip's next record right 11893 /
11893; only the DMA rings, the work area and the class packets differ). On the console, in play:

| sub-stage | before | with the skip |
|---|---|---|
| world passes | 48-56 ms | 48-52 ms |
| moby pass (draw loop) | 14-21 ms | 17-21 ms |
| moby joints (now the poses only) | 175-225 ms | 14-18 ms |
| game logic | 5-7 ms | 5-6 ms |
| frame | 250-300 ms | 91-98 ms (~10.5 fps) |

Next: the world passes (visibility, LOD and the VU1 chains for tfrag / tie / shrub, ~50 ms) and the moby draw loop
(~20 ms, which the MRP scene needs) - what a GX2 renderer can take instead; the poses (~15 ms) stay on the CPU (the
joint matrices a GPU skinning reads) unless they get faster.

### Ideas for later (Daino)

The GamePad as a second screen once the game draws: a real-time map, the quick select of the weapons.

### The world passes (same day)

Each pass's own time on the console (RAC1_WORLD_PASS_TIME, in play): tfrag 5 ms, tie 17, shrub 23, sky 7 (PC: 0.05 /
0.2 / 0.27 / 0.1). The culling leaves are EE / VU0 code transliterated on 128-bit registers: ~80x the PC, against
~25x for the game logic.

RAC1_WORLD_PASS_SKIP=6 runs the tie and shrub passes with the game's own render enable off (their culling not run)
and skips their relighting (it would read the stale lists). PC, 600 frames: hero and camera identical; what differs is
the lighting lists, the DMA rings, the passes' variables and the instances' colours in the level data (now those of
the level's load: a GX2 renderer culls and relights them itself). The tfrag's culling cannot be turned off this way
(its packets read the culling's lists in the scratchpad).

Console, in play, with RAC1_NO_GRAPHICS=2 RAC1_WORLD_PASS_SKIP=6: **frame 55-58 ms (~18 fps)**, from 250-300 at the
start of the day:

| sub-stage | ms |
|---|---|
| moby joints (the poses) | 18 |
| moby pass (draw loop) | 13-17 |
| world passes (tfrag 5, sky 7) | 12 |
| game logic (moby updates, hero, camera, the rest) | ~6 |
| a spike in "logic (other)" once per 60 frames | ~98 |

Next: the hot primitives on Espresso (the Q128 / VU0 macro registers on generic vectors, the emulated memory's
accesses), which every one of these runs on - a sampling profile on the console would show which; the sky pass (with
the host decoding the sky, does the GX2 renderer need it?); the spike.

### The hot spots on Espresso (same day)

A sampling profiler (ports/wiiu/game/wiiu_sampler.cpp, RAC1_WIIU_SAMPLE=<us>; ports/wiiu/tools/samples.py maps the
samples on the ELF) put ~30% of the frame in the VU0 macro ops: their flags were built with SSE2's packs / movemask,
loops of 16 on the shim. Without SSE2 they are now built lane by lane (vu0_regs.h fin_lanes: equal to the SSE2 path
on 4 million cases; the console's RAMHASH equals the PC's for 300 frames). In play: 55-58 ms -> 22-24 ms (the poses
18 -> 3.4, the moby draw loop 13-17 -> 8, the world passes 12 -> 4).

The spike every ~36 frames (~95 ms) was the music streams' 2 KB reads: the SD card's file system walks the cluster
chain from the file's start to seek back, and two streams 140 MB apart alternated (back: ~96 ms, ahead: ~10). The disc
image is now read through up to four handles, each read taking the one just before it (platform_wiiu.cpp read_at):
after the streams' first reads no stall is left. A stream's start still costs one ~100 ms read (an asynchronous
read-ahead would hide it).

The sky's pass cannot be skipped like the tie / shrub culling: without it the mobys' records differ.

Console now, in play (Veldin, NO_GRAPHICS=2, WORLD_PASS_SKIP=6): **18-24 ms per frame**; 600 frames in 20 s (127 s
this morning).

### Beyond Veldin (same day)

- PC, every level (0-18), 2000 frames of RAC1_SIM_INPUT=explore (a new testing input: out of gameplay Start as
  "shipskip", in gameplay forward with turns, jumps and attacks): with RAC1_NO_GRAPHICS=2 and with RAC1_WORLD_PASS_SKIP=6
  the hero / camera / bolts / hp lines are identical and no page of the mobys' area ever differs. 17 levels reach
  gameplay (7-33 of the lines); Umbris (7) and Batalia (8) stay in their arrival with this input.
- Console: Novalis, Kerwan, Gaspar (300 frames from RAC1_LEVEL, the arrival only): RAMHASH identical to the PC's (TZ=UTC).
- Console, Veldin with explore, a sampled gameplay-only profile (RAC1_WIIU_SAMPLE_AFTER=6): 13-15 ms per frame in the open,
  22-25 ms near the start (more mobys). The spread: the poses ~10%, the VU0 ops ~15%, the memory accesses ~9%, the drop
  shadows ~5%, the tfrag culling and the moby draw loop ~4% each, 989snd's synth ~4%.
- Tried and reverted (slower on the console, 5-10%): a fast path in fin_lanes, the failures of ee_read / ee_write out of
  line, ee_read / ee_write / fin_lanes always inlined (.text 25.5 -> 28 MB). Espresso's 32 KB instruction cache: code
  size costs; every change is measured on the console before it stays.
- Left: a music stream's start still costs one ~100 ms read (handles parked along the image would cut it).

### First played run (same day)

ports/wiiu/game/wiiu_input.cpp: the GamePad (VPAD) and the Pro / Classic controllers (KPAD) as one gamepad in GLFW's layout
for the host's bindings (pad_packet's Wii U branch), and ProcUI each host frame (HOME closes the game). wiiu_audio.cpp:
the game's IOP's output (RAC1_IOP_MIRROR=0, pushed every vsync) into two looping LPCM16 buffers played by two AX voices on
the TV and the GamePad. With RAC1_REALTIME=1 RAC1_FRAMES=7200, Daino played Veldin on the console (no picture: the log's
screen): the controls answer, the sound plays (the music, the enemies), HOME closes. It stutters: near the start of
Veldin a frame costs ~25 ms (~40 fps), so the game runs slower than real time and the sound's writer falls behind the
voices (1199 resyncs in 3 minutes).

Next for the sound: the IOP's mirror on a core of its own (as on the PC, ADR 0013: audio_out.cpp's iop_loop), which
plays on alone when the game is late and takes 989snd's synthesis (~4%) off the game's thread.

### The sound's mirror, and the first picture (same day)

- wiiu_audio.cpp now runs the IOP's mirror (ADR 0013) on an OSThread of its own on core 2: it replays the game's
  recorded IOP vsync by vsync into the AX voices' buffers and plays on alone while the game's thread is late. Veldin
  with explore in real time, 81 s: 75 resyncs (1199 in 3 minutes before), 1026 frames played alone, 44.3 game frames a
  second. One earlier run stopped logging after a fall's reload (frame ~720) and the console was found back in its
  menu, with no exit line: not reproduced since (with RAC1_WIIU_WATCHDOG=10); keep the watchdog on.
- Step 3, phase 0: wiiu_gfx.cpp (RAC1_WIIU_GFX=1) brings GX2 up through the WHB helpers on the TV and the GamePad and
  draws a frame after each host frame of the game (for now a test triangle). Seen on the console by Daino: the game
  plays (its sound) under the picture.
- Shaders: CafeGLSL v1.0.0-rc1 (Exzap, MIT, Mesa-based). Its Wii U release is a 21.7 MB static library (the compiler
  linked into the RPX: too big next to our 25.5 MB of .text under the 32 MB a bl reaches), so the shaders are compiled
  offline on the PC by its Linux CLI (-DRAC1_WIIU_GLSLC=<bin/glslcompiler>) into .gsh files under build-wiiu/shaders,
  copied to rac1-native/shaders on the SD. To evaluate later (Daino): shaders written in Latte's assembly with
  decaf-emu's latte-assembler, no Mesa in the chain.

Step 3's phases from here:
1. the GS replica's drawing (gs_draw.cpp, OpenGL) on GX2: the HUD, menus, text, fades;
2. the mobys: the MRP scene state on the CPU, the skinning in the vertex shader (no storage buffers: uniform blocks /
   textures for the joints), the MRP assets from the SD (Veldin: 13 MB);
3. the world (Veldin: 11 MB): the MRP walks the world passes' chains (the tie / shrub culling then back on, or a
   visibility of the renderer's own), no compute shader for tfrag (Latte has none in GX2);
4. effects, particles, sky, drop shadows.

### Phase 1's map (the GS replica, for the HUD / menus / text / fades)

- runtime/gs/gs_emu.* is CPU-side and backend-neutral (chain walk vif1_chain -> chain_walk -> vif_stream -> gif;
  primitives kept in prims_, never rasterized; texture decode at kick, cached; 4 MB swizzled local memory).
  gs_state.* and the GsRenderer class are not used by it.
- The OpenGL backend is one block of gs_draw.cpp (one program: VS / FS GLSL 330 with TFX, fog, alpha test + AFAIL,
  DATE, screen sampling, per-pixel mips, dual-source blending; a 15-float vertex streamed into one buffer; textures
  per TexImage; one render target per frame page; a screen copy per DATE primitive). draw_vk (its Vulkan twin, one
  128-byte GsDrawPush, a pipeline cache) is the better template for GX2: the push block becomes one uniform block.
- In a run without graphics the whole prepare / draw is skipped: no chain walk. Phase 1 needs the chain stage
  (orig_main's chain_join, reset_frame, the chain's start / end with the 0x1611c4 fallback, set_screen_area,
  vif1_chain) and draw(...) with screen_fbp / fb_w / fb_h / skip_fbp, synchronous (RAC1_GS_THREAD=0: the worker
  would share the VU1's namespace-scope thread_locals).
- The 2D needs no VU1 program (DIRECT / GIF packets). But the walk reaches the world's and the mobys' VU1 starts,
  whose programs are left out of the Wii U build (RAC1_VU1_MISSING stubs abort): a start hook that skips them, or
  not registering them, first. The pause menu's item panels draw mobys into render targets: empty without them.
- Big-endian to fix in gs_emu.cpp before anything shows right: r64 / r32 read EE memory with memcpy, image_data
  reads bytes (p[i]); TexImage packs RGBA as r | g << 8 ... in a uint32 (uploaded as bytes: swapped on BE).
- CPU: the texture decode is per texel with a swizzle and the CLUT read each time (only when the memory changed).

Thread-local values left as globals under -Dthread_local= (one thread uses them today; recheck when a worker
starts): vu1.cpp g_last_start / g_side_widen / g_start_observer, host_profiler.cpp t_main. The 989snd's random
sequence and the stream instance (snd989_stream.cpp t_cur), which the game's thread and the IOP's mirror share,
use platform/per_thread.h's PerThread on the Wii U: shared, the mirror's alone frames during a fall's reload
crashed the console (the game's thread freed its streams on the mirror's instance) and the mirror's random calls
moved the game's.
- After the fix: the picture + mirror run past three falls (frames 720, 1560, 2340) to its end; before it, two of
  three such runs died at the first fall's reload. With the picture on, WHBGfxFinishRender waits for the vsync each
  host frame: 15.9 host frames a second carrying ~3 game steps each; the backend's present must not hold the game.
