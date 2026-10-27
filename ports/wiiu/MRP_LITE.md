# MRP-lite: the Modern Rendering Pipeline on the Wii U's GX2

Step 3 of [PLAN.md](PLAN.md): the game's 3D (mobys, the world, the sky, the effects, the drop shadows) drawn natively on the
Wii U, the way the PC's MRP draws it ([docs/rendering.md](../../docs/rendering.md)), on a GPU that has no compute shaders, no
storage buffers, no multi-draw-indirect and no `gl_BaseInstance` (Latte, R700-class, GLSL compiled offline by CafeGLSL), and a
CPU (Espresso, 3 cores at 1.24 GHz, big-endian) whose one core is already busy with the game's update (13-25 ms a frame).

The principle stays the MRP's: the game's own render code decides what is drawn (visibility, LOD, poses, lights, tints); the
MRP only draws its decisions. What changes is how the per-frame data reaches the GPU.

## What the MRP is made of today

**CPU preparation, no GL** (reusable as it is, after the big-endian fixes below):
- mobys: `mrp_scene` (observers on the ported moby pass: per moby its class, LOD, model-to-view rows, joint matrices, three
  lights, tint, fade, render target; `MrpFrame`), and the data-only half of `mrp_moby_renderer.cpp` (`part_pre`: the clip
  matrix, the light rows - in doubles -; `build_frame`: `PartGpu` 288 B a part, one `DrawCmd` per texture run, batches by state).
- world: the scene worker (`mrp_world_renderer.cpp` `SceneWorker`) walks the world passes' chain on a private VU1 and matches
  the draw starts against the level's assets; its set-up starts run the ported world VU1 programs (`world_vu1_*.cpp`, left out
  of the Wii U build for the .text limit). Beside it, a **host-made path** already exists for tie / shrub
  (`static_frame_build`, `mrp_world_static.cpp`): visibility from the level's own tables (zones, frustum, clip box, draw
  distance, LOD with morph, fog, wind) - no culling pass of the game needed, which is what the Wii U wants (its tie / shrub
  culling is off, RAC1_WORLD_PASS_SKIP=6). Today it runs only with a LOD other than Original or a widened view. tfrag has a
  host decision too (`tfrag_decide`) and a complete CPU form of its GPU program (`tfrag_exec_cpu`).

**GL backend** (to write again for GX2): storage buffers for the joints, parts, commands, instances and colours; indirect
multi-draws per batch; `gl_BaseInstance` to find a draw's records; texture arrays with a sampler array; the tfrag compute
shader (with double precision); `precise` (GLSL 400); `textureQueryLod`; depth clamp; stencil drop shadows.

## Design

### The data path: uniform blocks per draw, a few textures

| PC | Wii U |
|---|---|
| SSBO of the frame's joints | a uniform block per moby (at most 112 joints: ~7 KB), bound per draw |
| SSBO of the parts (18 vec4) + `cmds[gl_BaseInstance]` | a uniform block per draw: the part + its command word |
| SSBO of the tie / shrub instances (160 B) | a uniform block per draw (the instance + the command) |
| SSBO of the resident lit colours (~0.4 MB) | an R32UI texture read with `texelFetch` in the vertex shader, its changed ranges uploaded |
| SSBO of the moby normal table (512 floats) | a constant uniform block (128 vec4) |
| `glMultiDrawElementsIndirect` | a CPU loop of `GX2DrawIndexedEx`, the state set per batch |
| texture arrays + a sampler array | 2D-array surfaces, one bound per batch (batches split by texture bucket) |

The per-draw blocks are written into a MEM2 ring (as `gs_draw_gx2.cpp` does for the 2D: aligned to 0x100, invalidated with
`GX2Invalidate`); GX2 reads them as big-endian words, which is what the PPC writes. The draw order stays the walker's / the
moby pass's (the alpha-fail splits and the blended draws depend on it).

### Shaders

The GL shaders' sources (`kVs` / `kFs` in `mrp_moby_renderer.cpp`, the world's in `mrp_world_renderer.cpp`, their Vulkan copies
in `tools/gen_vk_shaders.py`) brought to GLSL 330 for CafeGLSL, kept in `ports/wiiu/shaders/`:
- `precise` is GLSL 400: to check what CafeGLSL / Latte do with `a * b + c` (fused or not) against the CPU references
  (`mrp::skin_packet`, `world_mrp.cpp`), the moby skinning's 16-bit truncation and the colour step depend on it;
- `textureQueryLod`: the GS's own mip formula (the faithful mode) or `dFdx` / `dFdy`;
- the sky's depth clamp: z = w (1 - e) in the vertex shader;
- the GS texture functions, alpha test with its fail modes, fog, blending: already done for the 2D (`gs_draw_gx2.cpp`).

### The world without a compute shader

- tie / shrub: the host-made path (`static_frame_build`) turned on for the Wii U - the game's tie / shrub culling stays off. The
  instances' lit colours stay those of the level's load until the game's relighting runs again (to look at later: a
  relighting of our own, or the game's relighting on a worker core).
- tfrag: first the static form (each entry's near image, no morph: a T-only mesh the vertex shader transforms; one LOD
  everywhere, no cracks), then `tfrag_exec_cpu` on a worker core if the LOD morph is wanted. The game's tfrag pass keeps running
  (its packets read the culling's lists).
- the walker (for the sky and anything left on the chain): needs the world VU1 set-up programs. Either their fast ports put
  back into the build if the .text stays under 32 MB (to measure: the exact ports alone could stay out), or the set-up starts'
  effect computed without them. The sky's own path (the particle program, already in the build) is separate.

### Threads and cores

- Core 1: the game (as today). Core 2: the IOP's mirror; the MRP's scene build / world walk / tfrag on core 0 or 2 (the PC's
  scene worker is a thread already). Every value two threads share must not be a namespace-scope `thread_local` (the build
  defines it away): `vu1.cpp`'s `g_last_start` / `g_side_widen` / `g_start_observer` need `PerThread` first.
- A worker that runs the game's float code sets the FPSCR as the game's thread (round toward zero, NI), as `job_pool.cpp`'s
  `FloatEnv` does.

### Big-endian

Every MRP asset reader reads little-endian files with host-order `memcpy`:
- `.mrpm` / `.mrpt`: `In::get<T>` (`formats/moby_mrp.cpp` :416), `read_class`, `read_textures`; the armor textures' size
  (`mrp_moby_renderer.cpp` :563);
- `.mrpw`: `R::raw` (`formats/world_mrp.cpp` :33), `read_world`; the VIF blobs inside are read by `rd32` / `vif_list`
  (`mrp_world_assets.cpp`) and unpacked through `vu1::vif1_command` (which expects host words: the blobs' words swapped once);
- the walker: `r64` (`mrp_world_scene.cpp` :49) and an 8-byte copy (`mrp_world_renderer.cpp` :1593);
- `mrp_moby_renderer.cpp` :1893 reads a record's byte at `+0xbc` from host-order words (the wrong byte on the PPC);
- the GPU's own data: s16 attributes and u32 indices built by the PPC are big-endian - GX2's formats with the right endian
  swap, or none (GX2 reads big-endian natively; to verify per format).
These are fixed in the common code (the same results on the PC: checked by the three references and RAMHASH each time).

### The 2D, and main's replica-out

The game's 2D is drawn today through the GS replica's GX2 backend (`gs_draw_gx2.cpp`). On main the replica is being removed for
the HUD too (`feature/replica-out`): when that lands, the HUD follows main's native path here as well. What the 2D backend
taught (textures, render targets, blending, the uniform-block layout, the ring) carries over to the MRP-lite.

## Phases

1. **Assets on the console**: the readers made big-endian safe; Veldin's `.mrpm` / `.mrpt` / `.mrpw` on the SD card
   (generated files, never in the repository: 24 MB for Veldin); loaded at the level's start, a checksum of what was loaded
   compared with the PC's.
2. **Mobys**: the GX2 moby backend - one pass, no drop shadows, no modern lighting -, a depth buffer shared with the 2D; the
   skinning in the vertex shader. First picture: Ratchet and the mobys on Veldin over the 2D.
3. **tie / shrub**: the host-made path on, the template shader, the resident colours as a texture.
4. **tfrag**: the static form, then the morph on a worker core if needed.
5. **sky, effects (`mrp_fx`), alpha-fail passes, drop shadows** (a D24S8 buffer of our own for the stencil).
6. **Speed**: draw-call cost, batching, the work on cores 0 / 2.

Each phase ends with shots of the console's TV (RAC1_WIIU_SHOT) compared with the PC's at the same game frames.

## Risks

- The CPU cost of one draw call per texture run (no indirect draws) on Espresso.
- Exactness of the shaders without `precise` (CafeGLSL's fusing unknown).
- The walker's VU1 set-up programs and the .text limit.
- Uniform-block limits (64 KB a block and ~16 blocks a stage are R700's usual figures: unverified).
- Memory: the MRP's assets and the game's 32 MB, the GX2 surfaces, the texture arrays (MEM2; the out-of-memory seen once in
  play is not explained yet).

## Progress

- Phase 1 done (2026-10-27): the MRP asset readers (.mrpm / .mrpt / .mrpw, the VIF blobs at vif1_command's boundary, the
  walker's 64-bit reads, the moby record's byte at +0xbc) big-endian safe, the PC unchanged (the three references, RAMHASH as
  main's). RAC1_MRP_ASSET_CHECK=<level> hashes every parsed field of a level's MRP assets: Veldin's 122 files hash
  cb0304afbdf1e532 on the PC and on the console. Veldin's assets (moby/level0, moby/armor, world/level0: 28 MB) are on the SD.
  Left: the code that does arithmetic on palette entries as numbers (lighting analysis, debug thumbnails) and the GPU vertex
  packing, to look at in phases 2-3.
- Phase 2 (2026-10-27): the mobys drawn on the console (mrp_moby_gx2.cpp, shaders/mrp_moby.gsh, the CPU preparation shared
  through mrp_moby_backend.inc). Veldin with explore, the mobys alone (RAC1_WIIU_2D=3): Ratchet, the crates, the plants and
  the creatures at the PC's places and poses, textured and lit (game frames 150 / 200). What it took:
  - the MRP mobys turned on: RAC1_NO_GRAPHICS (always on here) turns the whole MRP off - RAC1_MOBY_MRP=1 in run.cfg;
  - no GL on the Wii U: glad is built but never loaded, a GL call jumps to 0 and takes the console down (white screen,
  static) - load_textures still bound a texture at its end. The Wii U build's other GL users are in functions its path
  never calls (checked on the binary: the glad_gl* relocations per function);
  - **the shaders' uniform blocks are read as little-endian words** (the vertex shader's and the pixel shader's): each
  32-bit word byte-swapped when written (block_of; gs_draw_gx2.cpp's block too - its ints were read wrong, unseen so far
  since its tests are mostly != 0). The fetch shader's attributes are right as they are (INTEGER, 16-bit ones 8_IN_16).
  Left: the drop shadows (their chain range skipped on the walk; a faint square of their last pass stays under the hero),
  the render targets' mobys (the menus' item models), the modern lighting; performance not measured yet. The debug
  switches (mrp_moby_gx2.h: RAC1_WIIU_MRP_STAGE / _DEBUG / _GSH) found it: careful with a "screen triangle per draw" mode
  - thousands of full-screen triangles a frame stall the GPU for seconds a frame (it looks like a hang: one draw a call).
- Phase 3 (2026-10-27): Veldin's ties and shrubs drawn with GX2 from the host-made static path (mrp_world_gx2.cpp,
  shaders/mrp_world.gsh, the CPU preparation shared through mrp_world_backend.inc). On the console (explore, game frame 200)
  the towers, the rocks, the crates, the plants, Ratchet and Clank at the PC's places, textured, lit and coloured as the PC's;
  missing: the ground (tfrag), the sky. The world and the mobys both in the world's projection (g_mrp_world_projection /
  g_mrp_moby_projection 0, the PC's default: the clip matrices), the depth shared (wiiu_gfx.cpp clears it once, then the world,
  then the mobys). What it took:
  - no walker (the world VU1 programs are not in the build): overlay_entry.cpp's world_pass takes the static snapshot before
    the tie / shrub passes with `handle` always on (mrp_world_ready(): the GX2 backend up, the level loaded);
    mrp_world_wiiu_prepare (orig_main's walk branch, next to the mobys' prepare) runs static_frame_build ->
    mrpw::scene_from_static -> build_triangles on the game's thread. scene_from_static shares emit_static's code
    (static_draws, state_packet: the PC unchanged) and takes the walker's VU state from the overlay's copies the passes'
    constants UNPACK: the tie A alpha-test state packet (0x1c2a30 / 0x1c2a40, always sent: its state was 0xffffffff on the
    PC), the shrub A fog constants (0x1c2ad8..0x1c2ae4). The frame environment's GS registers before the world passes
    (the chain's DIRECT packets the walker would read) are the values found on Veldin (TEST_1, ALPHA_1, ZBUF_1, PRMODECONT,
    TEXA, FBA_1), FOGCOL the replica's (gs_global().reg(0x3d), as the mobys');
  - per draw command a uniform block (the instance, 160 B, and its 8-word record), one GX2DrawIndexedEx with the template's
    first vertex as its offset (base vertex), the draw's texture array bound per draw (its unit in the frame's epoch: no
    texture units on GX2); the alpha test's two passes and the GS state per batch as the GL backend's;
  - the resident lit colours an R32UI texture (1024 texels a row, linear) read by texelFetch in the vertex shader; the whole
    table sent at a new level, then the changed ranges (none while the relighting is off, RAC1_WORLD_PASS_SKIP=6). Its words
    are written byte-swapped: GX2 reads the R32UI texels as little-endian words, as the uniform blocks (the PPC's order gave
    the colours' bytes reversed on the console; RAC1_WIIU_WORLD_RCSWAP=0 the PPC's order, debug bit 1024 flips it);
  - the game's own VU1 projection (mode 1, RAC1_WIIU_WORLD_PROJ=vu / RAC1_WIIU_MRP_PROJ=vu) repeats the GS's 16-bit screen
    coordinates: an instance crossing the camera's plane wraps into planes over the screen (the VU's clipping programs keep
    those from the PS2's GS; the static path draws whole instances) - seen on the console: hence the world's projection;
  - debug switches as the mobys' (mrp_world_gx2.h: RAC1_WIIU_WORLD_STAGE / _DEBUG / _DEBUG_AT (default 0) / _GSH), the world
    off with RAC1_WIIU_WORLD=0; the depth's clear left the moby backend (its debug bit 512 is RAC1_WIIU_ZCLEAR=0 now).
  Left: the tfrags (phase 4); the sky, the shrub far variants (phase 5); the drop shadows, the modern lighting; the relighting
  (the instances keep the level's load colours). Seen right on the console: the colours' byte order, GX2DrawIndexedEx's offset
  as the base vertex (debug bit 256 sets it by the attribute buffer's address instead). Unverified: the snapshot's inputs with the
  passes' culling off (the fog, tangents, zones static_note_pass reads before the passes: to compare with the PC's), the flat
  colour's provoking vertex (the non-IIP draws), CafeGLSL's MULADD_IEEE in the tie fat colour's float steps (`precise` on the
  PC), the level's templates' memory in MEM2 (their size on the log).
  With the 2D on (the GS replica's backend, gs_draw_gx2.cpp) the distant 3D comes out lighter, veiled, than on the PC; with
  RAC1_WIIU_2D=1 (no render targets) or 3 (no 2D) it does not. Not found in the frame's own primitives (all drawn onto the screen
  page 0x70; those reading it are skipped): left for main's mrp_2d on the Wii U, which replaces this backend.
