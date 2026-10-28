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
//   each host frame too, right before the chain's reader starts (orig_main's walk branch): the sky's range walked (mrp_world_sky.inc)
//       mrp_world_wiiu_sky_capture(walk);  // its packets put before the world's by the prepare, its batches made by build_triangles
//
//   then in the picture (wiiu_gfx.cpp's draw_frame, for the TV and again for the GamePad), after the depth's clear and BEFORE the
//   mobys (mrp_moby_gx2_draw) and the 2D:
//       rac1::mrp_world_gx2_draw_sky(cb, db, vp);  // the sky first: no depth test nor writes (behind everything)
//       rac1::mrp_world_gx2_draw(cb, db, vp);  // as mrp_moby_gx2_draw's arguments (the depth NOT cleared here: shared with the mobys)
//
//   at the end, before WHBGfxShutdown:
//       rac1::mrp_world_gx2_shutdown();
//
// Drawn, in the frame's batch order: the tfrags' GPU path batches (shaders/mrp_tfrag.gsh: the compute program's vertices as the CPU
// makes them, one GX2DrawIndexedEx per run of one texture unit, its texture array bound before it), then the ties' / shrubs' (tie A /
// shrub A templates, one GX2DrawIndexedEx per draw command with the draw's instance and record as a uniform block, its texture array
// bound per draw - or, by default, those draws merged: RAC1_WIIU_WORLD_MERGE below -, the resident lit colours an R32UI texture the
// vertex shader reads with texelFetch), in the world's projection (mode 0:
// the clip matrices, the tfrags' world points through the view-projection). Not drawn: the CPU path's triangles but the sky's (none other
// from the host's paths), the shrub far variants (phase 5), the drop shadows, the modern lighting. The sky (phase 5, mrp_world_gx2_draw_sky):
// its batches (Batch::sky) over the frame's CPU-path vertices with shaders/mrp_sky.gsh (mrp_sky.vert + mrp_world.frag), their textures the
// sky asset's (mrpwb::built_bucket), made at their first draw.
//
// State left by mrp_world_gx2_draw: as mrp_moby_gx2_draw's (mrp_moby_gx2.h). It may call GX2SetContextState (the context of `cb`)
// after the level's texture uploads (GX2CopySurface), which change the GPU's state.
#ifndef RAC1_PORTS_WIIU_MRP_WORLD_GX2_H
#define RAC1_PORTS_WIIU_MRP_WORLD_GX2_H

#include <runtime/gs/gs_draw.h>

#include <gx2/shaders.h>
#include <gx2/surface.h>
#include <whb/gfx.h>

#include <cstdint>

namespace rac1 {

// its shaders loaded (shaders/mrp_world.gsh, and the tfrags' shaders/mrp_tfrag.gsh, relative to the working directory) and its memory
// made: after WHBGfxInit. false: not loaded (mrp_world_ready() stays false: no world drawn; the tfrags' alone missing: no tfrags)
bool mrp_world_gx2_init();
// true once mrp_world_gx2_init succeeded
bool mrp_world_gx2_ready();
// (testing: the merge mode, RAC1_WIIU_WORLD_MERGE's - the GamePad's debug menu flips it live)
int& mrp_world_gx2_merge_mode();
// the prepared frame's tfrags / ties / shrubs into `cb` / `db` (bound by this call, `db` not cleared), the game's frame mapped onto `vp` (x / y
// from the bottom left, as gs::draw_gx2's); nullptr db: no depth test
void mrp_world_gx2_draw(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp);
// the prepared frame's sky into `cb` (the depth buffer bound, never tested nor written: its hierarchical Z untouched), the game's frame on
// `vp` as mrp_world_gx2_draw's; before it. State left: as mrp_world_gx2_draw's
void mrp_world_gx2_draw_sky(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp);
// the sky drawn (RAC1_WIIU_SKY=1; off by default; the GamePad's debug menu flips it live): off, the frame's sky is neither captured nor
// drawn, the game's sky pass not run either while the picture is up (mrp_world_wiiu_sky_pass_wanted), and the world's batches start from
// the GS registers before the sky's (as they did before the sky)
extern bool g_wiiu_sky;
// the sky's shaders loaded (shaders/mrp_sky.gsh): g_wiiu_sky can draw it
bool mrp_world_gx2_sky_loaded();

// (testing, RAC1_WIIU_WORLD_DEBUG=<m>[,<m>...]: the debug modes in turn, each for 50 game frames from game frame
// RAC1_WIIU_WORLD_DEBUG_AT (default 0) on, the last one kept; a mode's bits: 1 no depth test, 2 a plain grey (no texture, blending or
// alpha test), 4 the world's projection (the instances' clip matrices), 8 the shrubs alone, 16 the model's own x, y as the position
// (the transforms out), 32 the draws without their indices, 64 the ties alone, 256 the template's first vertex given by the attribute
// buffer's address (not by the draw's offset, GX2DrawIndexedEx's base vertex), 1024 the resident colours' words in the other byte order
// (the whole table sent again), 2048 one draw a call, 4096 the vertices' colours alone, 8192 the textures alone)
// (also testing: RAC1_WIIU_WORLD_STAGE=<n> how far a call goes - 0 / 1 nothing, 2 the level's textures made, 3 its geometry and
// colours sent and the state set, no draws, 4 (the default) everything; RAC1_WIIU_WORLD_GSH=<file> another build of the shaders;
// RAC1_WIIU_WORLD_RCSWAP=0 the resident colours' words (and the run table's) as the PPC holds them (byte-swapped by default);
// RAC1_WIIU_WORLD_MERGE=<n> the ties' / shrubs' draw commands merged (mrp_world_merge.inc): 0 one GX2 draw per command (the per-draw
// path, as before), 1 an instance's consecutive commands of one texture unit one draw, 2 of any unit (the batch's units bound at once),
// 3 (the default) and consecutive instances drawing the same commands one instanced draw (gl_InstanceID) - the debug bits 32 and 256
// take the per-draw path; RAC1_WIIU_WORLD_PROJ=vu
// the game's VU1 projection (mode 1); RAC1_WIIU_ZCLEAR=0 (wiiu_gfx.cpp) the depth not
// cleared before the world)
// (the tfrags', testing: RAC1_WIIU_TFRAG=0 none made nor drawn; RAC1_WIIU_TFRAG_DEBUG=<m>[,<m>...] / RAC1_WIIU_TFRAG_DEBUG_AT the
// tfrag batches' modes as RAC1_WIIU_WORLD_DEBUG's - bits 1 no depth test, 2 a plain grey (no texture, blending or alpha test), 4 a
// batch in one draw (its first run's texture: no split by texture unit), 8 the tfrags alone (the ties / shrubs not drawn), 1024 the
// 32-bit attributes not swapped (the other byte order), 2048 one tfrag draw a call, 4096 the vertices' colours alone, 8192 the textures
// alone; RAC1_WIIU_TFRAG_STAGE=<n> 1 the CPU route alone (nothing sent), 2 its data sent and the state set, no draws, 3 (the default)
// everything; RAC1_WIIU_TFRAG_GSH=<file> another build of the tfrag shaders)
// (the sky's, testing: RAC1_WIIU_SKY=1 on at the start; RAC1_WIIU_SKY_DEBUG=<m>[,<m>...] / RAC1_WIIU_SKY_DEBUG_AT its modes as the others' -
// bits 2 a plain grey (no texture, blending or alpha test), 16 the batches whose depth test fails at the far plane drawn too, 1024 the
// 32-bit attributes not swapped, 2048 one sky draw a call, 4096 the vertices' colours alone, 8192 the textures alone; RAC1_WIIU_SKY_GSH=<file>
// another build of the sky's shaders)
void mrp_world_gx2_debug_frame(uint64_t game_frame);

// (testing, 2026-10-28: the GPU's time - the console's TV frame took ~60 ms of GPU at full size, in proportion to its pixels) the
// picture's GPU switches: read once from run.cfg (gx2_perf_init: RAC1_WIIU_HIZ, RAC1_WIIU_NOKILL, RAC1_WIIU_LEAN, RAC1_WIIU_PIXELS_OFF,
// RAC1_WIIU_ZCLEAR), then flipped live by the GamePad's debug menu (Picture page); each one leaves the picture as it was, but
// pixels_off (a diagnostic)
struct Gx2Perf {
    bool hiz = false;         // the TV's / GamePad's depth buffers with their hierarchical Z (HTILE: WHB makes them without; wiiu_gfx.cpp)
    bool nokill = false;      // the batches without an alpha test drawn by the shaders' builds without discard (RAC1_NO_ALPHA_TEST: the
                              // depth tested and written before the shader); the world's, the tfrags', the mobys'
    bool lean = false;        // the world's / tfrags' lean pixel shader (mrp_world.frag RAC1_LEAN: the texture level's terms per vertex)
    bool pixels_off = false;  // (diagnostic) the 3D's and the 2D's scissor 8 x 8 pixels: the GPU's time without the pixels' work
    bool zclear2 = true;      // wiiu_gfx.cpp's own depth clear before the 3D (WHBGfxClearColor clears it already)
};
extern Gx2Perf g_gx2_perf;
// ((2026-10-28) the game's update on the game thread beside the drawing, orig_main.cpp's Wii U branch) what of the game's state the
// backends' draws read, taken at the frame's prepare (rac1_wiiu_picture_frame, wiiu_gfx.cpp), not at the draw: the world's view-projection
// (mrp_world_view_proj: the camera 0x166c80 / 0x166dc0, the projection's tangents 0x16caf0), the GS depth map (render_gs_depth: the
// projection's 0x16cb28...), the game's frame size (0x1518d0) and the mobys' mip choice (g_mrp_moby_mip, the game's graphics menu).
// In series nothing writes them between the prepare and the draw: the same values
struct Gx2DrawView {
    float vp[16] = {};
    bool depth_ok = false;
    double da = 0.0, db = 0.0;
    float fb_w = 512.0f, fb_h = 416.0f;
    int moby_mip = 0;
};
extern Gx2DrawView g_gx2_view;
// (the prepare's end: g_gx2_view taken now)
void gx2_draw_view_take();
// (once; called by the backends' inits and wiiu_gfx.cpp's first frame)
void gx2_perf_init();
// another build of `main`'s shaders (`path`, beside them: the same vertex shader, another pixel shader) loaded into `keep`; its pixel
// shader when it can stand in for `main`'s - the vertex shader the same (program and registers: the same outputs), the same uniform
// mode, its blocks and samplers where `main`'s are -, else nullptr (`keep` freed; the reason on the log)
const GX2PixelShader* gx2_ps_variant(const char* path, const WHBGfxShaderGroup& main, WHBGfxShaderGroup& keep);
// everything it made freed (the level's textures and buffers, the colours' texture, the ring, the shaders): before WHBGfxShutdown;
// it waits for the GPU
void mrp_world_gx2_shutdown();

}  // namespace rac1

#endif
