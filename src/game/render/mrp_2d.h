// The MRP's 2D (host code): the HUD, text, menus, fades, tints, the menus' pictures the game draws into the GS's memory, as the
// MRP's chain reader found them (mrp_chain.h: the GIF packets read, each primitive with the GS state it was kicked with), drawn
// with OpenGL 4.6 or Vulkan 1.3.
//
// A call builds the frame's vertices and draw runs on the CPU first (one pass, the same for both backends: the widened view's
// rules - the HUD's shift, the full-span primitives, the screen layers, the edge slivers -, the glyphs' sampling in their cells
// (#76), the render targets' sizes), then submits them: the vertices in one buffer, one draw per run of primitives with the same
// state, in the chain's order. The GS's own pixel arithmetic is kept where the picture depends on it: its texture functions,
// alpha test, the ALPHA register's (A - B) C + D as the GPU's blending, the render targets keeping the GS's alpha (0x80 = 1.0).
// The textures come from the GS's local memory decoded once (gs::LocalMemory::texture_at_kick), one GPU texture per decoded
// image (a texture the game uploads again unchanged is the same image), forgotten when unused for a while or at a level change.
#ifndef RAC1_GAME_RENDER_MRP_2D_H
#define RAC1_GAME_RENDER_MRP_2D_H

#include <game/render/mrp_chain.h>
#include <runtime/gs/gs_types.h>

#include <cstddef>

namespace rac1 {

// orig: (host) the primitives [from, to) of `f` that are not the effects' (mrp_fx draws those) over the current frame: the game's
// fb_w x fb_h frame onto the viewport `vp` of the win_w x win_h window; with `depth`, the Z-tested ones against the host's depth
// buffer (window depth = a + b Z)
void mrp_2d_draw(const MrpChainFrame& f, int win_w, int win_h, float fb_w, float fb_h, const gs::DepthMap* depth, const gs::Viewport* vp, size_t from = 0,
                 size_t to = SIZE_MAX);
// orig: (host) the 2D's GPU textures and render targets forgotten (a level change; the GPU idle for them: Vulkan's are destroyed
// when the frames using them are done). Returns the textures' count before.
size_t mrp_2d_clear_caches();
// orig: (host) the textures held now (the profiler, the tests)
size_t mrp_2d_texture_count();

}  // namespace rac1

#endif
