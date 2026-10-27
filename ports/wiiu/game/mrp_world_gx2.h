// The MRP world's ties, shrubs (MRP_LITE.md phase 3) and tfrags (phase 4) drawn with GX2 on the Wii U: the frame mrp_world_renderer.cpp
// prepares on the CPU from the host-made paths (mrp_world_wiiu_prepare: static_frame_build and tfrag_decide -> mrpw::scene_from_static ->
// build_triangles, then the tfrags' vertices / triangles by the CPU route tfrag_frame_cpu; no walker, no compute shader) drawn by
// mrp_world_gx2.cpp instead of its GL backend. Host code.
//
// What the caller does on the Wii U (orig_main's no-graphics branch, the picture on: rac1_wiiu_picture()):
//
//   once, after WHBGfxInit (wiiu_gfx_start), unless RAC1_WIIU_WORLD=0:
//       rac1::mrp_world_gx2_init();      // shaders/mrp_world.gsh beside the game's data; sets g_mrp_world_projection = 0 (the world's)
//
//   every game update, before the real tie / shrub passes (overlay_entry.cpp world_pass): the static path's snapshot
//   (mrpw::static_note_pass, its `handle` mrp_world_ready()) - the passes' own culling and relighting are off on the Wii U
//   (RAC1_WORLD_PASS_SKIP=6): the ties / shrubs are the host's, their lit colours the level's load's; before the real tfrag pass the
//   tfrag decisions' snapshot (mrpw::tfrag_note_pass, `handle` mrp_world_ready()) - the tfrag pass itself runs (its packets read the
//   culling's lists): the frame's view and constants are read from its chain (mrpw::tfrag_pass_vu);
//
//   each host frame, after the game's frames ran and before the next update, in the walk branch next to the mobys' prepare:
//       mrp_world_wiiu_prepare(active);  // (mrp_world_renderer.h) `active` as the PC's mrp_world_begin_frame's
//
//   then in the picture (wiiu_gfx.cpp's draw_frame, for the TV and again for the GamePad), after the depth's clear and BEFORE the
//   mobys (mrp_moby_gx2_draw) and the 2D:
//       rac1::mrp_world_gx2_draw(cb, db, vp);  // as mrp_moby_gx2_draw's arguments (the depth NOT cleared here: shared with the mobys)
//
//   at the end, before WHBGfxShutdown:
//       rac1::mrp_world_gx2_shutdown();
//
// Drawn, in the frame's batch order: the tfrags' GPU path batches (shaders/mrp_tfrag.gsh: the compute program's vertices as the CPU
// makes them, one GX2DrawIndexedEx per run of one texture unit, its texture array bound before it), then the ties' / shrubs' (tie A /
// shrub A templates, one GX2DrawIndexedEx per draw command with the draw's instance and record as a uniform block, its texture array
// bound per draw, the resident lit colours an R32UI texture the vertex shader reads with texelFetch), in the world's projection (mode 0:
// the clip matrices, the tfrags' world points through the view-projection). Not drawn: the CPU path's triangles (none from the host's
// paths), the sky and the shrub far variants (phase 5), the drop shadows, the modern lighting.
//
// State left by mrp_world_gx2_draw: as mrp_moby_gx2_draw's (mrp_moby_gx2.h). It may call GX2SetContextState (the context of `cb`)
// after the level's texture uploads (GX2CopySurface), which change the GPU's state.
#ifndef RAC1_PORTS_WIIU_MRP_WORLD_GX2_H
#define RAC1_PORTS_WIIU_MRP_WORLD_GX2_H

#include <runtime/gs/gs_draw.h>

#include <gx2/surface.h>

#include <cstdint>

namespace rac1 {

// its shaders loaded (shaders/mrp_world.gsh, and the tfrags' shaders/mrp_tfrag.gsh, relative to the working directory) and its memory
// made: after WHBGfxInit. false: not loaded (mrp_world_ready() stays false: no world drawn; the tfrags' alone missing: no tfrags)
bool mrp_world_gx2_init();
// true once mrp_world_gx2_init succeeded
bool mrp_world_gx2_ready();
// the prepared frame's tfrags / ties / shrubs into `cb` / `db` (bound by this call, `db` not cleared), the game's frame mapped onto `vp` (x / y
// from the bottom left, as gs::draw_gx2's); nullptr db: no depth test
void mrp_world_gx2_draw(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp);

// (testing, RAC1_WIIU_WORLD_DEBUG=<m>[,<m>...]: the debug modes in turn, each for 50 game frames from game frame
// RAC1_WIIU_WORLD_DEBUG_AT (default 0) on, the last one kept; a mode's bits: 1 no depth test, 2 a plain grey (no texture, blending or
// alpha test), 4 the world's projection (the instances' clip matrices), 8 the shrubs alone, 16 the model's own x, y as the position
// (the transforms out), 32 the draws without their indices, 64 the ties alone, 256 the template's first vertex given by the attribute
// buffer's address (not by the draw's offset, GX2DrawIndexedEx's base vertex), 1024 the resident colours' words in the other byte order
// (the whole table sent again), 2048 one draw a call, 4096 the vertices' colours alone, 8192 the textures alone)
// (also testing: RAC1_WIIU_WORLD_STAGE=<n> how far a call goes - 0 / 1 nothing, 2 the level's textures made, 3 its geometry and
// colours sent and the state set, no draws, 4 (the default) everything; RAC1_WIIU_WORLD_GSH=<file> another build of the shaders;
// RAC1_WIIU_WORLD_RCSWAP=0 the resident colours' words as the PPC holds them (byte-swapped by default); RAC1_WIIU_WORLD_PROJ=vu
// the game's VU1 projection (mode 1); RAC1_WIIU_ZCLEAR=0 (wiiu_gfx.cpp) the depth not
// cleared before the world)
// (the tfrags', testing: RAC1_WIIU_TFRAG=0 none made nor drawn; RAC1_WIIU_TFRAG_DEBUG=<m>[,<m>...] / RAC1_WIIU_TFRAG_DEBUG_AT the
// tfrag batches' modes as RAC1_WIIU_WORLD_DEBUG's - bits 1 no depth test, 2 a plain grey (no texture, blending or alpha test), 4 a
// batch in one draw (its first run's texture: no split by texture unit), 8 the tfrags alone (the ties / shrubs not drawn), 1024 the
// 32-bit attributes not swapped (the other byte order), 2048 one tfrag draw a call, 4096 the vertices' colours alone, 8192 the textures
// alone; RAC1_WIIU_TFRAG_STAGE=<n> 1 the CPU route alone (nothing sent), 2 its data sent and the state set, no draws, 3 (the default)
// everything; RAC1_WIIU_TFRAG_GSH=<file> another build of the tfrag shaders)
void mrp_world_gx2_debug_frame(uint64_t game_frame);
// everything it made freed (the level's textures and buffers, the colours' texture, the ring, the shaders): before WHBGfxShutdown;
// it waits for the GPU
void mrp_world_gx2_shutdown();

}  // namespace rac1

#endif
