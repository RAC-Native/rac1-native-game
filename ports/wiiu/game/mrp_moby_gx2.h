// The MRP's mobys drawn with GX2 on the Wii U (MRP_LITE.md phase 2): the frame mrp_moby_renderer.cpp prepares on the CPU
// (mrp_moby_begin_frame / mrp_moby_prepare, unchanged) drawn by mrp_moby_gx2.cpp instead of its GL backend. Host code.
//
// What the caller does on the Wii U (orig_main's no-graphics branch, the picture on: rac1_wiiu_picture()):
//
//   once, after WHBGfxInit (wiiu_gfx_start):
//       rac1::mrp_moby_gx2_init();      // shaders/mrp_moby.gsh beside the game's data; sets g_mrp_moby_projection = 1
//
//   each host frame, after the game's frames ran (game_frames: mrp_scene_begin_frame / mrp_scene_end_frame around each update,
//   g_moby_mrp_skip decided before it from mrp_moby_ready(), as on the PC) and before the next update, with the game's state as
//   that update left it (no snapshot on the Wii U), in the walk branch where the GS replica walks the frame's chain:
//
//       const bool title = boot_active();
//       const bool flight = exiting && level_exit_loading_screen();
//       const bool scene = ee_thread_suspended() && ee_thread_frame() == rac1::EeVsyncFrame::Scene;
//       const bool fade = ee_thread_suspended() && !scene;                         // (the branch's own `fade`)
//       int mw = 0, mh = 0;
//       const bool movie = movie_hw_shown(mw, mh) != nullptr;
//       const bool vendor_pic = !title && !scene && vendor_picture_frame();
//       const bool menu_mode = lw(OV(0x0015f5c4)) == 3 && g_mode_at_update == 3;
//       mrp_moby_begin_frame((title && !boot_in_title() && !flight) || (exiting && !flight) || movie || fade || vendor_pic,
//                            menu_mode, title && mrp_title_menu_drawn());     // (chain_stage's call on the PC)
//       rac1::gs::Viewport gv;                                                 // (only its xscale / xscale_3d are read: 1)
//       if (mrp_moby_active()) mrp_moby_prepare(&gv);                          // the frame's parts, records, batches
//
//     the begin_frame first (it loads the level's MRP assets when the level changed and decides whether the MRP draws this
//     frame); it may come before or after the chain's walk (gs.vif1_chain): the walk only needs g_moby_mrp_skip's ranges, as
//     today. The prepare reads the game's memory (the class tables, the normal table, the mobys' bytes): before the next update.
//
//   then in the picture (wiiu_gfx.cpp's draw_frame, for the TV and again for the GamePad), BEFORE the 2D (draw_gx2):
//
//       rac1::mrp_moby_gx2_draw(cb, db, vp);   // cb / db: WHBGfxGetTVColourBuffer() / WHBGfxGetTVDepthBuffer() (DRC: theirs);
//                                              // vp: the 4:3 area draw_frame gives the 2D (x / y from the bottom left)
//
//   it clears `db` (depth 1.0) and draws the prepared frame's mobys into cb / db; nothing when no frame was prepared since the
//   last mrp_moby_begin_frame (the title before its loop, the level exit, a movie, a fade: the MRP not drawing). The GamePad's
//   call draws the same frame again (the per-draw blocks written for the TV's reused).
//
//   at the end, before WHBGfxShutdown (wiiu_gfx_stop / wiiu_gfx_game_ended):
//       rac1::mrp_moby_gx2_shutdown();
//
// State left by mrp_moby_gx2_draw (draw_gx2 sets its own anyway): `cb` / `db` bound, the shader mode
// GX2_SHADER_MODE_UNIFORM_REGISTER, no depth test, blending off, every channel written, the z clipping on, the viewport and the
// scissor the whole buffer, no culling. It may call GX2SetContextState (the context of `cb`: the TV's or the GamePad's) after
// the depth clear and the level's texture uploads (GX2CopySurface), which change the GPU's state.
#ifndef RAC1_PORTS_WIIU_MRP_MOBY_GX2_H
#define RAC1_PORTS_WIIU_MRP_MOBY_GX2_H

#include <runtime/gs/gs_draw.h>

#include <gx2/surface.h>

namespace rac1 {

// its shaders loaded (shaders/mrp_moby.gsh, relative to the working directory) and its memory made: after WHBGfxInit.
// false: not loaded (mrp_moby_ready() stays false: the game runs as without it)
bool mrp_moby_gx2_init();
// true once mrp_moby_gx2_init succeeded (mrp_moby_ready() asks it)
bool mrp_moby_gx2_ready();
// the prepared frame's mobys into `cb` / `db` (bound by this call), the game's frame mapped onto `vp` (x / y from the bottom
// left, as gs::draw_gx2's), `db` cleared first; nullptr db: no depth (drawn without the depth test)
void mrp_moby_gx2_draw(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp);

// (testing, RAC1_WIIU_MRP_DEBUG=<m>[,<m>...]: the debug modes in turn, each for 50 game frames from game frame
// RAC1_WIIU_MRP_DEBUG_AT (default 125) on, the last one kept; a mode's bits: 1 no depth test, 2 a plain grey (no texture, blending or alpha
// test), 4 the world's projection (the parts' clip matrices), 16 the model's own x, y as the position (the transforms out), 32 the draws without their indices, 128 the attributes
// fetched as SCALED (not INTEGER), 256 a triangle over the screen from each vertex's index, 512 the depth not cleared, 2048 one draw a call (with
// 256: a screen triangle per draw is too much to fill), 4096 the vertices' colours alone, 8192 the textures alone)
void mrp_moby_gx2_debug_frame(uint64_t game_frame);
// everything it made freed (the level's textures and buffers, the ring, the shaders): before WHBGfxShutdown; it waits for the GPU
void mrp_moby_gx2_shutdown();

}  // namespace rac1

#endif
