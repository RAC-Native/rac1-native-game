// The Vendor moby's world draw callback (class 11, Veldin 0x2a5040, shared across levels via OV() relocation):
// the four camera-facing hologram frame quads (effect texture 0x18, scrolling along 0x161394, corners and UVs at
// 0x1d7360..0x1d749c), the rising hologram pulse quad (effect texture 0x1b, phase 0x161398), and the four corner
// glows on the Vendor's base. Declarations in shared_fx_tr.h.
#include <game/ovl/shared_fx_tr.h>

#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/glows.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// orig: 0x002a5040
void vendor_draw_tr(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x290);
    const uint32_t d = sp;
    const uint32_t t = sp + 0x90;
    const uint32_t euler = sp + 0xd0;
    const uint32_t rot = sp + 0xe0;
    const uint32_t ring = sp + 0x110;
    const uint32_t pt = sp + 0x150;
    const uint32_t pv = lwu(m + 0x78);

    sw(euler + 0, 0);
    sw(euler + 4, 0);
    swc1(euler + 8, lwc1(OV(0x00166dd8)));
    euler_to_mat3_vu(M(rot), V(euler));

    const float scroll_next = fpu_add(lwc1(OV(0x00161394)), u2f(0x3c23d70au));
    swc1(OV(0x00161394), scroll_next);
    if (1.0f < scroll_next) {
        swc1(OV(0x00161394), fpu_sub(scroll_next, 1.0f));
    }

    sd(d + 0x80, 1);
    sd(d + 0x88, 0x4000000044ull);
    sd(d + 0x70, 0);
    sd(d + 0x78, effect_tex0(0x18));

    const uint32_t points = OV(0x001d7360);
    const uint32_t texels = OV(0x001d73f0);
    const uint32_t colours = OV(0x001d7438);
    const uint32_t quads = OV(0x001d7460);

    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t q = quads + i * 16;
        uint32_t idx[4];
        for (uint32_t c = 0; c < 4; ++c) {
            idx[c] = lwu(q + c * 4);
            mat3_transform(V(t + c * 0x10), V(points + (idx[c] << 4)), M(rot));
        }
        const float px = lwc1(m + 0x10), py = lwc1(m + 0x14), pz = lwc1(m + 0x18);
        for (uint32_t c = 0; c < 4; ++c) {
            const float scale = lwc1(pv + 0x90);
            swc1(d + c * 0x10 + 0, fpu_add(px, fpu_mul(lwc1(t + c * 0x10 + 0), scale)));
            swc1(d + c * 0x10 + 4, fpu_add(py, fpu_mul(lwc1(t + c * 0x10 + 4), scale)));
            swc1(d + c * 0x10 + 8, fpu_add(pz, lwc1(t + c * 0x10 + 8)));
        }
        const float s = lwc1(OV(0x00161394));
        for (uint32_t c = 0; c < 4; ++c) {
            sw(d + 0x40 + c * 4, lwu(colours + idx[c] * 4));
            swc1(d + 0x50 + c * 8, lwc1(texels + idx[c] * 8));
            swc1(d + 0x54 + c * 8, fpu_add(lwc1(texels + 4 + idx[c] * 8), s));
        }
        quad_draw(d, 0);
    }

    sd(d + 0x78, effect_tex0(0x1b));
    const float pulse_next = fpu_add(lwc1(OV(0x00161398)), u2f(0x3c23d70au));
    swc1(OV(0x00161398), pulse_next);
    if (1.0f < pulse_next) {
        sw(OV(0x00161398), 0);
    }

    const float phase = lwc1(OV(0x00161398));
    const float pos_xy = fpu_mul(phase, u2f(0x3f99999au));
    const float neg_xy = fpu_mul(-phase, u2f(0x3f99999au));
    const float z_hi = fpu_add(fpu_mul(phase, u2f(0x3fb33333u)), u2f(0x3f8ccccdu));
    const float z_lo = fpu_sub(z_hi, u2f(0x3e4ccccdu));
    V(ring + 0x00) = Vec4{fpu_mul(neg_xy, lwc1(pv + 0x90)), fpu_mul(neg_xy, lwc1(pv + 0x90)), z_hi, 0.0f};
    V(ring + 0x10) = Vec4{fpu_mul(pos_xy, lwc1(pv + 0x90)), fpu_mul(neg_xy, lwc1(pv + 0x90)), z_lo, 0.0f};
    V(ring + 0x20) = Vec4{fpu_mul(neg_xy, lwc1(pv + 0x90)), fpu_mul(pos_xy, lwc1(pv + 0x90)), z_hi, 0.0f};
    V(ring + 0x30) = Vec4{fpu_mul(pos_xy, lwc1(pv + 0x90)), fpu_mul(pos_xy, lwc1(pv + 0x90)), z_lo, 0.0f};

    swc1(euler + 0, 0.0f);
    swc1(euler + 4, 0.0f);
    swc1(euler + 8, lwc1(OV(0x00166dd8)));
    euler_to_mat3_vu(M(rot), V(euler));
    for (uint32_t c = 0; c < 4; ++c) {
        mat3_transform(V(t + c * 0x10), V(ring + c * 0x10), M(rot));
    }

    const uint32_t alpha = f2u(eer::cvt_w(fpu_mul(fpu_sub(1.0f, lwc1(OV(0x00161398))), 128.0f)));
    const uint32_t col = (alpha << 24) | 0x808080u;
    const float px = lwc1(m + 0x10), py = lwc1(m + 0x14), pz = lwc1(m + 0x18);
    for (uint32_t c = 0; c < 4; ++c) {
        swc1(d + c * 0x10 + 0, fpu_add(px, lwc1(t + c * 0x10 + 0)));
        swc1(d + c * 0x10 + 4, fpu_add(py, lwc1(t + c * 0x10 + 4)));
        swc1(d + c * 0x10 + 8, fpu_add(pz, lwc1(t + c * 0x10 + 8)));
        sw(d + 0x40 + c * 4, col);
    }
    swc1(d + 0x50, 0.0f);
    swc1(d + 0x54, 0.0f);
    swc1(d + 0x58, 1.0f);
    swc1(d + 0x5c, 0.0f);
    swc1(d + 0x60, 0.0f);
    swc1(d + 0x64, 1.0f);
    swc1(d + 0x68, 1.0f);
    swc1(d + 0x6c, 1.0f);
    quad_draw(d, 0);

    for (int32_t i = 0; i < 4; ++i) {
        const float pi = u2f(0x40490fd0u);
        const float ang = fpu_sub(fpu_mul(fpu_mul(static_cast<float>(i), pi), 0.5f), pi);
        swc1(pt + 0, fpu_mul(cos_vu(ang), u2f(0x3f8ccccdu)));
        swc1(pt + 4, fpu_mul(sin_vu(ang), u2f(0x3f8ccccdu)));
        swc1(pt + 8, u2f(0x3f170a3du));
        mat3_transform(V(pt), V(pt), M(m + 0xc0));
        vec3_add(V(pt), V(pt), V(m + 0x10));
        glow_draw(pt, lwu(m + 0x90) & 0xffff0000u, u2f(0x3e087fccu), 0.0f);
    }

    ee_stack_free(0x290);
}

}  // namespace rac1
