// The MRP's drop shadows (ADR 0010 step 4: the special cases).
//
// The original draws a moby's drop shadow with the GS as a counter in the frame's alpha channel: the buffer's alpha cleared to 0x7f,
// every shadow volume (a tube: a ring of points extruded along the light, cut to a slab around the ground, 0x285a68 /
// 0x2860a8 / 0x2861f8 / 0x286848 / 0x285d30) drawn as quads and two caps whose faces multiply the frame's alpha (read back as the
// texture at the pixel itself) by 0x82 / 128 or 0x7f / 128 where they pass the Z test against the frame (a GEQUAL test: nearer than
// the surface), then the frame darkened by FIX 0x20 where the alpha has bit 7 (DATE). The faces of a pixel's alpha net out as the
// sign of (faces marking - faces unmarking): proved over every order of up to 12 faces (the product's integer arithmetic).
//
// The MRP does the same from the data the original computes (the observer of moby_shadows.h: the tubes' world points, the faces'
// layers and clip flags from the scratchpad, the colour words, the darkening's blend): the faces' triangles in the world's
// projection against the world's depth buffer with a stencil counter (INCR_WRAP for a marking face, DECR_WRAP for an unmarking
// one, passing the depth test), then one darkening triangle where the counter is above its start. Host code.
#ifndef RAC1_GAME_RENDER_MRP_SHADOWS_H
#define RAC1_GAME_RENDER_MRP_SHADOWS_H

#include <cstddef>
#include <cstdint>

namespace rac1 {

// orig: (host) 1 (default) the MRP draws the drop shadows (RAC1_MRP_SHADOWS=0 / the debug menu: off)
extern int g_mrp_shadows;

// orig: (host) hooks the drop shadow observer (moby_shadows.h)
void mrp_shadows_install();
// orig: (host) before the game's frame update: clears the frame's capture; `on` when the MRP will draw this frame's mobys
void mrp_shadows_begin_frame(bool on);
// orig: (host) true when this frame's shadows were captured: the host's GS walk skips the shadow tags (`first` / `last`)
bool mrp_shadows_chain(uint32_t& first, uint32_t& last);
// orig: (host) the shadows' pass on the bound framebuffer (the world's projection and depth, the viewport set): called by the MRP
// between the first chain's mobys and the deferred ones, as the chain orders it. True when it drew
bool mrp_shadows_draw(const float* view_proj);
// orig: (host) the frame's faces and darkening captured for a frame drawn after the next update (render_toggles.h
// g_draw_from_snapshot)
void mrp_shadows_snapshot();

struct MrpShadowsWiiuView {
    const float* mark = nullptr;
    size_t mark_floats = 0;
    const float* unmark = nullptr;
    size_t unmark_floats = 0;
    uint64_t apply_alpha = 0;
    uint32_t apply_rgba = 0;
};
bool mrp_shadows_wiiu_view(MrpShadowsWiiuView& out);
void mrp_shadows_clear_snapshot();
// orig: (host) the frame's totals for RAC1_FRAME_PROF / RAC1_MRP_DEBUG
void mrp_shadows_report();
// orig: (host) RAC1_MRP_SHADOW_CHECK: true when the faithful GS path should also walk the shadow tags, so that mrp_shadows_check_gs can compare
bool mrp_shadows_check_enabled();
// orig: (host) the check: the GS path's primitives of this frame (its shadow chain, the alpha counter's faces: TEST_1 0x51001) against the faces the pass
// captured, in order: count, alpha byte, and each vertex's window position and depth through the world's projection against the GS's
void mrp_shadows_check_gs(const void* gs_primitives, int win_w, int win_h, float fb_w, float fb_h, int vp_x, int vp_y, int vp_w, int vp_h, double depth_a,
                          double depth_b, const float* view_proj);

}  // namespace rac1

#endif
