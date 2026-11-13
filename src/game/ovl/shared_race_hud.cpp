// The hoverboard races' HUD: one port of the copies Rilgar and Kalebo III carry (the same code relinked). Its settings,
// strings and the race's trick state are the level's own words, at the same addresses on both levels (RACE_ADDR: Rilgar's or
// Kalebo III's, as hero_hoverboard.cpp's HB_ADDR); the three-trick string sits apart on each level. The rest is the base
// ELF's (bare: the hero's block 0x13f350, the screen 0x13e500, the widescreen flag 0x15ed80, the level 0x15ed84, the race
// flag 0x13d388) or the engine's (OV: the frame counter 0x15f5cc). The text is drawn by Veldin's draw_detector_text
// (0x1fb470, hud_slot7.cpp).
//
// The race's trick state (RACE_ADDR): 0x15fad4 the trick's score (the hero's +0x70c: its turns in 360ths of a flip, for a
// pose its frames), 0x15fad8 the tricks in the combo (the hero's +0x6f0), 0x15fae0..0x15faec the four tricks' flags (the
// hero's +0x6e0..), 0x15faf0..0x15fafc the last four combos, 0x15fb00 the points the landing gives. The meter element's
// bytes: +0x70 a timer, +0x71 the message's frames left, +0x72 its state (0 waiting, 1 a trick in the air, 2 landed,
// 3 landed a combo); +0x74 the meter's fill, +0x8 its full value, +0xc the boost it follows (the hero's +0x864).
//
// The PAL builds changed the meter's draw and the bar (data/regions/SCES-50916*.txt: L5:0x262f50 / 0x266320, L16:0x235660 /
// 0x238a30): the texts (the place, the poses, the flips) come from the level's text table in the player's language. The
// 1.02 build's (the PAL disc 2.00's) is 1.02 Fixes fix 19 (fixes102_race_text, at the three sprintf the generated copies
// hook as well); the PAL disc 1.00's own is the same kind of change (text only), not forked.
#include <game/ovl/shared_race_hud.h>

#include <game/enhance/fixes102.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/globals.h>
#include <game/ovl/hud_slot7_tr.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
template <uint32_t A>
uint32_t race_addr() {
    return on_level(KALEBO) ? L16_ADDR(A) : L5_ADDR(A);
}
#define RACE_ADDR(a) race_addr<(a)>()

constexpr uint32_t P = g::PLAYER;

// a colour word as the original passes it: a word loaded (sign-extended) or built by `lui` (sign-extended)
uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
// the text drawn with its shadow (0x80000000: built by ori + dsll, not sign-extended) one pixel off
constexpr uint64_t SHADOW = 0x80000000ull;

// the race's trick state cleared: the tricks' flags, the score and the count
void tricks_clear() {
    for (uint32_t k = 0; k < 4; ++k) sw(RACE_ADDR(0x0015faec) - k * 4, 0);
}

// the screen's height (0x13e504) less a y counted from its bottom, 0xa more without the widescreen (0x15ed80)
int32_t from_bottom(int32_t y) { return lw(0x0013e504) - (y + (lw(0x0015ed80) != 0 ? 0xa : 0x12)); }
}  // namespace

// States: 0 waits for a trick (the hero's +0x30e at 6 or more and a score of 1 or more: the score kept, the tricks cleared,
// state 1 with the message's frames); 1 follows the trick in the air (the timer to 10, the message counting down, the
// count and the flags copied, the best score kept and the message restarted with it) until the hero is back on the board
// (+0x300) or in the states 0x6d..0x6f: then the hero's trick state is cleared and a combo (the state 0x6b) shifts the
// history (more than two tricks) and goes to 3 for 120 frames, anything else to 2 for 60; 2 and 3 count the timer down, and
// at 110 frames of a combo its points go to the hero's +0x8a8 and five times to his boost +0x864 (at most 17 seconds' worth);
// at 0 everything is cleared (the timer wraps to 255).
// orig: L5:0x00262b58 L16:0x00235268
void race_meter_update(uint32_t e) {
    const uint32_t sp = ee_stack_alloc(0x50);
    const uint32_t fill = sp;
    swc1(fill, int_to_float(lw(e + 0x74)));
    approach_f(fill, int_to_float(lw(lwu(e + 0xc))), 6.0f);
    sw(e + 0x74, static_cast<uint32_t>(float_to_int(lwc1(fill))));
    const uint32_t timer = e + 0x70, msg = e + 0x71, state = e + 0x72;
    switch (lbu(state)) {
    case 0: {
        if (lh(P + 0x30e) < 6) break;
        const uint32_t score = lwu(P + 0x70c);
        if (!(1.0f <= int_to_float(static_cast<int32_t>(score)))) break;
        sw(RACE_ADDR(0x0015fad4), score);
        sw(RACE_ADDR(0x0015fad8), 0);
        tricks_clear();
        sb(state, 1);
        sb(timer, 0);
        sb(msg, lbu(RACE_ADDR(0x0015f87c)));
        break;
    }
    case 1: {
        if (lbu(timer) < 10) sb(timer, lbu(timer) + 1);
        if (lbu(msg) != 0) sb(msg, lbu(msg) - 1);
        sw(RACE_ADDR(0x0015fad8), lwu(P + 0x6f0));
        for (uint32_t k = 0; k < 4; ++k) sw(RACE_ADDR(0x0015fae0) + k * 4, lwu(P + 0x6e0 + k * 4));
        if (lw(RACE_ADDR(0x0015fad4)) < lw(P + 0x70c)) {
            sw(RACE_ADDR(0x0015fad4), lwu(P + 0x70c));
            sb(msg, lbu(RACE_ADDR(0x0015f87c)));
        }
        if (lw(P + 0x300) == 0 && static_cast<uint32_t>(lw(g::HERO_STATE) - 0x6d) >= 3u) break;  // (still in the air)
        sw(RACE_ADDR(0x0015fb00), 0);
        sw(P + 0x70c, 0);
        sw(P + 0x6f0, 0);
        for (uint32_t k = 0; k < 4; ++k) sw(P + 0x6ec - k * 4, 0);
        for (uint32_t k = 0; k < 3; ++k) sw(P + 0x708 - k * 4, 0);
        if (lw(g::HERO_STATE) == 0x6b) {
            if (!(lw(RACE_ADDR(0x0015fad8)) < 3)) {  // (the history shifted: the newest slot open)
                const uint32_t h2 = lwu(RACE_ADDR(0x0015faf8)), h1 = lwu(RACE_ADDR(0x0015faf4)), h0 = lwu(RACE_ADDR(0x0015faf0));
                sw(RACE_ADDR(0x0015fafc), h2);
                sw(RACE_ADDR(0x0015faf8), h1);
                sw(RACE_ADDR(0x0015faf4), h0);
                sw(RACE_ADDR(0x0015faf0), 0xffffffffu);
            }
            sb(state, 3);
            sb(msg, lbu(RACE_ADDR(0x0015f87c)));
            sb(timer, static_cast<uint32_t>(frame_scale_i(0x78)));
        } else {
            sb(state, 2);
            sb(msg, lbu(RACE_ADDR(0x0015f87c)));
            sb(timer, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
        break;
    }
    case 2:
    case 3: {
        if (lbu(timer) == 0) {
            sw(RACE_ADDR(0x0015fb00), 0);
            sw(RACE_ADDR(0x0015fad4), 0);
            sw(RACE_ADDR(0x0015fad8), 0);
            tricks_clear();
            sb(state, 0);
            sb(timer, 0);
        } else if (lbu(state) == 3 && static_cast<int32_t>(lbu(timer)) == frame_scale_i(0x6e)) {
            const int32_t points = lw(RACE_ADDR(0x0015fb00));
            sw(P + 0x8a8, lwu(P + 0x8a8) + static_cast<uint32_t>(points));
            sw(P + 0x864, lwu(P + 0x864) + static_cast<uint32_t>(points) * 5u);
            const int32_t most = float_to_int(fpu_mul(int_to_float(frame_scale_i(0x11)), 60.0f));
            if (most < lw(P + 0x864)) sw(P + 0x864, static_cast<uint32_t>(float_to_int(fpu_mul(int_to_float(frame_scale_i(0x11)), 60.0f))));
        }
        sb(timer, lbu(timer) - 1);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x50);
}

// While the hero is on the board (his +0x208c 0x16): in the states 1 / 2 the trick's score (in degrees, x 360) and the
// poses' count ("x%d Poses"), faded in by the timer, the colour from the message's frames (state 1) or fixed (2); in the
// state 3 the combo: one of five kinds (by the tricks' flags) and three tiers (by the score) gives its points from the
// level's table, shared by the times the same combo is in the history, and its name; a landing without a combo of 1 or of
// more tricks: ten or twenty points a flip and the tricks' names. Then the element is placed (0x100 x 0x40) and the meter
// drawn (its fill over its full value), and while the hero's +0x8ae is past 120 frames the boost's message pulses.
// orig: L5:0x00262f50 L16:0x00235660
void race_meter_draw(uint32_t e) {
    const uint32_t sp = ee_stack_alloc(0x140);
    const uint32_t text = sp, names = sp + 0x80, px = sp + 0x90, py = sp + 0x94;
    float alpha = fpu_div(int_to_float(lbu(e + 0x70)), 10.0f);
    if (1.0f < alpha) alpha = 1.0f;
    if (lw(g::HERO_SUBSTATE) != 0x16) {
        ee_stack_free(0x140);
        return;
    }
    const uint32_t st = lbu(e + 0x72);
    if (st != 0) {
        libc_sprintf(text, RACE_ADDR(0x0015f860), {sx(lwu(RACE_ADDR(0x0015fad4)) * 0x168u)});
        const float left = int_to_float(lbu(e + 0x71));
        const float frames = int_to_float(frame_scale_i(lw(RACE_ADDR(0x0015f87c))));
        uint32_t colour = st == 1 || st == 3 ? rgba_lerp(lwu(RACE_ADDR(0x0015f894)), lwu(RACE_ADDR(0x0015f898)), fpu_div(left, frames))
                                             : lwu(RACE_ADDR(0x0015f8a0));
        const uint32_t a = static_cast<uint32_t>(float_to_int(fpu_mul(alpha, 128.0f))) << 24;
        colour += a;
        if (st - 1u < 2u) {
            const int32_t x0 = lw(RACE_ADDR(0x0015f8b4)), y0 = lw(RACE_ADDR(0x0015f8b8));
            text_centered(x0 + 1, y0 + 1, sx(a), text, -1);
            text_centered(x0, y0, sx(colour), text, -1);
            const int32_t poses = lw(RACE_ADDR(0x0015fad8));
            libc_sprintf(text, RACE_ADDR(0x0015f8d0), {sx(static_cast<uint32_t>(poses))});
            fixes102_race_text(text, 0, 0, poses);  // enhancement: 1.02 Fixes, fix 19 (the poses)
            const int32_t x1 = lw(RACE_ADDR(0x0015f8bc)), y1 = lw(RACE_ADDR(0x0015f8c0));
            text_centered(x1 + 1, y1 + 1, sx(a), text, -1);
            text_centered(x1, y1, sx(colour), text, -1);
        } else if (st == 3) {
            const int32_t tricks = lw(RACE_ADDR(0x0015fad8));
            int32_t kind = -1;  // (the combo's kind by the four tricks' flags)
            if (tricks == 3) {
                if (lw(RACE_ADDR(0x0015faec)) == 0) kind = 0;
                else if (lw(RACE_ADDR(0x0015fae8)) == 0) kind = 1;
                else kind = lw(RACE_ADDR(0x0015fae4)) != 0 ? 3 : 2;
            } else if (tricks == 4) {
                kind = 4;
            }
            const int32_t x = lw(RACE_ADDR(0x0015f8c4)), y = lw(RACE_ADDR(0x0015f8c8));
            if (kind >= 0) {
                const int32_t score = lw(RACE_ADDR(0x0015fad4));
                const int32_t tier = score < 4 ? (score < 2 ? 0 : 1) : 2;
                const int32_t combo = kind + tier * 5;
                sw(RACE_ADDR(0x0015faf0), static_cast<uint32_t>(combo));
                sw(RACE_ADDR(0x0015fb00), lwu(RACE_ADDR(0x0017e918) + static_cast<uint32_t>(combo) * 4));
                int32_t times = 1;  // (this combo and the history's repeats of it)
                for (uint32_t k = 0; k < 3; ++k)
                    if (lw(RACE_ADDR(0x0015faf4) + k * 4) == combo) ++times;
                const int32_t points = ee_div(lw(RACE_ADDR(0x0015fb00)), times);
                sw(RACE_ADDR(0x0015fb00), static_cast<uint32_t>(points));
                libc_sprintf(text, RACE_ADDR(0x0015f860), {sx(static_cast<uint32_t>(points))});
                hud_message_box(x, y - 0x18, sx(colour), text);
                libc_sprintf(text, RACE_ADDR(0x0015f8e0), {sx(sub_00203ad8(TXT(0x5092 + combo)))});
                hud_message_box(lw(RACE_ADDR(0x0015f8c4)), lw(RACE_ADDR(0x0015f8c8)), sx(colour), text);
            } else if (tricks > 0) {
                // the tricks whose flag is set (their indices 0..3), at most the count
                int32_t found = 0;
                for (int32_t i = 0; i < 4 && found < tricks; ++i) {
                    if (lw(RACE_ADDR(0x0015fae0) + static_cast<uint32_t>(i) * 4) == 0) continue;
                    sw(names + static_cast<uint32_t>(found) * 4, static_cast<uint32_t>(i));
                    ++found;
                }
                const int32_t score = lw(RACE_ADDR(0x0015fad4));
                const int32_t points = static_cast<int32_t>(static_cast<uint32_t>(score) * (tricks == 1 ? 10u : 20u));
                sw(RACE_ADDR(0x0015fb00), static_cast<uint32_t>(points));
                libc_sprintf(text, RACE_ADDR(0x0015f860), {sx(static_cast<uint32_t>(points))});
                hud_message_box(lw(RACE_ADDR(0x0015f8c4)), lw(RACE_ADDR(0x0015f8c8)) - 0x18, sx(colour), text);
                if (tricks == 1) {
                    const uint32_t name = sub_00203ad8(TXT(0x517e + lw(names)));
                    const int32_t flips = lw(RACE_ADDR(0x0015fad4));
                    libc_sprintf(text, RACE_ADDR(0x0015f8e8), {sx(name), sx(static_cast<uint32_t>(flips))});
                    fixes102_race_text(text, 1, name, flips);  // enhancement: 1.02 Fixes, fix 19 (the flips)
                } else {
                    const uint32_t first = sub_00203ad8(TXT(0x517e + lw(names)));
                    const uint32_t second = sub_00203ad8(TXT(0x517e + lw(names + 4)));
                    const uint32_t two = on_level(KALEBO) ? L16_ADDR(0x001e6810) : L5_ADDR(0x00216c50);  // ("%s + %s + %d ...")
                    libc_sprintf(text, two, {sx(first), sx(second), sx(lwu(RACE_ADDR(0x0015fad4)))});
                }
                hud_message_box(lw(RACE_ADDR(0x0015f8c4)), lw(RACE_ADDR(0x0015f8c8)), sx(colour), text);
            }
        }
    }
    sw(py, lwu(e + 0x54));
    sw(px, lwu(e + 0x50));
    sw(e + 0x58, 0x100);
    sw(e + 0x5c, 0x40);
    hud_element_place(e, px, py);
    const int32_t width = ee_div(static_cast<int32_t>(lwu(e + 0x74) * 0xddu), lw(e + 8)) + 0x1b;
    hud_sprite_texels(hud_frame(0x7558, 1), lw(px), lw(py), width, 0x40, 0x80);
    hud_sprite(hud_frame(0x7558, 0), lw(px), lw(py), 0x100, 0x40, 0x80);
    if (!(frame_scale_i(0x78) < static_cast<int32_t>(lbu(P + 0x8ae)))) {
        ee_stack_free(0x140);
        return;
    }
    // the boost's message, pulsing once a second
    const int32_t second = frame_scale_i(0x3c);
    const float phase = fpu_div(int_to_float(ee_rem(lw(OV(0x0015f5cc)), second)), int_to_float(second));
    const float wave = sin_vu(fpu_sub(fpu_mul(phase, u2f(0x40c90fd0u)), u2f(0x40490fd0u)));  // (2 pi, pi)
    const uint32_t colour = rgba_lerp(lwu(RACE_ADDR(0x0015f8ac)), lwu(RACE_ADDR(0x0015f8b0)), fpu_add(fpu_mul(wave, 0.5f), 0.5f));
    libc_sprintf(text, sub_00203ad8(TXT(0x531a)), {});
    hud_message_box(lw(RACE_ADDR(0x0015f8a4)) + 1, lw(RACE_ADDR(0x0015f8a8)) + 1, sx(0x80000000u), text);
    hud_message_box(lw(RACE_ADDR(0x0015f8a4)), lw(RACE_ADDR(0x0015f8a8)), colour, text);
    ee_stack_free(0x140);
}

// Shown while the element's +0x6c is positive: a box, then "Lap: n/3" from the left (the lap the hero's +0x89a, at most 3)
// and the place ("1st".."nth", the hero's +0x8c0) right-aligned before the race's title, each with its shadow; on
// Kalebo III (level 16) also the race's bolt (the hero's +0x8ce, carried or not) in its box (at the race's place, or the
// element's when no race runs).
// orig: L5:0x00266320 L16:0x00238a30
void race_bar_draw(uint32_t e) {
    if (!(lw(e + 0x6c) > 0)) return;
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t text = sp, count = sp + 0x40;
    const int32_t box_y = from_bottom(lw(RACE_ADDR(0x0015f988)));
    const int32_t y = from_bottom(lw(RACE_ADDR(0x0015f9a4)));
    const int32_t shadow_y = y + 1;
    hud_box(lw(RACE_ADDR(0x0015f984)), box_y, lw(RACE_ADDR(0x0015f98c)), lw(RACE_ADDR(0x0015f990)), sx(lwu(RACE_ADDR(0x0015f994))));
    int32_t x = lw(RACE_ADDR(0x0015f9a0));
    libc_sprintf(text, RACE_ADDR(0x0015f9d8), {sx(sub_00203ad8(TXT(0x523f)))});
    draw_detector_text(x - 1, shadow_y, SHADOW, text, -1);
    draw_detector_text(x, y, sx(lwu(RACE_ADDR(0x0015f998))), text, -1);
    x += text_width_small(text, -1);
    {
        int32_t lap = lh(P + 0x89a) + 1;
        if (!(lap < 4)) lap = 3;
        libc_sprintf(text, RACE_ADDR(0x0015f9e0), {sx(static_cast<uint32_t>(lap))});
    }
    draw_detector_text(x - 1, y + 2, SHADOW, text, -1);
    draw_detector_text(x, y, sx(lwu(RACE_ADDR(0x0015f99c))), text, -1);
    // the place, right-aligned before the title
    const int32_t place = lw(P + 0x8c0);
    x = lw(RACE_ADDR(0x0015f9b0));
    switch (place) {
    case 1: libc_sprintf(text, RACE_ADDR(0x0015f9e8), {1}); break;
    case 2: libc_sprintf(text, RACE_ADDR(0x0015f9f0), {2}); break;
    case 3: libc_sprintf(text, RACE_ADDR(0x0015f9f8), {3}); break;
    default: libc_sprintf(text, RACE_ADDR(0x0015fa00), {sx(static_cast<uint32_t>(place))}); break;
    }
    fixes102_race_text(text, 2, 0, lw(P + 0x8c0));  // enhancement: 1.02 Fixes, fix 19 (the place)
    x -= text_width_small(text, -1);
    draw_detector_text(x + 1, shadow_y, SHADOW, text, -1);
    draw_detector_text(x, y, sx(lwu(RACE_ADDR(0x0015f99c))), text, -1);
    libc_sprintf(text, RACE_ADDR(0x0015fa08), {sx(sub_00203ad8(TXT(0x5240)))});
    x -= text_width_small(text, -1);
    draw_detector_text(x + 1, shadow_y, SHADOW, text, -1);
    draw_detector_text(x, y, sx(lwu(RACE_ADDR(0x0015f998))), text, -1);
    if (lw(0x0015ed84) != 0x10) {
        ee_stack_free(0xf0);
        return;
    }
    // Kalebo III: the bolt
    libc_sprintf(count, RACE_ADDR(0x0015f860), {lbu(P + 0x8ce)});
    int32_t bx = 0, by = 0;
    if (lbu(0x0013d388) != 0) {
        bx = lw(RACE_ADDR(0x0015f9c0));
        by = lw(RACE_ADDR(0x0015f9c4));
    } else {
        bx = lw(e + 0x50);
        by = lw(0x0015ed80) != 0 ? 0xa : 0x12;
    }
    hud_box(bx - 0x18, by, lw(RACE_ADDR(0x0015f9c8)), 0x20, 0x60);
    hud_sprite(hud_frame(0x7558, 2), bx, by, 0x20, 0x20, 0x80);
    draw_detector_text(bx + 0x29, by + 8, SHADOW, count, -1);
    draw_detector_text(bx + 0x28, by + 7, lbu(P + 0x8ce) != 0 ? 0x80e08060ull : 0x80202080ull, count, -1);
    ee_stack_free(0xf0);
}

}  // namespace rac1
