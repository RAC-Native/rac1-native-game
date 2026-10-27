// The host's drawing of the GS emulator's primitives (gs_emu.h) with OpenGL: the GS's texture
// functions, alpha test and blending on the game's frame (its display size) scaled to the window.
#ifndef RAC1_RUNTIME_GS_GS_DRAW_H
#define RAC1_RUNTIME_GS_GS_DRAW_H

#include <runtime/gs/gs_blend.h>
#include <runtime/gs/gs_emu.h>

namespace rac1::gs {

void draw_init();
// The primitives over the current frame, the game's fb_w x fb_h frame onto the viewport
// (`vp`, else the whole win_w x win_h); with `depth`, the Z-tested ones against the host's
// depth buffer (GEQUAL / GREATER as the host's LEQUAL / LESS, Z written unless ZMSK).
// only the primitives [from, to) of the list; those the MRP draws (Primitive::fx with Gs::fx_external()) never.
void draw(const Gs& gs, int win_w, int win_h, float fb_w, float fb_h, const DepthMap* depth = nullptr, const Viewport* vp = nullptr, int prim_filter = 0,
          size_t from = 0, size_t to = SIZE_MAX);

// (host) the drawing's textures forgotten (a level change, between frames; Vulkan's destroyed once the frames using them are done):
// their count before
size_t draw_clear_caches();

uint32_t texture_for(const Gs& gs, uint64_t tex0, uint64_t texa, const TexImage* img);
void vk_draw_shutdown();

}  // namespace rac1::gs

#endif
