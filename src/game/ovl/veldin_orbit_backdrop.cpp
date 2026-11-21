// Drek's fleet's (level 17) class 1772, ported by hand from the level's own code: a far backdrop of light beams. It sets
// the level's far distance (L17:0x15f638: 150 while Ratchet walks inside its box pvars +0x4c, 10 when he flies the ship
// or is outside it), lays out its beams once (from its place, pvars +0, to a point far away, +0x10) and then moves the
// beams' shared phases in the level's own tables every frame for its draw callback (L17:0x2f32f0, queued each frame, below).
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
#include <game/ovl/quad_draw.h>
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

// Class 1772's draw callback: the GS register 0x47 set to 0x513f1 for the draw (0x5360b again after it), the level's far
// word L17:0x16d3e4 = 8192000 and the far point's height (pvars +0x14) = -8000; then two long quads from the moby's place
// (pvars +0) to the far point (+0x10), facing the camera (L17:0x1676c0), as wide as 0x1de0b8[i] at the place and +0x28 at the
// far end, the effect texture 0x1de088[i], the colour 0x1623b8, the (s, t) pairs L17:0x1e6fa0 with s scrolled by 0x1de0a0[i]
// (the scroll then moved by 0x1623c0[i] x the beams' frame scale 0x15ed7c); then five flat squares 1000 above the far point,
// each 0x1ddff8[j] wide, the effect texture 0x2f, their (s, t) on a circle turned by 0x1de0d0[j], the colour 0x1de010[j] ..
// 0x1de028[j] by the fade 0x1de040[j]. The original's frame (0x1b0) and offsets kept: +0 the quad's descriptor for quad_draw
// (it reads 0x90: the points +0 .. +0x30, the colours +0x40, the (s, t) +0x50, the GS words +0x70), +0x90 / +0xa0 the two
// sides, +0xb0 the beam, +0xc0 a corner (vec3_sub / vec3_cross_ba / vec3_set_length / vec3_add: 16 each). The additions of
// 0.0 to the squares' corners kept (they make -0 +0). VU0: no VU0 instruction of its own; its callees are the host ports
// the generated code called (vec3_*, angle_add, sin_vu, cos_vu, rgba_lerp, quad_draw), which keep no VU0 state.
// orig: L17:0x002f32f0
void vo_cb_2f32f0_l17(uint32_t m) {
    const Frame f(0x1b0);
    const uint32_t q = f.sp, side_a = f.sp + 0x90, side_b = f.sp + 0xa0, beam = f.sp + 0xb0, corner = f.sp + 0xc0;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t a = pv, b = pv + 0x10, cam = L17_ADDR(0x001676c0);
    sub_002a0ba8(0x47, 0x513f1u);
    swc1(L17_ADDR(0x0016d3e4), 8192000.0f);
    swc1(pv + 0x14, -8000.0f);
    // the two long beams
    for (uint32_t i = 0; i < 2; ++i) {
        vec3_sub(V(beam), V(a), V(b));
        vec3_sub(V(side_a), V(a), V(cam));
        vec3_sub(V(side_b), V(b), V(cam));
        vec3_cross_ba(V(side_a), V(side_a), V(beam));
        vec3_set_length(V(side_a), V(side_a), lwc1(L17_ADDR(0x001de0b8) + i * 4));
        vec3_cross_ba(V(side_b), V(side_b), V(beam));
        vec3_set_length(V(side_b), V(side_b), lwc1(pv + 0x28));
        sd(q + 0x80, (0xff90ull << 32) | 0x260ull);  // TEX1
        sd(q + 0x88, (0x80ull << 32) | 0x48ull);     // ALPHA
        const uint32_t rgba = lwu(L17_ADDR(0x001623b8));
        for (uint32_t k = 0; k < 4; ++k) sw(q + 0x40 + k * 4, rgba);
        sd(q + 0x70, 0);  // CLAMP
        // (the original first copies the (s, t) pairs plain, then again with the scroll on s: only the second is drawn)
        vec3_sub(V(corner), V(a), V(side_a));
        lq_sq(q, corner);
        vec3_add(V(corner), V(a), V(side_a));
        lq_sq(q + 0x10, corner);
        vec3_sub(V(corner), V(b), V(side_b));
        lq_sq(q + 0x20, corner);
        vec3_add(V(corner), V(b), V(side_b));
        lq_sq(q + 0x30, corner);
        sd(q + 0x78, effect_tex0(lw(L17_ADDR(0x001de088) + i * 4)));
        const uint32_t scroll = L17_ADDR(0x001de0a0) + i * 4;
        const float s = lwc1(scroll);
        for (uint32_t k = 0; k < 4 * 8; k += 8) {
            swc1(q + 0x50 + k, fpu_add(lwc1(L17_ADDR(0x001e6fa0) + k), s));
            swc1(q + 0x54 + k, lwc1(L17_ADDR(0x001e6fa4) + k));
        }
        swc1(scroll, fpu_add(lwc1(scroll), fpu_mul(lwc1(L17_ADDR(0x001623c0) + i * 4), lwc1(0x0015ed7c))));
        quad_draw(q, 0);
    }
    // the five squares above the far point
    sd(q + 0x78, effect_tex0(0x2f));
    for (uint32_t j = 0; j < 5; ++j) {
        const float w = lwc1(L17_ADDR(0x001ddff8) + j * 4);
        const float turn = lwc1(L17_ADDR(0x001de0d0) + j * 4);
        const float h = fpu_mul(w, 0.5f);
        const float x = fpu_sub(lwc1(pv + 0x10), h), y = fpu_add(lwc1(pv + 0x14), 1000.0f), z = fpu_add(lwc1(pv + 0x18), h);
        const float xs[4] = {fpu_add(x, 0.0f), fpu_add(x, 0.0f), fpu_add(x, w), fpu_add(x, w)};
        const float zs[4] = {fpu_add(z, 0.0f), fpu_sub(z, w), fpu_add(z, 0.0f), fpu_sub(z, w)};
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t p = q + k * 0x10;
            swc1(p + 0x0, xs[k]);
            swc1(p + 0x4, y);
            swc1(p + 0x8, zs[k]);
            swc1(p + 0xc, 1.0f);
        }
        // the corners' (s, t) on a circle of radius 0.5 about (0.5, 0.5), a quarter turn apart
        static constexpr uint32_t quarter[4] = {0x4096cbe4u, 0x40490fdbu, 0, 0x3fc90fdbu};  // (3 pi / 2, pi, 0, pi / 2)
        for (uint32_t k = 0; k < 4; ++k) {
            const float t = k == 2 ? turn : angle_add(turn, u2f(quarter[k]));
            swc1(q + 0x50 + k * 8, fpu_add(fpu_mul(sin_vu(t), 0.5f), 0.5f));
            swc1(q + 0x54 + k * 8, fpu_add(fpu_mul(cos_vu(t), 0.5f), 0.5f));
        }
        const uint32_t rgba = rgba_lerp(lwu(L17_ADDR(0x001de010) + j * 4), lwu(L17_ADDR(0x001de028) + j * 4), lwc1(L17_ADDR(0x001de040) + j * 4));
        for (uint32_t k = 0; k < 4; ++k) sw(q + 0x40 + k * 4, rgba);
        quad_draw(q, 0);
    }
    sub_002a0ba8(0x47, 0x5360bu);
}

}  // namespace rac1
