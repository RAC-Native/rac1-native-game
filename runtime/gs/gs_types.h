// The GS's drawing as plain data: a kicked vertex, the GS state a primitive was kicked with, a texture as the local memory
// held it, a primitive, and VU1 program 7's batches. Made by the GS replica's walk (gs_emu.h, the faithful reference) and by the
// MRP's own chain reader (src/game/render/mrp_chain.h); read by their drawers.
#ifndef RAC1_RUNTIME_GS_GS_TYPES_H
#define RAC1_RUNTIME_GS_GS_TYPES_H

#include <cstdint>
#include <memory>
#include <vector>

namespace rac1::gs {

struct Vertex {
    float x = 0, y = 0;   // pixels, XYOFFSET removed
    uint32_t z = 0;
    uint8_t r = 0, g = 0, b = 0, a = 0;
    float q = 1.0f, s = 0, t = 0;  // ST / Q
    float u = 0, v = 0;            // UV, texels
    uint8_t fog = 0;
};

// The drawing state a primitive was kicked with (raw GS register values, the context's).
struct DrawState {
    uint64_t prim = 0, tex0 = 0, tex1 = 0, clamp = 0, alpha = 0, test = 0, scissor = 0;
    uint64_t texa = 0, fba = 0, frame = 0, zbuf = 0, colclamp = 0, pabe = 0, fogcol = 0, dthe = 0;
    uint64_t miptbp1 = 0;  // MIPTBP1: the mip levels 1..3's bases / widths
    uint64_t xyoffset = 0;  // (host) XYOFFSET of the context: the kicked X / Y are the vertices' x / y x 16 + its x / y
};

// A texture as the local memory held it when a primitive sampled it (TEX0's size, CLUT and
// TEXA applied, RGBA with the raw alpha): the memory is rewritten during a frame (the same
// blocks reused by later uploads), so a texture is read at its draw, as the GS does.
struct TexImage {
    uint64_t id = 0;  // unique per decode
    int w = 0, h = 0;
    std::vector<uint32_t> px;
};

struct Primitive {
    // 0 point, 1 line, 2 line strip, 3 triangle, 4 strip, 5 fan, 6 sprite (assembled: 1 line, 3 triangle, 6 sprite);
    // (host) 7: IMAGE transfers into the frame (Gs::set_screen_area, MrpChainInput::scr_*): `tex` the frame's picture after them
    // (fbw x 64 by the frame's height, CT32 as the memory holds it), two vertices at its corners;
    // (host) 8: the host's 3D drawn here (a skip range's host_3d in fx_id), no vertices
    int type = 0;
    DrawState st;
    std::vector<Vertex> v;  // 2 (line, sprite) or 3 (triangle)
    std::shared_ptr<const TexImage> tex;  // textured: its texture at the kick (mipmapped: level mip_lo)
    // mipmapped (TEX1 MXL > 0): the levels mip_lo + 1 .. mip_lo + mip_n its pixels may sample (the
    // GS picks the level per pixel from Q), their TEX0 (base / width / size of the level)
    int mip_lo = 0, mip_n = 0;
    uint64_t mip_tex0[3] = {};
    std::shared_ptr<const TexImage> mip_tex[3];
    bool vu = false;    // kicked by a VU1 program (XGKICK, path 1): the game's projected 3D
    bool moby = false;  // kicked specifically by the moby VU1 program
    bool wide = false;  // (host) from a substituted chain (Gs::add_chain_substitute, MrpChainInput::subs): drawn past the frame's sides
    // (host) 1: from a substitute, 2: from the real range it stands for. The drawing takes the projected 3D from the
    // substitute and the 2D (the HUD, the text: the substitute's may use VRAM its texture uploads did not reach) from the real range.
    uint8_t sub = 0;
    uint32_t vu_start = 0;  // the micro address of the VU1 program that kicked it (testing: RAC1_GS_PRIMDBG)
    // (host) kicked inside an effects range (Gs::set_chain_fx, MrpChainInput::fx): the MRP draws it (mrp_fx.h); its value the range's kind
    // (the producer, whose projection takes the vertices back to the world), fx_id the range's id
    uint8_t fx = 0;
    uint32_t fx_id = 0;
    // (host) kicked inside a screen layer's range (Gs::set_chain_layers, MrpChainInput::layers): a full-screen 2D layer the game draws over its
    // 4:3 frame, stretched across a widened view (gs_draw.cpp)
    bool layer = false;
    // (host) kicked inside a HUD element's range (Gs::set_chain_hud, MrpChainInput::hud): 1 an element at the frame's left, 2 at its right; moved
    // toward the edges of a HUD wider than 4:3 (Viewport::hud_shift, gs_draw.cpp). 0: none (centred, or not the HUD)
    uint8_t hud = 0;
};

// (host) one batch of VU1 program 7 (the shadow-mesh program, shadows_vu1.cpp) as the program found it in VU1 memory, before it
// transformed it in place and clipped it at the guard band (Gs::set_chain_vu7, MrpChainInput::vu7): the MRP draws these itself, in the world.
struct Vu7Vertex {
    float xyz[3];    // the input point (w 1), the space the program's screen matrix takes
    float st[3];     // S, T, Q as the batch holds them (Q 1)
    uint8_t rgba[4];
};
struct Vu7Batch {
    DrawState st;                          // the GS state its kick would have (PRIM from the batch's GIF tag)
    std::shared_ptr<const TexImage> tex;   // textured: its texture (base level) at the start
    bool mip = false;                      // (TEX1 asked for mip levels: only the base level is kept)
    float m[16] = {};                      // the screen matrix vf18..vf21 (column c = vf[18 + c])
    float off[4] = {};                     // vf30: the screen offset (w: the fog's offset)
    float k = 0;                           // vf31.w: the Q numerator
    float fog[2] = {};                     // vf7.x / .y: the fog's clamp
    float clip[16] = {};                   // the clip matrix vf22..vf25 (the guard band's): its 0xe start clips what crosses it
    // the clipped triangles' GS state: the program kicks them as fans under its own GIF tag (vf31, 0x3ac: PRIM 0x7d, context 1),
    // whatever the batch's PRIM (a second pass in context 2 draws its clipped triangles with context 1's texture and blending)
    DrawState st_fan;
    std::shared_ptr<const TexImage> tex_fan;
    // per vertex of its strip as the program kicked it: 1 drawn, 0 its ADC set (the triangle ending there clipped - drawn as a fan -
    // or past the guard band); empty when the kick was not seen whole
    std::vector<uint8_t> kicked;
    uint32_t first = 0, n = 0;             // its vertices in Gs::vu7_vertices() / MrpChainFrame::vu7_verts
    uint32_t start = 0;                    // 0xc (no clipping) or 0xe (the clipping path)
    uint32_t tag = 0;                      // its vu7 range's (Gs::Vu7Range::tag)
    uint8_t sub = 0;                       // as Primitive::sub
};

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

}  // namespace rac1::gs

#endif
