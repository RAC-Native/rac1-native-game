// The GS replica's primitives drawn with GX2 on the Wii U (the game's 2D: the HUD, the text, the menus, the fades); see
// gs_draw_gx2.cpp for what it draws and how a caller uses it. Host code.
#ifndef RAC1_PORTS_WIIU_GS_DRAW_GX2_H
#define RAC1_PORTS_WIIU_GS_DRAW_GX2_H

#include <runtime/gs/gs_draw.h>

#include <gx2/surface.h>

#include <cstddef>
#include <cstdint>

namespace rac1::gs {

// Its shaders loaded (shaders/gs_draw.gsh, relative to the working directory) and its memory made: after WHBGfxInit.
// false: not loaded (draw_gx2 then draws nothing).
bool draw_gx2_init();
// Everything it made freed (its textures, its buffers, the shaders): before WHBGfxShutdown. It waits for the GPU.
void draw_gx2_shutdown();
// The primitives [from, to) of the frame (as rac1::gs::draw, gs_draw.h) into `screen`, the colour buffer bound now (the TV's
// or the GamePad's: bound again after the render targets), the game's fb_w x fb_h frame onto the viewport `vp` (else the whole
// buffer; its x / y from the bottom left, as the GL backend's). `targets`: the GS's drawing into its render targets done by
// this call (the first of a frame's); false: skipped, the targets sampled as an earlier call left them (a second picture of
// the same frame). No depth: the projected 3D (Primitive::vu) is not drawn.
void draw_gx2(const Gs& gs, const GX2ColorBuffer* screen, float fb_w, float fb_h, const Viewport* vp = nullptr, bool targets = true,
              int prim_filter = 0, size_t from = 0, size_t to = SIZE_MAX);
// The GS's blending of a primitive (PRIM's ABE, ALPHA: Cv = (A - B) C + D, FIX as the constant colour) set as GX2's on render
// target 0, as draw_gx2 sets it for the screen (the alpha written the source's); ABE off: blending off. (The MRP's mobys,
// mrp_moby_gx2.cpp.)
void set_blend_gx2(uint64_t prim, uint64_t alpha);
extern uint64_t g_blend_dst_alpha;  // ((2026-11-01) the blendings set that read the destination's alpha, counted)
// Whether that blending reads the source alpha as a factor (in a render target, which keeps the GS's raw alpha, draw_gx2 then draws
// the batch twice: the colour blended with the alpha as 0x80 = 1.0, then the raw alpha alone; mrp_2d_gx2.cpp does the same)
bool blend_reads_src_alpha_gx2(uint64_t prim, uint64_t alpha);

}  // namespace rac1::gs

#endif
