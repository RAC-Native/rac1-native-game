// The ship's draw callbacks: the hand-written piece and the registration (see ship_draw_cbs.h).
#include <game/ovl/ship_draw_cbs.h>

#include <game/ovl/aridia_ship.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vu0_macro.h>

namespace rac1 {

namespace {
// orig: (helper) repeated in 0x002002e0: its vf3.y at the return, the radius (x the camera's scale 0x16ccf0.w) past the
// near plane (vf1.w + the view's z)
float point_screen_vf3y(uint32_t pt, float r) {
    const Vec4 cam = V(OV(0x0016ccf0));
    const Vec4 r0 = V(OV(0x0016ccc0)), r1 = V(OV(0x0016ccd0)), r2 = V(OV(0x0016cce0));
    const Vec4 q = V(pt);
    const Vec4 p = Vec4{vuc(q.x * cam.w), vuc(q.y * cam.w), vuc(q.z * cam.w), vuc(q.w * cam.w)};
    const float dx = vuc(p.x - cam.x), dy = vuc(p.y - cam.y), dz = vuc(p.z - cam.z);
    const float vz = vuc(vuc(vuc(r0.z * dx) + vuc(r1.z * dy)) + vuc(r2.z * dz));
    const float rr = vuc(r * cam.w);
    const float near_y = vuc(0.0f - p.w);
    const float s1y = vuc(near_y + vz);
    return vuc(rr - s1y);
}
}  // namespace

// The point's test 0x2002e0; its result back. Then (the delay slot's VFTOI0.y runs on both paths) off the screen *out = 0, else
// *out = the vf3.y integer's word >> 7 (DSRL32 7 of the 64-bit pair: logical) clamped by PMAXW / PMINW to 0..0x80.
// orig: 0x002003d8
int64_t point_on_screen_depth(uint32_t pt, uint32_t out, float r) {
    const int64_t v = point_on_screen(pt, r);
    if (v < 0) {
        sw(out, 0);
        return v;
    }
    const uint32_t y = vu0m::ftoi(point_screen_vf3y(pt, r), 1.0f);
    const int32_t w = static_cast<int32_t>(y >> 7);  // (the pair's high word shifted down: the bits above it are zero)
    sw(out, static_cast<uint32_t>(std::min(std::max(w, 0), 0x80)));
    return v;
}

// orig: (none) — registers the ship's draw callbacks for the indirect calls (ovl_call_moby_fn)
void ship_draw_cbs_register() {
    // the hand-written ports of Aridia's copies, proved against Veldin's at 0x28e670 / 0x28efc8 / 0x28f238 too (batch 503)
    ovl_register_moby_fn(0x0028e670, ari_ship_glow_draw_l2);
    ovl_register_moby_fn(0x0028efc8, ari_ship_engine_glow_draw_l2);
    ovl_register_moby_fn(0x0028f238, ari_ship_takeoff_draw_l2);
}

}  // namespace rac1
