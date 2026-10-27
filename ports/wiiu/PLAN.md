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
  few pages differ in the mobys' skinning output (vertex colours a few units off), most likely Cemu's float
  emulation (newlib's sqrtf on the 750 starts from frsqrte, whose table Cemu does not reproduce bit for bit; on the
  console fputest's sqrt hash equals the PC's). The exact comparison is for the console.
- **Cemu workflow** (debugging only): `ports/wiiu/tools/cemu_rpx.py` (conditional traps made nops), RAC1_FPU_NEAREST=1,
  `--enable-gdbstub` + `powerpc-eabi-gdb` for backtraces.
- **Found and fixed on the way:** code over 32 MB loops the static constructors (VU1 programs left out); wut's
  recursive_mutex is not recursive (989snd); elf2rpl takes no TLS; freopen(stdout) writes nothing (own devoptab);
  the sound commands' records, 989snd's reader / samples / opcodes / names, the stream's ADPCM words, the WAD /
  level core / overlay parsers, VIF halfword and byte elements, VU1 MPG words, the drop shadows' unaligned
  doubleword, RAMHASH on big-endian.

Next: the console run (timings: `/home/daino/progetti/rac1-scratch/wiiu/console_run.cfg`), then RAMHASH on the console
against the PC (EE rounding on both), then the remaining edges (GS replica packets, the sky decoder).
