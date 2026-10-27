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
