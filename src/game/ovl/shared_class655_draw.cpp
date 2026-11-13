// Class 655's draw: one port of the copies Quartu (L15), Drek's fleet (L17) and Veldin's finale (L18) carry (the same code
// relinked; compared word by word: only relocated words differ). Each level's words (per level below): its record (+0 the
// scroll, +4 its speed, -0x18 / -0x10 / +0x18 / +0x1c the long strips' alphas, +8 / +0xc the two strips' colours, +0x10 the
// facing strip's width, +0x14 / +0x20 / +0x24 widths, +0x48 / +0x4c stretches, +0x50 / +0x54 the two effect textures), the four
// short strips' alphas (halves; on Veldin's finale a word later in the record than on the other two: their own address),
// the points' buffers. The camera is the engine's (OV 0x166dc0). The glows are Quartu's and Drek's fleet's class655_glows,
// Veldin's finale's own build (class655_glows_l18). Header shared_class655_draw.h.
//
// The quads' descriptors (quad_draw): the corners +0, their colours +0x40, their texture coordinates +0x50, the GS
// registers +0x70.
#include <game/ovl/shared_class655_draw.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shared_class655_glows.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>

namespace rac1 {

namespace {
uint32_t record() {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161ae4);
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x00162048);
    return L15_ADDR(0x00161c54);
}
uint32_t short_alphas() {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161b58);
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x001620c0);
    return L15_ADDR(0x00161cc8);
}
uint32_t points_buffer() {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x001d39d0);
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x001d9d00);
    return L15_ADDR(0x001d3750);
}

// the reciprocal of a length, or the length itself (0) when it is 0, as the original scales a 0 vector
float inverse_or_zero(float len) { return len == 0.0f ? len : fpu_div(1.0f, len); }
}  // namespace

// orig: L15:0x002d8878 L17:0x002d6ef0 L18:0x002ebb60
void class655_strip(uint32_t points, uint32_t alpha0, uint32_t alpha1, int32_t count, float width, float stretch) {
    const uint32_t rec = record();
    const uint32_t cam = OV(0x00166dc0);
    const uint32_t sp = ee_stack_alloc(0x2a0);
    const uint32_t q1 = sp, q2 = sp + 0x90;  // (the two quads' descriptors)
    const uint32_t to = sp + 0x140, along = sp + 0x150, side = sp + 0x120, off = sp + 0x130;
    const uint32_t e1 = sp + 0x160, e2 = sp + 0x170, d1 = sp + 0x180, d2 = sp + 0x190, back = sp + 0x1a0, ext = sp + 0x1b0;
    const uint64_t blend = 0x8000000048ull, clamp = 0x0000ff9000000260ull;
    const uint32_t faded1 = lwu(rec + 8) | (alpha0 << 24);
    sd(q1 + 0x78, effect_tex0(lw(rec + 0x50)));
    sd(q1 + 0x88, blend);
    sd(q1 + 0x80, clamp);
    sd(q1 + 0x70, 0);
    for (uint32_t k = 0; k < 4; ++k) sw(q1 + 0x40 + k * 4, faded1);
    sd(q2 + 0x78, effect_tex0(lw(rec + 0x54)));
    sw(q2 + 0x50, 0);
    const uint32_t faded2 = lwu(rec + 0xc) | (alpha1 << 24);
    const float zero = lwc1(q2 + 0x50);
    const float scroll = lwc1(rec);
    const float s1 = fpu_add(scroll, 1.0f), s0 = fpu_add(scroll, zero);
    sd(q2 + 0x88, blend);
    sd(q2 + 0x80, clamp);
    sd(q2 + 0x70, 0);
    // (the facing strip's texture scrolled along it; the other's fixed)
    swc1(q1 + 0x58, s0);
    swc1(q1 + 0x68, s1);
    for (uint32_t k = 0; k < 4; ++k) sw(q2 + 0x40 + k * 4, faded2);
    sw(q2 + 0x54, 0);
    sw(q2 + 0x58, 0);
    swc1(q2 + 0x5c, 1.0f);
    swc1(q2 + 0x60, 1.0f);
    sw(q2 + 0x64, 0);
    swc1(q2 + 0x68, 1.0f);
    swc1(q2 + 0x6c, 1.0f);
    swc1(q1 + 0x50, s0);
    swc1(q1 + 0x54, zero);
    swc1(q1 + 0x5c, 1.0f);
    swc1(q1 + 0x60, s1);
    swc1(q1 + 0x64, zero);
    swc1(q1 + 0x6c, 1.0f);
    // the facing strip: each point's edge across the line and the view (the level's width), a quad from the last edge
    vec3_sub(V(to), V(points), V(cam));
    vec3_sub(V(along), V(points + 0x10), V(points));
    vec3_cross_ba(V(side), V(along), V(to));
    vec3_scale(V(side), V(side), fpu_mul(lwc1(rec + 0x10), inverse_or_zero(vec3_length(V(side)))));
    vec3_add(V(q1), V(points), V(side));
    vec3_sub(V(q1 + 0x10), V(points), V(side));
    const int32_t legs = count - 1;
    for (int32_t i = 1; i < legs; ++i) {
        const uint32_t p = points + static_cast<uint32_t>(i) * 0x10;
        vec3_sub(V(to), V(p), V(cam));
        vec3_sub(V(along), V(p + 0x10), V(p));
        vec3_cross_ba(V(side), V(along), V(to));
        const float len = vec3_length(V(side));
        vec3_scale(V(side), V(side), fpu_mul(lwc1(rec + 0x10), inverse_or_zero(len)));
        vec3_add(V(q1 + 0x20), V(p), V(side));
        vec3_sub(V(q1 + 0x30), V(p), V(side));
        if (i == 1) {
            sw(q1 + 0x40, lwu(rec + 8));
            sw(q1 + 0x44, lwu(rec + 8));
        } else if (i == count - 2) {
            sw(q1 + 0x48, lwu(rec + 8));
            sw(q1 + 0x4c, lwu(rec + 8));
        } else if (i == 2) {
            sw(q1 + 0x44, faded1);
            sw(q1 + 0x40, faded1);
        }
        quad_draw(q1, 0);
        lq_sq(q1, q1 + 0x20);
        lq_sq(q1 + 0x10, q1 + 0x30);
    }
    // the other strip: each leg's ends brought to the same distance from the camera, stretched along the leg, its width
    // across the leg as seen from the camera
    for (int32_t i = 0; i < legs; ++i) {
        const uint32_t pa = points + static_cast<uint32_t>(i) * 0x10, pb = pa + 0x10;
        vec3_sub(V(d1), V(pa), V(cam));
        vec3_sub(V(d2), V(pb), V(cam));
        const float l1 = vec3_length(V(d1)), l2 = vec3_length(V(d2));
        if (l2 < l1) {
            if (l2 == 0.0f) lq_sq(d2, d1);
            else vec3_scale(V(d2), V(d2), fpu_div(l1, l2));
        } else {
            if (l1 == 0.0f) lq_sq(d1, d2);
            else vec3_scale(V(d1), V(d1), fpu_div(l2, l1));
        }
        vec3_add(V(e1), V(cam), V(d1));
        vec3_add(V(e2), V(cam), V(d2));
        vec3_sub(V(along), V(e2), V(e1));
        vec3_scale(V(ext), V(along), stretch);
        vec3_sub(V(e1), V(e1), V(ext));
        vec3_add(V(e2), V(e2), V(ext));
        vec3_sub(V(back), V(cam), V(e1));
        vec3_cross_ba(V(side), V(back), V(along));
        const float len = vec3_length(V(side));
        vec3_scale(V(off), V(side), len == 0.0f ? fpu_mul(width, len) : fpu_mul(width, fpu_div(1.0f, len)));
        vec3_add(V(q2), V(e1), V(off));
        vec3_sub(V(q2 + 0x10), V(e1), V(off));
        vec3_add(V(q2 + 0x20), V(e2), V(off));
        vec3_sub(V(q2 + 0x30), V(e2), V(off));
        if (i == 0) {
            sw(q2 + 0x40, lwu(rec + 0xc));
            sw(q2 + 0x44, lwu(rec + 0xc));
        } else if (i == count - 2) {
            sw(q2 + 0x48, lwu(rec + 0xc));
            sw(q2 + 0x4c, lwu(rec + 0xc));
        } else if (i == 1) {
            sw(q2 + 0x44, faded2);
            sw(q2 + 0x40, faded2);
        }
        quad_draw(q2, 0);
    }
    sub_002a0ba8(0x47, 0x5360b);
    ee_stack_free(0x2a0);
}

// orig: L15:0x002d8710 L17:0x002d6d88 L18:0x002eb9f8
void class655_draw(uint32_t m) {
    const uint32_t rec = record();
    // the scroll moved back by its speed a frame, kept above -8
    const float scroll = fpu_sub(lwc1(rec), fpu_mul(lwc1(rec + 4), lwc1(0x0015ed60)));
    swc1(rec, scroll);
    if (scroll <= -8.0f) swc1(rec, fpu_add(scroll, 8.0f));
    const uint32_t pts = points_buffer(), halves = short_alphas();
    const float stub = 0.2f;  // (the short strips' stretch)
    class655_strip(pts, lwu(rec - 0x18), lwu(rec - 0x10), 0x14, lwc1(rec + 0x14), lwc1(rec + 0x48));
    class655_strip(pts + 0x140, lwu(rec + 0x18), lwu(rec + 0x1c), 0x14, lwc1(rec + 0x24), lwc1(rec + 0x4c));
    for (uint32_t k = 0; k < 4; ++k) {
        static const uint32_t at[] = {0x460, 0x4b0, 0x500, 0x550};
        const uint32_t a = static_cast<uint32_t>(lh(halves + k * 2));
        class655_strip(pts + at[k], a, a, 5, lwc1(rec + 0x24), stub);
    }
    if (on_level(VELDIN_FINALE)) class655_glows_l18(m);
    else class655_glows(m);
}

}  // namespace rac1
