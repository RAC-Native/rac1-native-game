// The MRP's 2D drawn with GX2 on the Wii U (MRP_LITE.md, the 2D on mrp_2d): the frame's primitives as main's chain reader read them
// (src/game/render/mrp_chain.h), built into vertices and runs by mrp_2d.cpp's CPU half (mrp_2d_backend.inc, the same build as the
// PC's) and drawn by mrp_2d_gx2.cpp instead of its OpenGL / Vulkan submit. It replaces the GS replica's GX2 backend
// (gs_draw_gx2.cpp, kept for RAC1_WIIU_REPLICA=1). Host code.
//
// What the caller does on the Wii U (wiiu_gfx.cpp):
//
//   once, after WHBGfxInit:            rac1::mrp_2d_gx2_init();     // shaders/mrp_2d.gsh beside the game's data
//   each picture, after the world and the mobys (draw_frame), the chain's reading joined (mrp_chain_join):
//       rac1::mrp_2d_gx2_draw(rac1::mrp_chain_frame(), cb, fb_w, fb_h, vp, targets);
//     cb: the TV's colour buffer (WHBGfxGetTVColourBuffer: bound by the caller, bound again after the render targets), or the
//     GamePad's with `targets` false (the GamePad's picture samples the targets the TV's call drew: drawing them again would blend
//     the game's drawing into them twice); vp: the 4:3 area of the game's frame (x / y from the bottom left, as gs::draw_gx2's)
//   at a level change (the GPU idle for them):   rac1::mrp_2d_clear_caches();   // (mrp_2d.h: the GX2 textures on the Wii U)
//   at the end, before WHBGfxShutdown:          rac1::mrp_2d_gx2_shutdown();
//
// State left by mrp_2d_gx2_draw: `cb` bound as colour buffer 0, the shader mode GX2_SHADER_MODE_UNIFORM_REGISTER, blending off, every
// channel written, the scissor and the viewport the whole buffer, no depth test, no culling. The depth buffer the caller bound
// stays bound (no test, no writes). A screen copy (GX2CopySurfaceEx) sets the context of `cb` again (the TV's or the GamePad's).
#ifndef RAC1_PORTS_WIIU_MRP_2D_GX2_H
#define RAC1_PORTS_WIIU_MRP_2D_GX2_H

#include <game/render/mrp_chain.h>
#include <runtime/gs/gs_draw.h>

#include <gx2/surface.h>

#include <cstddef>
#include <cstdint>

namespace rac1 {

// its shaders loaded (shaders/mrp_2d.gsh, relative to the working directory) and its memory made: after WHBGfxInit. false: not
// loaded (mrp_2d_gx2_draw then draws nothing)
bool mrp_2d_gx2_init();
bool mrp_2d_gx2_ready();
// the primitives [from, to) of `f` (mrp_2d_draw's: not the effects') into `screen`, the game's fb_w x fb_h frame onto `vp`
//
// (testing, RAC1_WIIU_2D_DEBUG=<bits>: 1 one draw a call - the call's first run -, 2 no textures (the vertices' colours), 4 no
// blending, 8 the screen-reading primitives (DATE, the frame sampled) without the screen's copy (a black, transparent screen),
// 16 those primitives not drawn (as gs_draw_gx2.cpp); RAC1_WIIU_2D_LOG=0: no log line - every 300 calls by default: the primitives
// drawn and left out by reason, the targets, the textures, the screen copies, the CPU's time)
void mrp_2d_gx2_draw(const MrpChainFrame& f, const GX2ColorBuffer* screen, float fb_w, float fb_h, const gs::Viewport& vp, bool targets, size_t from = 0,
                     size_t to = SIZE_MAX);
// ((2026-10-28) RAC1_WIIU_2D_PREBUILD, on by default) the build of the next mrp_2d_gx2_draw into `screen` with these arguments made now
// (mrp2d::build, CPU work alone: no GPU memory written) - wiiu_gfx.cpp calls it for the TV before it waits for the GPU's frame before, which
// the build then overlaps; that draw takes it instead of building
void mrp_2d_gx2_prebuild(const MrpChainFrame& f, const GX2ColorBuffer* screen, float fb_w, float fb_h, const gs::Viewport& vp, size_t from = 0,
                         size_t to = SIZE_MAX);
// (a prebuild no draw took - the TV's frame not drawn - forgotten: the next frame's draw builds its own)
void mrp_2d_gx2_prebuild_drop();
// everything it made freed (its textures, render targets, screen copies, the ring, the shaders): before WHBGfxShutdown; it waits for
// the GPU first unless `gpu_idle` (the game's end, its last frame waited for: a GX2DrawDone there, the application leaving the
// foreground, hung the console)
void mrp_2d_gx2_shutdown(bool gpu_idle = false);

}  // namespace rac1

#endif
