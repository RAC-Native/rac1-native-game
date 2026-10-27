// The GS replica's primitives drawn with GX2 on the Wii U (the game's 2D: the HUD, the text, the menus, the fades); see
// gs_draw_gx2.cpp for what it draws and how a caller uses it. Host code.
#ifndef RAC1_PORTS_WIIU_GS_DRAW_GX2_H
#define RAC1_PORTS_WIIU_GS_DRAW_GX2_H

#include <runtime/gs/gs_draw.h>

#include <cstddef>
#include <cstdint>

namespace rac1::gs {

// Its shaders loaded (shaders/gs_draw.gsh, relative to the working directory) and its memory made: after WHBGfxInit.
// false: not loaded (draw_gx2 then draws nothing).
bool draw_gx2_init();
// Everything it made freed (its textures, its buffers, the shaders): before WHBGfxShutdown. It waits for the GPU.
void draw_gx2_shutdown();
// The primitives [from, to) of the frame (as rac1::gs::draw, gs_draw.h) into the GX2 colour buffer bound now, `win_w` x
// `win_h` pixels (the TV's or the GamePad's), the game's fb_w x fb_h frame onto the viewport `vp` (else the whole buffer;
// its x / y from the bottom left, as the GL backend's). No depth: the projected 3D (Primitive::vu) is not drawn.
void draw_gx2(const Gs& gs, int win_w, int win_h, float fb_w, float fb_h, const Viewport* vp = nullptr, int prim_filter = 0,
              size_t from = 0, size_t to = SIZE_MAX);

}  // namespace rac1::gs

#endif
