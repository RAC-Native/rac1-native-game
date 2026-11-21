// Drek's fleet's (level 17) class 1772, ported by hand from the level's own code: a far backdrop of light beams. It sets
// the level's far distance (L17:0x15f638: 150 while Ratchet walks inside its box pvars +0x4c, 10 when he flies the ship
// or is outside it), lays out its beams once (from its place, pvars +0, to a point far away, +0x10) and then moves the
// beams' shared phases in the level's own tables every frame for its draw callback (L17:0x2f32f0, queued each frame).
// The original's order of float operations kept. Level 17 only: `orig: L17:`, the level's own data L17_ADDR, base ELF
// bare. Registered in veldin_orbit_mobys.cpp.
//
// The level's tables (five beams): L17:0x1de0d0 their angles (laid out 0, 25, .. 100 and turned by 0x1ddfe0 x 0.1 a
// second), 0x1de040 their fades (0 .. 1, moved by 0x1de070 x the sign 0x1de058, which turns at either end); two
// scrolls 0x1de0a0 (moved by 0x1623c0, wrapped into [0, 1)). The tuning words L17:0x1623d0 / 0x1623d4 / 0x1623d8 give the
// far point's two lengths.
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// the frame's time scale as the beams read it (1/60, 1/50 at 50 Hz)
float frame_scale() { return lwc1(0x0015ed7c); }

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his place, +0x15f0 his vehicle, +0x2084 his state)

// an EE stack frame of the original's size (its vectors are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
}  // namespace

// orig: L17:0x002f3848
void vo_class1772_update_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x4c) != -1) {
        const uint32_t ship = lwu(P + 0x15f0);
        const bool flying = lw(P + 0x2084) == 0x32 && ship != 0 && static_cast<int16_t>(lhu(ship + 0xa6)) == 0x563;
        swc1(L17_ADDR(0x0015f638), !flying && point_in_box(P + 0x80, lw(pv + 0x4c)) != 0 ? 150.0f : 10.0f);
    }

    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        for (int32_t i = 0; i < 5; ++i)
            swc1(L17_ADDR(0x001de0d0) + static_cast<uint32_t>(i) * 4, fpu_mul(static_cast<float>(i), 25.0f));
        // the far point: the moby's place moved along its yaw, then along the pvars' yaw +0x44, 300 lower in x
        const Frame f(160);
        const uint32_t a = f.sp, b = f.sp + 0x10, sum = f.sp + 0x20;
        lq_sq(pv, m + 0x10);
        vec_from_spherical_v(V(a), fpu_div(fpu_add(lwc1(L17_ADDR(0x001623d4)), lwc1(L17_ADDR(0x001623d0))), lwc1(L17_ADDR(0x001623d8))),
                             lwc1(m + 0x48), 0.0f);
        vec_from_spherical_v(V(b), fpu_div(lwc1(L17_ADDR(0x001623d0)), lwc1(L17_ADDR(0x001623d8))), lwc1(pv + 0x44), 0.0f);
        vec3_add(V(sum), V(a), V(b));
        lq_sq(a, sum);
        vec3_add(V(sum), V(pv), V(a));
        lq_sq(pv + 0x10, sum);
        sb(m + 0x30, 0xff);
        swc1(pv + 0x10, fpu_sub(lwc1(pv + 0x10), 300.0f));
        return;
    }

    // the fades: moved, turned round at 1 and at 0
    const float fs = frame_scale();
    for (uint32_t k = 0; k < 5 * 4; k += 4) {
        const uint32_t fade = L17_ADDR(0x001de040) + k, sign = L17_ADDR(0x001de058) + k;
        const float v = fpu_add(lwc1(fade), fpu_mul(fpu_mul(lwc1(L17_ADDR(0x001de070) + k), lwc1(sign)), fs));
        swc1(fade, v);
        if (1.0f < v) {
            swc1(fade, 1.0f);
            swc1(sign, -1.0f);
        }
        if (lwc1(fade) < 0.0f) {
            sw(fade, 0);
            swc1(sign, 1.0f);
        }
    }
    // the angles turned
    for (uint32_t k = 0; k < 5 * 4; k += 4) {
        const uint32_t angle = L17_ADDR(0x001de0d0) + k;
        swc1(angle, angle_add(lwc1(angle), fpu_mul(fpu_mul(lwc1(L17_ADDR(0x001ddfe0) + k), u2f(0x3dcccccdu)), lwc1(0x0015ed7c))));
    }
    // the two scrolls, wrapped into [0, 1)
    const float fs2 = frame_scale();
    for (uint32_t k = 0; k < 2 * 4; k += 4) {
        const uint32_t scroll = L17_ADDR(0x001de0a0) + k;
        const float v = fpu_add(lwc1(scroll), fpu_mul(lwc1(L17_ADDR(0x001623c0) + k), fs2));
        swc1(scroll, v);
        if (1.0f <= v) swc1(scroll, fpu_sub(v, 1.0f));
        const float w = lwc1(scroll);
        if (w < 0.0f) swc1(scroll, fpu_add(w, 1.0f));
    }
    sub_001f93e0(L17_ADDR(0x002f32f0), m);  // (its draw, vo_cb_2f32f0_l17)
}

}  // namespace rac1
