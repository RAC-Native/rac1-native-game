// Ported overlay code: the Morph-o-Ray's HUD element (controller slot 4, key 0x7533, requested by its hand update 0x2bc7e8):
// its start 0x236b10, update 0x236d00 and draw 0x236e50, and the counter update 0x236128 its update runs. It is the
// oxygen indicator's frame (hud_o2.cpp) with the bubbles over it: the same table of 100 bubbles at 0x17e660 (x, y and a
// velocity) and the same layout numbers 0x15f750.., without the box and the blinking; the bubbles below the line that the
// counter's value (+0x74 of 10000) sets are drawn.
#include <game/math/vecmath.h>
#include <game/ovl/hud_o2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/suck_cannon.h>
#include <game/ovl/weapon_classes.h>

namespace rac1 {

namespace {
constexpr OvConst<0x0017e660u> BUBBLES{};  // 100 x (x, y, vx, vy)
constexpr OvConst<0x0015f750u> TIER_A{};
constexpr OvConst<0x0015f754u> TIER_B{};
constexpr OvConst<0x0015f758u> TIER_C{};
constexpr OvConst<0x0015f760u> WIDTH{};
constexpr OvConst<0x0015f764u> HEIGHT{};
constexpr OvConst<0x0015f768u> RATIO_X{};
constexpr OvConst<0x0015f76cu> RATIO_Y{};
constexpr OvConst<0x0015f3f8u> FRAME{};  // the frame counter

// CVT.W.S: truncated toward zero, saturating
int32_t trunc_w(float f) {
    const uint32_t u = f2u(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? INT32_MIN : INT32_MAX;
    return static_cast<int32_t>(f);
}

// A counter element's frame without the counting: the shown value (+0x74) takes the value (+0x78, from *(+0xc) clamped to
// 0 .. +8) at once, 3 s up on a change (unless it is fading in); the fades (+0x70 / +0x71) and the element's animation.
// orig: 0x00236128
void hud_counter_snap_update(uint32_t e) {
    const uint32_t fade = e + 0x70;
    if (lwu(e + 0xc) != 0) {
        int32_t v = lw(lwu(e + 0xc));
        if (!(-1 < v)) v = 0;
        sw(e + 0x78, static_cast<uint32_t>(v));
        if (lw(e + 8) < v) sw(e + 0x78, lwu(e + 8));
    }
    if (lw(e + 0x74) != lw(e + 0x78)) {
        sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0xb4)));
        if (!(lw(e + 0x6c) < 0x18)) sw(e + 0x74, lwu(e + 0x78));
    }
    if (lw(e + 0x7c) < frame_scale_i(5)) {
        if (lbu(fade + 1) != 0 || lbu(fade) != 0) sw(e + 0x6c, 1);
        if (lbu(fade + 1) != 0) sb(fade + 1, lbu(fade + 1) - 1u);
        else if (lbu(fade) != 0) sb(fade, lbu(fade) - 1u);
    } else if (static_cast<int32_t>(lbu(fade)) < frame_scale_i(8)) {
        sb(fade, lbu(fade) + 1u);
    } else if (static_cast<int32_t>(lbu(fade + 1)) < frame_scale_i(8)) {
        sb(fade + 1, lbu(fade + 1) + 1u);
    }
    hud_anim_frame(e + 0x40);
}
}  // namespace

// The bubbles' table, as the oxygen indicator's start makes it (the same code).
// orig: 0x00236b10
void morph_hud_start(uint32_t e) { hud_o2_start(e); }

// The counter, then every other bubble (by the frame counter's parity) moves by its velocity and bounces off the frame's
// edges (its tier's bound: the first 10, the next 20, the rest).
// orig: 0x00236d00
void morph_hud_update(uint32_t e) {
    hud_counter_snap_update(e);
    const int32_t frame = lw(FRAME);
    const float a = lwc1(TIER_A), b = lwc1(TIER_B), c = lwc1(TIER_C);
    for (int32_t i = frame - (frame / 2) * 2; i < 100; i += 2) {
        const uint32_t rec = BUBBLES + static_cast<uint32_t>(i) * 16u;
        const float tier = i < 10 ? a : (i < 30 ? b : c);
        const float lo_x = fpu_sub(tier, u2f(0x41880000u));
        const float vx = lwc1(rec + 8);
        const float lo_y = fpu_sub(tier, u2f(0x42e80000u));
        const float x = fpu_add(lwc1(rec), vx);
        const float y = fpu_add(lwc1(rec + 4), lwc1(rec + 0xc));
        const float hi_x = -lo_x;
        const float hi_y = -lo_y;
        swc1(rec, x);
        swc1(rec + 4, y);
        if (x < lo_x) {
            swc1(rec, lo_x);
            swc1(rec + 8, -vx);
        }
        if (hi_x < lwc1(rec)) {
            const float v = lwc1(rec + 8);
            swc1(rec, hi_x);
            swc1(rec + 8, -v);
        }
        if (lwc1(rec + 4) < lo_y) {
            const float v = lwc1(rec + 0xc);
            swc1(rec + 4, lo_y);
            swc1(rec + 0xc, -v);
        }
        if (hi_y < lwc1(rec + 4)) {
            const float v = lwc1(rec + 0xc);
            swc1(rec + 4, hi_y);
            swc1(rec + 0xc, -v);
        }
    }
}

// The frame's sprite at the element's place, then the bubbles of the first tier's size below the line.
// orig: 0x00236e50
void morph_hud_draw(uint32_t e) {
    const uint32_t xy = ee_stack_alloc(0x10);  // the place goes by address
    sw(xy + 4, lwu(e + 0x54));
    sw(xy, lwu(e + 0x50));
    sw(e + 0x58, lwu(WIDTH));
    sw(e + 0x5c, lwu(HEIGHT));
    hud_element_place(e, xy, xy + 4);
    hud_element_offset(e, xy, xy + 4, lw(e + 0x6c), 0);
    const int32_t frame0 = hud_frame(lwu(e), 0);
    suck_camera_23bac0(static_cast<uint32_t>(frame0), static_cast<uint32_t>(lw(xy) + lh(e + 0x48)),
                       static_cast<uint32_t>(lw(xy + 4) + lh(e + 0x4a)), lwu(WIDTH), lwu(HEIGHT), 0x80);

    // the bubbles' origin, scale and the line (the layout ratios of the frame)
    const float k16 = 16.0f;
    const float fH = static_cast<float>(lw(HEIGHT));
    const float g2 = lwc1(RATIO_Y);
    const float fW = static_cast<float>(lw(WIDTH));
    const float g1 = lwc1(RATIO_X);
    const float half = 0.5f;
    const float used = static_cast<float>(10000 - lw(e + 0x74));
    const float px = static_cast<float>(lw(xy) + lh(e + 0x48));
    const float py = static_cast<float>(lw(xy + 4) + lh(e + 0x4a));
    const float oy_half = fpu_mul(fpu_mul(fH, g2), half);
    const float ox_half = fpu_mul(fpu_mul(fW, g1), half);
    const float line_frac = fpu_div(used, 10000.0f);
    const float border_x = fpu_mul(fpu_mul(fpu_sub(1.0f, g1), half), fW);
    const float border_y = fpu_mul(fpu_mul(fpu_sub(1.0f, g2), half), fH);
    Vec4 scale{};
    scale.x = fpu_mul(fpu_div(ox_half, u2f(0x41880000u)), k16);
    scale.y = fpu_mul(fpu_div(-oy_half, u2f(0x42e80000u)), k16);
    scale.z = 1.0f;
    scale.w = 1.0f;
    Vec4 origin{};
    origin.x = fpu_mul(fpu_add(fpu_add(ox_half, px), border_x), k16);
    origin.y = fpu_mul(fpu_add(fpu_add(oy_half, py), border_y), k16);
    float line = fpu_sub(u2f(0x42e80000u), fpu_mul(fpu_add(line_frac, line_frac), u2f(0x42e80000u)));

    const int32_t sprite = hud_frame(lwu(e), 1);
    Vec4 tier_a{};  // (the first tier's offset)
    const float d1 = fpu_mul(lwc1(TIER_A), k16);
    tier_a.y = d1;
    tier_a.x = d1;
    int32_t size = 0;
    for (int32_t i = 0; i < 100; ++i) {
        const uint32_t rec = BUBBLES + static_cast<uint32_t>(i) * 16u;
        if (i == 0) {
            vec3_sub(origin, origin, tier_a);
            const float v = lwc1(TIER_A);
            line = fpu_add(line, v);
            size = trunc_w(fpu_mul(v, 32.0f));
        }
        if (line < lwc1(rec + 4)) continue;
        Vec4 pos{};
        vec3_mul(pos, V(rec), scale);
        vec3_add(pos, pos, origin);
        hud_sprite_xyz(sprite, trunc_w(pos.x), trunc_w(pos.y), size, size, 0x80);
    }
    ee_stack_free(0x10);
}

}  // namespace rac1
