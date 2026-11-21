// Kalebo III (level 16): three draw callbacks of the level's own classes, each quads through quad_draw (its descriptor
// and matrix in an EE frame of the original's size, at the original's offsets). 0x2c9cd0, class 471's (the conveyor,
// kalebo_conveyor.cpp): its belt, two layers of quads along its box's x (pvars +0x60), a glow whose colour pulses (two seconds a
// cycle) and the belt whose texture scrolls (pvars +0x70 by +0x6c), both fading out at the belt's ends.
// 0x2cef60, queued by class 541's group spin (kalebo_fighter.cpp): a glow facing the camera on each fighter of the
// group, its colour by the fighter's spin (pvars +0x11c). 0x2cf7a8, class 552's (the barrier, kalebo_barrier.cpp): once
// a frame per pair, the beam between each barrier of the group and its partner (pvars +0), four quads stacked up.
// Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250
// (cb2c9cd0, cb2cef60, cb2cf7a8, the P16.6 checks).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

#include <game/ovl/variants_l16.h>

#include <cstdint>

namespace rac1 {

namespace {
using kalebo::Frame;
constexpr uint64_t QUAD_TEX1 = 0xff9000000260ull;  // (the descriptors' +0x80 word, the level's constant)
}  // namespace

// orig: L16:0x002c9cd0
void kal_cb_2c9cd0_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float len = fpu_mul(vec3_length(V(lvd::box(lvd::BOXES, lw(pv + 0x60)))), u2f(0x40001062u));  // (its box's x, x 2.001)
    const int32_t segs = float_to_int(len);
    const float scroll = fpu_add(lwc1(pv + 0x70), lwc1(pv + 0x6c));
    const float step = fpu_div(2.0f, len);  // (a quad's length in the box's frame)
    swc1(pv + 0x70, scroll);
    if (1.0f < scroll) swc1(pv + 0x70, fpu_sub(scroll, 1.0f));
    else if (scroll < 0.0f) swc1(pv + 0x70, fpu_add(scroll, 1.0f));
    const int32_t period = frame_scale_i(0x78);
    const int32_t phase = ee_rem(lw(L16_ADDR(0x0015f5cc)), period);
    const float turn = fpu_mul(fpu_div(int_to_float(phase), int_to_float(period)), u2f(0x40c90fd0u));  // (x 6.28318)
    const float pulse = fpu_add(fpu_mul(sin_vu(fpu_sub(turn, u2f(0x40490fd0u))), 0.5f), 0.5f);
    const uint32_t lit = rgba_lerp(lwu(L16_ADDR(0x001619a8)), lwu(L16_ADDR(0x001619ac)), pulse);
    const uint32_t plain = lwu(L16_ADDR(0x001619b0));

    const Frame f(0x1c0);
    const uint32_t glow = f.sp, belt = f.sp + 0x90;  // (two quad_draw descriptors, 0x90 bytes each)
    sd(glow + 0x78, effect_tex0(lw(L16_ADDR(0x00161990))));
    sd(glow + 0x80, QUAD_TEX1);
    sd(glow + 0x88, kalebo::quad_blend(L16_ADDR(0x00161998)));
    sd(glow + 0x70, 0);
    sd(belt + 0x78, effect_tex0(lw(L16_ADDR(0x00161994))));
    sd(belt + 0x80, QUAD_TEX1);
    sd(belt + 0x88, kalebo::quad_blend(L16_ADDR(0x00161998)));
    sd(belt + 0x70, 0);
    const uint32_t st_glow = L16_ADDR(0x001d2ff0), st_belt = L16_ADDR(0x001d3010), corners = L16_ADDR(0x001d3030);
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(glow + 0x50 + k * 8, lwc1(st_glow + k * 8));
        swc1(glow + 0x54 + k * 8, lwc1(st_glow + k * 8 + 4));
        swc1(belt + 0x50 + k * 8, fpu_sub(lwc1(st_belt + k * 8), lwc1(pv + 0x70)));
        swc1(belt + 0x54 + k * 8, lwc1(st_belt + k * 8 + 4));
        sw(glow + 0x40 + k * 4, lit);
        sw(belt + 0x40 + k * 4, plain);
        lq_sq(glow + k * 0x10, corners + k * 0x10);
        lq_sq(belt + k * 0x10, corners + k * 0x10);
        if (k < 2) {  // (the back corners one quad back)
            swc1(glow + k * 0x10, fpu_sub(lwc1(glow + k * 0x10), step));
            swc1(belt + k * 0x10, fpu_sub(lwc1(belt + k * 0x10), step));
        }
    }
    for (int32_t s = 0; s < segs + 2; ++s) {
        if (s == 0) {  // (the first quad fades in: its back corners clear)
            for (uint32_t o : {0x40u, 0x44u}) { sw(glow + o, 0); sw(belt + o, 0); }
        } else if (s == segs + 1) {  // (the last fades out: its front corners clear)
            for (uint32_t o : {0x48u, 0x4cu}) { sw(glow + o, 0); sw(belt + o, 0); }
        } else if (s == 1) {
            for (uint32_t o : {0x40u, 0x44u}) { sw(glow + o, lit); sw(belt + o, plain); }
        }
        quad_draw(glow, lvd::box(lvd::BOXES, lw(pv + 0x60)));
        quad_draw(belt, lvd::box(lvd::BOXES, lw(pv + 0x60)));
        for (uint32_t k = 0; k < 4; ++k) {
            swc1(glow + k * 0x10, fpu_add(lwc1(glow + k * 0x10), step));
            swc1(belt + k * 0x10, fpu_add(lwc1(belt + k * 0x10), step));
        }
    }
}

// orig: L16:0x002cef60
void kal_cb_2cef60_l16(uint32_t m) {
    const Frame f(0x160);
    const uint32_t q = f.sp, mat = f.sp + 0x90;  // (quad_draw's descriptor, 0x90 bytes, and its matrix, 0x40)
    sd(q + 0x78, effect_tex0(0xb));
    sd(q + 0x88, 0x8000000048ull);
    sd(q + 0x80, QUAD_TEX1);
    sd(q + 0x70, 5);
    static const uint32_t kSt[8] = {0, 0, 0, 0x3f800000u, 0x3f800000u, 0, 0x3f800000u, 0x3f800000u};  // (s, t): (0,0) (0,1) (1,0) (1,1)
    for (uint32_t k = 0; k < 8; ++k) sw(q + 0x50 + k * 4, kSt[k]);
    for (uint32_t k = 0; k < 4; ++k) vec3_scale(V(q + k * 0x10), V(L16_ADDR(0x001d3130) + k * 0x10), 0.2f);
    for (uint32_t e = lvd::group(static_cast<int32_t>(lbu(m + 0x21)));; e += 2) {
        const uint32_t mob = kalebo::moby_at(lhu(e));
        const float spin = lwc1(lwu(mob + 0x78) + 0x11c);
        const uint32_t colour = rgba_lerp(lwu(L16_ADDR(0x001619e0)), lwu(L16_ADDR(0x001619e4)), fpu_mul(fpu_add(sin_vu(spin), 1.0f), 0.5f));
        for (uint32_t k = 0; k < 4; ++k) sw(q + 0x40 + k * 4, colour);
        if (mob != 0 && lh(mob + 0xa6) == 0x21d && lbu(mob + 0x20) != 0xfe && lbu(mob + 0x20) != 0xfd) {
            const uint32_t place = mat + 0x30;
            lq_sq(place, mob + 0x10);
            swc1(place + 0xc, 1.0f);
            swc1(place + 8, fpu_add(lwc1(place + 8), u2f(0x3e4ccccdu)));  // (0.2 above it)
            vec3_sub(V(mat), V(L16_ADDR(0x001671c0)), V(place));  // (toward the camera)
            vec3_set_length(V(mat), V(mat), 1.0f);
            vec3_cross_ba(V(mat + 0x10), V(mat), V(0x0013f5e0));
            vec3_set_length(V(mat + 0x10), V(mat + 0x10), -1.0f);
            vec3_cross_ba(V(mat + 0x20), V(mat + 0x10), V(mat));
            quad_draw(q, mat);
        }
        if (lh(e) < 0) break;  // (the group's last)
    }
}

// The beam between barrier `a` and its partner `b`: four quads of the level's texture 0x161a00 in the frame whose x runs
// from `a` to `b` (z up), the first 0x1619f8 above `a`, each next one 0x1619fc higher.
// orig: L16:0x002cf850
void kal_barrier_beam_l16(uint32_t a, uint32_t b) {
    const Frame f(0x120);
    const uint32_t q = f.sp, mat = f.sp + 0x90;  // (quad_draw's descriptor, 0x90 bytes, and its matrix, 0x40)
    vec3_sub(V(mat), V(b + 0x10), V(a + 0x10));
    vec_clear(V(mat + 0x20));
    swc1(mat + 0x28, 1.0f);
    vec3_cross_ba(V(mat + 0x10), V(mat), V(mat + 0x20));
    lq_sq(mat + 0x30, a + 0x10);
    swc1(mat + 0x3c, 1.0f);
    swc1(mat + 0x38, fpu_add(lwc1(mat + 0x38), lwc1(L16_ADDR(0x001619f8))));
    sd(q + 0x78, effect_tex0(lw(L16_ADDR(0x00161a00))));
    sd(q + 0x80, QUAD_TEX1);
    sd(q + 0x88, kalebo::quad_blend(L16_ADDR(0x001619e8)));
    sd(q + 0x70, 0);
    const uint32_t colour = lwu(L16_ADDR(0x00161a04));
    const uint32_t corners = L16_ADDR(0x001d3170), st = L16_ADDR(0x001d31b0);
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(q + 0x50 + k * 8, lwc1(st + k * 8));
        swc1(q + 0x54 + k * 8, lwc1(st + k * 8 + 4));
        sw(q + 0x40 + k * 4, colour);
        lq_sq(q + k * 0x10, corners + k * 0x10);
    }
    for (int32_t i = 0; i < 4; ++i) {
        quad_draw(q, mat);
        swc1(mat + 0x38, fpu_add(lwc1(mat + 0x38), lwc1(L16_ADDR(0x001619fc))));
    }
}

// orig: L16:0x002cf7a8
void kal_cb_2cf7a8_l16(uint32_t m) {
    uint32_t e = lvd::group(static_cast<int32_t>(lbu(m + 0x21)));
    if (e == 0) return;
    for (;; e += 2) {
        const uint32_t mob = kalebo::moby_at(lhu(e));
        if (lh(mob + 0xa6) == 0x228) {  // (a barrier, class 552)
            const uint32_t pv = lwu(mob + 0x78), frame = lwu(L16_ADDR(0x0015f5cc));
            const uint32_t partner = lwu(pv);
            if (lwu(pv + 4) != frame && partner != 0) {  // (not drawn this frame yet, from either end)
                sw(pv + 4, frame);
                sw(lwu(partner + 0x78) + 4, frame);
                kal_barrier_beam_l16(mob, partner);
            }
        }
        if (lh(e) < 0) break;
    }
}

}  // namespace rac1
