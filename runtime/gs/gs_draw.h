// The host's drawing of the GS emulator's primitives (gs_emu.h) with OpenGL: the GS's texture
// functions, alpha test and blending on the game's frame (its display size) scaled to the window.
#ifndef RAC1_RUNTIME_GS_GS_DRAW_H
#define RAC1_RUNTIME_GS_GS_DRAW_H

#include <runtime/gs/gs_emu.h>

namespace rac1::vk {
struct GraphicsPipelineDesc;
}

namespace rac1::gs {

void draw_init();
// How the host's depth buffer relates to the GS's Z: window depth = a + b * Z (the host knows
// the game's projection). Without it (or for primitives without the Z test) there is no depth.
struct DepthMap {
    double a = 0.0, b = 0.0;
};
// Where the game's frame goes in the window (pixels from the bottom left) and how much it is
// narrowed about its centre (a host widescreen view: the frame's 2D - HUD, menus - kept 4:3).
// With `xscale_3d` != `xscale` (the game's projection widened to the window) the game's projected
// 3D (the primitives VU1 programs kick, or Z-tested at a depth inside the view: particles, glass)
// is drawn at `xscale_3d` instead, and the 2D covering the frame's whole width or height (fades,
// the menus' darkening) reaches the window's edges; the 2D is cut at the frame's edges.
struct Viewport {
    int x = 0, y = 0, w = 0, h = 0;
    float xscale = 1.0f, xscale_3d = 1.0f;
    int screen_fbp = -1;  // the frame's own page (FRAME's FBP): the screen even when sampled
    // a page whose primitives are not drawn (-1: none): during the screen fade the render buffer, which is not
    // the buffer shown then (the fade darkens the display buffer, page 0, and clears the render buffer for the next frame)
    int skip_fbp = -1;
    // the frame's picture an IMAGE transfer put back (a type-7 primitive, Gs::set_screen_area) drawn by the host:
    // called with the screen's framebuffer bound, the scissor test off and every colour channel written; nullptr: not drawn
    void (*screen_picture)(const TexImage& img, const Viewport& vp) = nullptr;
    // the projected 3D cut at the frame's 4:3 edges as the 2D (the full-screen menu's mobys: a part of its 4:3 layout)
    bool cut_3d = false;
    // the frame pages from this one up (the z buffer's and the textures' memory: the menus' render targets) are never the
    // screen: drawn into a target even when this frame does not sample them (-1: only the sampled ones)
    int rt_min_fbp = -1;
    // a type-8 primitive (the host's 3D in the primitives' order: Gs::SkipRange::host_3d) drawn into a render target:
    // called with that target bound (`w` x `h` pixels, one per GS pixel, row 0 at its texture's row 0), its depth buffer attached
    // (cleared to the far end when `first`: the first of a run of type-8 primitives into it, after the GS's own drawing there),
    // the viewport the target's, the scissor test off; the GL state set again after it. nullptr, or a type-8 into the screen: not drawn
    void (*host_3d)(const Primitive& p, int w, int h, bool first) = nullptr;
    // the frame's 2D cut to the area a TV showed (the overscan crop, a host presentation choice): the 2D on the
    // screen (not the projected 3D, not a render target, not the 2D covering the frame's whole width or height: fades,
    // darkenings, the letterbox) is cut at these margins, in the game's frame pixels (left, right, top, bottom); 0: the full frame
    int crop[4] = {0, 0, 0, 0};
    // (the modern view; off for the faithful reference) the 2D slivers at the frame's edges not drawn: a textured 2D primitive
    // of which at most 4 frame pixels are inside the frame, the rest past its left, right or top edge (a HUD element parked
    // there as it hides: the Suck Cannon's gauge and the air meter keep their 3 leftmost columns at x 509..511 for 16 frames,
    // which a TV's overscan hid). Not the projected 3D, render targets, untextured fills (a wipe's pieces), the full-span 2D
    bool hide_edge_slivers = false;
    // (the modern view; off for the faithful reference) a font's glyph samples only the texels of its own cell: its bilinear
    // filtering, scaled up, no longer blends in the texels beyond it, the neighbouring glyphs (issue #76; see draw())
    bool sprite_clamp = false;
    // (a HUD wider than 4:3, Options > Graphics > HUD) the HUD's elements anchored at the frame's sides (Primitive::hud) moved by
    // this many frame pixels toward their side: the left ones left, the right ones right, their scissor with them (past the
    // frame's edges, never past the window's). 0: the HUD at the 4:3 frame's edges, as the original
    float hud_shift = 0.0f;
};
// The primitives over the current frame, the game's fb_w x fb_h frame onto the viewport
// (`vp`, else the whole win_w x win_h); with `depth`, the Z-tested ones against the host's
// depth buffer (GEQUAL / GREATER as the host's LEQUAL / LESS, Z written unless ZMSK).
// only the primitives [from, to) of the list; those the MRP draws (Primitive::fx with Gs::fx_external()) never.
void draw(const Gs& gs, int win_w, int win_h, float fb_w, float fb_h, const DepthMap* depth = nullptr, const Viewport* vp = nullptr, int prim_filter = 0,
          size_t from = 0, size_t to = SIZE_MAX);

uint32_t field(uint64_t v, int lo, int n);
uint32_t texture_for(const Gs& gs, uint64_t tex0, uint64_t texa, const TexImage* img);
void blend_state(const DrawState& st, bool to_rt = false);
void vk_blend_state(const DrawState& st, bool to_rt, vk::GraphicsPipelineDesc& desc, float& out_fix);
void vk_draw_shutdown();

}  // namespace rac1::gs

#endif
