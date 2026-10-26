// Ported overlay code: the quick select (triangle), a HUD element (slot 3, requested by the
// hero's input 0x229b70): a ring of 8 slots (the items 0x141ea0, their icons and names from the
// item table 0x179ac0) chosen with the left stick or the d-pad, the choice equipped when
// triangle is let go. Its pages 0x15fac0 (the current 0x15f72c) of 0x1c-byte slots at
// 0x17dac0 (+0 the icon's animation, +8 / +0xc / +0x10 / +0x14 the d-pad neighbours, +0x18 the
// item); the slot shown +0x74, the stick's hold timer +0x78.
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

#include <vector>

namespace rac1 {

namespace {
constexpr uint32_t PAD = 0x0013c940;
constexpr uint32_t P = 0x0013f350;
// orig: (helper) repeated in 0x00237ba0 / 0x002382a0 — the current page's slot `i`
uint32_t qs_slot(int32_t i) {
    return static_cast<uint32_t>(i) * 0x1c + lwu(lwu(OV(0x0015fac0)) + lwu(OV(0x0015f72c)) * 4);
}
// orig: (helper) repeated in 0x002382a0 — a ratio clamped to 0..1 (the compares keep NaN)
float qs_clamp01(float v) {
    float r = 1.0f;
    if (!(1.0f < v)) {
        r = v;
        if (v < 0.0f) r = 0.0f;
    }
    return r;
}
}  // namespace

// The element with serial `serial` emptied (an empty request on its slot): 1, or 0 for none.
// orig: 0x00235908
int32_t hud_request_clear(int32_t serial) {
    int32_t i = 0;
    for (; i < 0xd; ++i)
        if (lw(OV(0x0017dc50) + static_cast<uint32_t>(i) * 0x90 + 0x64) == serial) break;
    if (!(i < 0xd)) return 0;
    camera_request(static_cast<uint32_t>(i), 0xffff, 0, 0, 0, 0, 0);
    return 1;
}

// An element's place (*px, *py: EE words) moved by its layout flags (+0x60): centred
// vertically unless 1 / 2, horizontally unless 4 (8: right-aligned).
// orig: 0x00235a70
int32_t hud_element_place(uint32_t e, uint32_t px, uint32_t py) {
    const uint32_t f = lwu(e + 0x60);
    const int32_t w = lw(e + 0x58), h = lw(e + 0x5c);
    if (((f ^ 1) & 1) != 0 && (f & 2) == 0) sw(py, static_cast<uint32_t>(lw(py) - (h >> 1)));
    const uint32_t g = lwu(e + 0x60);
    if ((g & 4) == 0) sw(px, static_cast<uint32_t>(lw(px) - ((g & 8) != 0 ? w : w >> 1)));
    return 0;
}

// Text centred on x in the small font (effect texture 1); returns its left edge.
// orig: 0x001fb9a8
int32_t text_centered_small(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    x -= text_width_small(s, n) >> 1;
    text_draw(x, y, colour, s, n, effect_tex0(1), OV(0x001c3150));
    return x;
}

// The quick select's start: 8 slots, page 0 of 0x15f738, its size 0x15f718 x 0x15f71c, none
// chosen (-2), the stick timer 1/2 s; the slots from the quick-select items 0x141ea0.
// orig: 0x00237ae8
void quick_select_start(uint32_t e) {
    sw(OV(0x0015fa90), 8);
    sw(OV(0x0015fac0), OV(0x0015f738));
    sh(e + 0x48, 0);
    sh(e + 0x4a, 0);
    sw(e + 0x58, lwu(OV(0x0015f718)));
    sw(e + 0x74, 0xfffffffeu);
    sw(e + 0x5c, lwu(OV(0x0015f71c)));
    sw(e + 0x78, static_cast<uint32_t>(frame_scale_i(0x1e)));
    sw(e + 0x70, 0);
    sw(OV(0x0015f72c), 0);
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t item = lwu(0x00141ea0 + i * 4);
        sw(OV(0x0017dac0) + i * 0x1c + 0x18, item);
        sw(OV(0x0017dac0) + i * 0x1c, lhu(item * 0x4c + OV(0x00179ac0) + 0x38));
    }
}

// The quick select's frame: kept up; the stick (beyond 0.9) points at a slot of the ring (with
// some stickiness at the edges, 0x15f788) or the d-pad walks the slots' neighbours; the choice
// fades in. While triangle is held it fades in; once let go the chosen item is equipped (its
// use counted, the level's bit in 0x1418ec, the ammo clamped), the element fades out and is
// cleared.
// orig: 0x00237ba0
void quick_select_update(uint32_t e) {
    const uint32_t b = e + 0x70;
    sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0xb4)));
    sw(e + 0x6c, 0x18);
    if (timer_tick_i16(ee_ref<int16_t>(0x00141622)) != 0) sw(PAD + 0x1cc, 2);
    float sx = lwc1(PAD + 0x148), sy = lwc1(PAD + 0x14c);
    const float len = sqrt_f(sx * sx + sy * sy);
    if (!(len == 0.0f)) {
        sx = fpu_div(sx, len);
        sy = fpu_div(sy, len);
    }
    const float ang = atan2_f(sx, sy);
    {
        const int32_t t = lw(e + 0x78);
        if ((t >> 24) == 0) {
            bool reset = (lwu(PAD + 0x1c4) & 0xf000) != 0 || len < 0.5f;
            if (!reset) {
                sw(e + 0x78, static_cast<uint32_t>(t - 1));
                reset = t - 1 == -1;
            }
            if (reset) {
                sw(e + 0x74, 0xffffffffu);
                sw(e + 0x78, 0x010000ff);
            }
        }
    }
    int32_t dir = 0;
    const int32_t old = lw(e + 0x74);
    if (static_cast<int8_t>(lbu(e + 0x7b)) == 1 && (lwu(PAD + 0x1c0) & 0x10) != 0) {
        if (u2f(0x3f666666u) < len) {
            const float PI = u2f(0x40490fdbu), TWO_PI = u2f(0x40c90fdbu), HALF_PI = u2f(0x3fc90fdbu);
            const int32_t n = lw(OV(0x0015fa90));
            const float fn = static_cast<float>(n);
            const float a = ang + PI + PI + HALF_PI + fpu_div(PI, fn);
            const float pos = a * fpu_div(fn, TWO_PI);
            bool pick = true;
            if (old != -1) {
                const float cur = fpu_div(static_cast<float>(old) * TWO_PI, int_to_float(n)) - PI;
                if (!(u2f(0x3f16cbe4u) < angle_dist(cur, angle_sub(ang, HALF_PI)))) pick = false;
            }
            if (pick) {
                int32_t s = ee_rem(float_to_int(pos), lw(OV(0x0015fa90)));
                if (((s ^ 1) & 1) != 0) {
                    const float frac = pos - static_cast<float>(float_to_int(pos));
                    const float edge = lwc1(OV(0x0015f788));
                    if (1.0f - edge < frac) ++s;
                    else if (frac < edge) --s;
                    s = ee_rem(s + lw(OV(0x0015fa90)), lw(OV(0x0015fa90)));
                }
                dir = -1;
                sw(e + 0x74, static_cast<uint32_t>(s));
                const float d = angle_wrap(a - (static_cast<float>(s) * u2f(0x3f490fdbu) + u2f(0x3ec90fdbu)));
                swc1(OV(0x0015f784), d);
                if (0.0f < d) dir = 1;
            }
        } else if (lw(PAD + 0x1d8) == 0) {
            const uint32_t h = lwu(PAD + 0x1c4);
            if ((h & 0xf000) != 0) {
                int32_t s = old;
                if ((h & 0x1000) != 0) s = s == -1 ? lw(OV(0x0017dc20)) : lw(qs_slot(s) + 0x10);
                if ((lwu(PAD + 0x1c4) & 0x4000) != 0) s = s == -1 ? lw(OV(0x0017dc24)) : lw(qs_slot(s) + 0x14);
                if ((lwu(PAD + 0x1c4) & 0x8000) != 0) s = s == -1 ? lw(OV(0x0017dc18)) : lw(qs_slot(s) + 8);
                if ((lwu(PAD + 0x1c4) & 0x2000) != 0) s = s == -1 ? lw(OV(0x0017dc1c)) : lw(qs_slot(s) + 0xc);
                sw(e + 0x74, static_cast<uint32_t>(s));
            }
        }
        // the chosen slot must hold an item (else its neighbour in the stick's direction)
        if (lw(qs_slot(lw(e + 0x74))) == 0 && dir != 0) {
            const int32_t s = ee_rem(lw(e + 0x74) + dir, lw(OV(0x0015fa90)));
            if (lw(qs_slot(s)) != 0) sw(e + 0x74, static_cast<uint32_t>(s));
        }
    }
    if (lw(e + 0x74) == old) {
        if (static_cast<int32_t>(lbu(b + 1)) < lw(OV(0x0015f780))) sb(b + 1, lbu(b + 1) + 1);
    } else {
        sb(b + 1, 0);
    }
    if ((lwu(PAD + 0x1c0) & 0x10) != 0) {
        if (static_cast<int32_t>(lbu(b)) < lw(OV(0x0015f780))) sb(b, lbu(b) + 1);
        if (lbu(b) != 0) return;
    } else {
        const int32_t s = lw(e + 0x74);
        if (s >= 0) {
            const uint32_t item = lwu(qs_slot(s) + 0x18);
            if (item != 0) {
                if (lwu(P + 0x10b8) != item) {
                    const uint32_t c = lhu(0x001418e8);
                    if (!(0xfffe < c)) sh(0x001418e8, c + 1);
                }
                const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
                if (static_cast<int32_t>(lhu(0x001418ea)) < q) sh(0x001418ea, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
                sw(0x001418ec, lwu(0x001418ec) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
                if (item == 0x18) sb(P + 0x1ff5, 1);
                else sw(P + 0x20b8, item);
                const uint32_t row = item * 0x18 + OV(0x001c40b0);
                if (lhu(row + 8) != 0) {
                    const uint32_t a = item * 4 + 0x0013d428;
                    const int32_t max = static_cast<int32_t>(lhu(row + 0xe));
                    if (max < lw(a)) sw(a, static_cast<uint32_t>(max));
                }
            }
        }
        if (lbu(b) != 0) {
            sb(b, lbu(b) - 1);
            if (lbu(b) != 0) return;
        }
    }
    sw(OV(0x0015fa98), lwu(e + 0x78));
    sw(OV(0x0015fa9c), lwu(e + 0x74));
    hud_request_clear(lw(e + 0x64));
    sw(e + 0x6c, 0xfffffffau);
    sw(OV(0x0015fa94), lwu(OV(0x0015f3f8)));
}

// The quick select: its frame (four mirrored quarters of 0xe934), the ring of slots (the chosen
// one ringed by 0xe99c, the others dimmer; an icon's frame 4 for an item marked in 0x13e520),
// the chosen item's ammo and its name (split in two lines at its '-' or first space).
// orig: 0x002382a0
int32_t quick_select_draw(uint32_t e) {
    const float fbox = qs_clamp01(fpu_div(int_to_float(lbu(e + 0x70)), int_to_float(lw(OV(0x0015f780)))));
    const float fin = qs_clamp01(fpu_div(int_to_float(lbu(e + 0x71)), int_to_float(lw(OV(0x0015f780)))));
    const int32_t ph = ee_rem(lw(OV(0x0015f5cc)), lw(OV(0x0015f7b0)));
    float t = fpu_div(int_to_float(ph), int_to_float(lw(OV(0x0015f7b0))));
    t = t * u2f(0x40c90fd0u);
    const float sn = sin_vu(t - u2f(0x40490fd0u));
    const float a128 = fbox * 128.0f;
    const int32_t alpha = float_to_int(a128);
    const float pulse = sn * 0.125f + 0.875f;
    const int32_t ring_a = float_to_int(a128 * pulse);
    const uint32_t pos = ee_stack_alloc(0x10);
    sw(pos, lwu(e + 0x50));
    sw(pos + 4, lwu(e + 0x54));
    hud_element_place(e, pos, pos + 4);
    const int32_t x = lw(pos), y = lw(pos + 4);
    ee_stack_free(0x10);
    const int32_t w = lw(OV(0x0015f718)), h = lw(OV(0x0015f71c));
    const int32_t hw = ee_div(w, 2), hh = ee_div(h, 2);
    const uint64_t a64 = static_cast<uint64_t>(static_cast<int64_t>(alpha));
    hud_sprite(hud_frame(0xe934, 0), x, y, hw, hh, a64);
    hud_sprite(hud_frame(0xe934, 0), x + lw(OV(0x0015f718)), y + lw(OV(0x0015f71c)), -hw, -hh, a64);
    hud_sprite(hud_frame(0xe934, 0), x + lw(OV(0x0015f718)), y, -hw, hh, a64);
    hud_sprite(hud_frame(0xe934, 0), x, y + lw(OV(0x0015f71c)), hw, -hh, a64);
    const float PI = u2f(0x40490fdbu);
    for (int32_t i = 0; i < lw(OV(0x0015fa90)); ++i) {
        const float fi = static_cast<float>(i), fn = static_cast<float>(lw(OV(0x0015fa90)));
        const float a = angle_add(fpu_div((fi + fi) * PI, fn) - PI, u2f(0x3fc90fdbu));
        const int32_t sx = x + lw(OV(0x0015f720)) + float_to_int(lwc1(OV(0x0015f78c)) * cos_vu(a) * lwc1(OV(0x0015f7c4)));
        const int32_t sy = y + lw(OV(0x0015f724)) + float_to_int(lwc1(OV(0x0015f78c)) * sin_vu(a));
        if (lw(e + 0x74) == i && lw(qs_slot(i)) != 0)
            hud_sprite(hud_frame(0xe99c, 0), sx - 0x13, sy - 0x13, 0x26, 0x26, static_cast<uint64_t>(static_cast<int64_t>(ring_a)));
        if (lw(qs_slot(i)) != 0) {
            const int32_t ia = i == lw(e + 0x74) ? alpha : float_to_int(static_cast<float>(alpha) * lwc1(OV(0x0015f7c0)));
            const uint32_t item = lwu(qs_slot(i) + 0x18);
            const int32_t f = hud_frame(lwu(qs_slot(i)), lbu(0x0013e520 + item) != 0 ? 4 : 0);
            hud_sprite_at(e, f, sx, sy, 1, static_cast<uint64_t>(static_cast<int64_t>(ia)));
        }
    }
    const int32_t sel = lw(e + 0x74);
    if (sel < 0 || lw(qs_slot(sel)) == 0) return lw(e + 0x58);
    const float k = fbox * fin;
    const uint64_t shadow = rgba_lerp(lwu(OV(0x0015f798)), lwu(OV(0x0015f79c)), k);
    const uint32_t item = lwu(0x00141ea0 + static_cast<uint32_t>(sel) * 4);
    const uint32_t row = item * 0x18 + OV(0x001c40b0);
    const uint32_t buf = ee_stack_alloc(0x50);
    if (lhu(row + 8) != 0) {
        const int32_t ammo = lw(0x0013d428 + item * 4);
        const std::vector<uint64_t> args = {static_cast<uint64_t>(static_cast<int64_t>(ammo)), static_cast<uint64_t>(lhu(row + 0xe))};
        libc_sprintf(buf, OV(0x0015f7c8), args);
        const uint64_t col = ammo != 0 ? rgba_lerp(lwu(OV(0x0015f7a8)), lwu(OV(0x0015f7ac)), k) : rgba_lerp(lwu(OV(0x0015f7a0)), lwu(OV(0x0015f7a4)), k);
        text_centered_small(x + lw(OV(0x0015f720)) + 1, y + lw(OV(0x0015f724)) + 0xd, shadow, buf, -1);
        text_centered_small(x + lw(OV(0x0015f720)), y + lw(OV(0x0015f724)) + 0xc, col, buf, -1);
    }
    ee_stack_free(0x50);
    const uint64_t col = rgba_lerp(lwu(OV(0x0015f790)), lwu(OV(0x0015f794)), k);
    const uint32_t it = lwu(qs_slot(sel) + 0x18);
    const uint32_t ir = it * 0x4c + OV(0x00179ac0);
    const uint32_t name = sub_00203ad8(lbu(0x0013e520 + it) != 0 ? lh(ir + 0x48) : lh(ir + 0x46));
    if (name == 0 || libc_strlen(name) == 0) return lw(e + 0x58);
    const int32_t n = libc_strlen(name);
    int32_t split = 0;
    uint32_t rest = name;
    // the first line up to a '-' (kept), else up to a space (dropped); the first character is
    // never looked at
    for (int32_t i = 1; i < n; ++i)
        if (lbu(name + static_cast<uint32_t>(i)) == '-') {
            split = i + 1;
            rest = name + static_cast<uint32_t>(i) + 1;
            break;
        }
    if (split == 0)
        for (int32_t i = 1; i < n; ++i)
            if (lbu(name + static_cast<uint32_t>(i)) == ' ') {
                split = i;
                rest = name + static_cast<uint32_t>(i) + 1;
                break;
            }
    int32_t ty = y + lw(OV(0x0015f724));
    ty += lhu(lwu(0x00141ea0 + static_cast<uint32_t>(sel) * 4) * 0x18 + OV(0x001c40b0) + 8) != 0 ? lw(OV(0x0015f7b4)) : (split != 0 ? lw(OV(0x0015f7b8)) : lw(OV(0x0015f7bc)));
    const int32_t tx = x + lw(OV(0x0015f720));
    if (split != 0) {
        text_centered_small(tx + 1, ty + 1, shadow, name, split);
        text_centered_small(x + lw(OV(0x0015f720)), ty, col, name, split);
        ty += 0x10;
        text_centered_small(x + lw(OV(0x0015f720)) + 1, ty + 1, shadow, rest, -1);
        text_centered_small(x + lw(OV(0x0015f720)), ty, col, rest, -1);
    } else {
        text_centered_small(tx + 1, ty + 1, shadow, name, -1);
        text_centered_small(x + lw(OV(0x0015f720)), ty, col, name, -1);
    }
    return lw(e + 0x58);
}

namespace {
// orig: (none) — the draw function as an indirect-call target
void quick_select_draw_fn(uint32_t e) { (void)quick_select_draw(e); }
}  // namespace

// orig: (none) — the quick select's functions for the slots' indirect calls.
void quick_select_fns_register() {
    ovl_register_moby_fn(0x00237ae8, quick_select_start);
    ovl_register_moby_fn(0x00237ba0, quick_select_update);
    ovl_register_moby_fn(0x002382a0, quick_select_draw_fn);
}

}  // namespace rac1
