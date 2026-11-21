// Ported overlay code: the glows (see glows.h).
#include <game/ovl/glows.h>

#include <game/enhance/fixes102.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// The frame as the original lays it out (descriptor at +0, the basis rows d / right / up /
// point at +0xa0..+0xd0, z axis at +0x90), so the vector helpers see the same lanes.
// orig: 0x00263618
void glow_draw(uint32_t point, uint32_t rgba, float size, float toward) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    sd(sp + 0x90, 0);
    sd(sp + 0x98, 0);
    swc1(sp + 0x98, 1.0f);
    swc1(sp + 0x9c, 1.0f);
    const uint64_t tex0 = effect_tex0(0xb);
    sd(sp + 0x88, 0x8000000048ull);
    sd(sp + 0x80, 0x0000ff9000000260ull);
    sd(sp + 0x70, 5);
    sd(sp + 0x78, tex0);
    sw(sp + 0x50, 0);
    sw(sp + 0x54, 0);
    sw(sp + 0x58, 0);
    swc1(sp + 0x5c, 1.0f);
    swc1(sp + 0x60, 1.0f);
    sw(sp + 0x64, 0);
    swc1(sp + 0x68, 1.0f);
    swc1(sp + 0x6c, 1.0f);
    const uint32_t d = sp + 0xa0, right = sp + 0xb0, up = sp + 0xc0, at = sp + 0xd0, push = sp + 0xe0;
    lq_sq(at, point);
    vec3_sub(V(d), V(OV(0x00166dc0)), V(at));
    vec3_set_length(V(d), V(d), 1.0f);
    vec3_cross_ba(V(right), V(d), V(sp + 0x90));
    vec3_set_length(V(right), V(right), 1.0f);
    vec3_cross_ba(V(up), V(right), V(d));
    vec3_scale(V(push), V(d), toward);
    vec3_add(V(at), V(at), V(push));
    for (uint32_t k = 0; k < 4; ++k) sw(sp + 0x40 + 4 * k, rgba);
    for (uint32_t k = 0; k < 4; ++k) {
        vec3_scale(V(sp + 0x10 * k), V(OV(0x001b0470) + 0x10 * k), size);
        mat4_transform(V(sp + 0x10 * k), V(sp + 0x10 * k), M(d));
    }
    quad_draw(sp, 0);
    ee_stack_free(0xf0);
}

namespace {
// orig: (helper) repeated in 0x00207d40 — the glows of the *(0x13f350 + 0x1eb0) points at +0x1dd0
// (colour +0x1e50, size +0x1e70, pull +0x1e90 by point)
void hero_point_glows() {
    constexpr uint32_t P = 0x0013f350;
    if (lh(P + 0x1eb0) <= 0) return;
    int32_t i = 0;
    do {
        const uint32_t k = static_cast<uint32_t>(i);
        ++i;
        glow_draw(P + 0x1dd0 + 0x10 * k, lwu(P + 0x1e50 + 4 * k), lwc1(P + 0x1e70 + 4 * k), lwc1(P + 0x1e90 + 4 * k));
    } while (i < lh(P + 0x1eb0));
}
}  // namespace

// By the game mode 0x15f5c4 (2, 6: the points) and the hero's item mode +0x20a4: 3 three glows
// on +0x1d50.. (colour +0x22e4), 0 a small glow on +0x1d80 (raised 0.015 for it) unless busy,
// then the points, 2 a larger one lowered 0.07, 1 one whose size approaches 0.07 (0.1015 when
// +0x22fc) at the frame rate, its point sinking.
// orig: 0x00207d40
void hero_glows_draw(uint32_t) {
    constexpr uint32_t P = 0x0013f350;
    const int32_t mode = lw(OV(0x0015f5c4));
    if (mode == 2 || mode == 6) {
        hero_point_glows();
        return;
    }
    const uint32_t item = lbu(P + 0x20a4);
    if (item == 3) {
        const uint32_t c = lwu(P + 0x22e4);
        for (uint32_t k = 0; k < 3; ++k) glow_draw(P + 0x1d50 + 0x10 * k, c, u2f(0x3e4ccccdu), u2f(0x3da3d70au));
        return;
    }
    if (item == 0) {
        bool small = false;
        if (sub_0020d498(3) == 3) small = lh(P + 0x22d8) == 0;
        else if (sub_0020d498(3) == 2) small = lh(P + 0x22d8) == 0;
        if (small) {
            const float k = u2f(0x3c75c28fu);
            swc1(P + 0x1d88, fpu_add(lwc1(P + 0x1d88), k));
            glow_draw(P + 0x1d80, 0x280000c0u, u2f(0x3d6978d5u), u2f(0x3da3d70au));
            swc1(P + 0x1d88, fpu_sub(lwc1(P + 0x1d88), k));
        }
        hero_point_glows();
        return;
    }
    if (item == 2) {
        const float k = u2f(0x3d8f5c29u);
        swc1(P + 0x1d88, fpu_sub(lwc1(P + 0x1d88), k));
        glow_draw(P + 0x1d80, 0x300000c0u, 0.25f, u2f(0x3da3d70au));
        swc1(P + 0x1d88, fpu_add(lwc1(P + 0x1d88), k));
        return;
    }
    if (item != 1) return;
    float target = lh(P + 0x22fc) != 0 ? u2f(0x3dcfdf3cu) : u2f(0x3d8f5c29u);
    // (1.02 Fixes, fix 29: the 1.02 build, 2.00 0x0020857c / 0x00208590, makes the glow a seventh smaller: 0.06, 0.087)
    if (fixes102_on(29)) target = lh(P + 0x22fc) != 0 ? u2f(0x3db22d0eu) : u2f(0x3d75c28fu);
    if (lbu(0x0015edb3) != 0) {
        swc1(P + 0x1d88, fpu_sub(lwc1(P + 0x1d88), u2f(0x3c23d70au)));
        target = fpu_mul(target, u2f(0x3fd9999au));
    } else {
        swc1(P + 0x1d88, fpu_sub(lwc1(P + 0x1d88), u2f(0x3ca3d70au)));
    }
    approach_f(0x0014097c, target, fpu_mul(lwc1(0x0015ed60), u2f(0x3b449ba6u)));
    glow_draw(0x001410d0, lwu(P + 0x2300), lwc1(0x0014097c), u2f(0x3da3d70au));
}

// orig: 0x002df3d8
void light_group_glows_draw(uint32_t m) {
    const int32_t level = lw(0x0015ed84);
    const float size = (level == 0 || level == 0x12) ? 1.0f : u2f(0x3f99999au);
    uint32_t o = 0;
    group_first_r(o, lbu(m + 0x21), 0, 0);
    while (o != 0) {
        if (lbu(o + 0x31) != 0 && lh(o + 0xa6) == lh(m + 0xa6)) glow_draw(o + 0x10, lwu(lwu(o + 0x78) + 4), size, 0.5f);
        group_next_r(o, o, 0, 0);
    }
}

// orig: (none)
void glow_fns_register() {
    ovl_register_moby_fn(0x00207d40, hero_glows_draw);
    ovl_register_moby_fn(0x002df3d8, light_group_glows_draw);
}

}  // namespace rac1
