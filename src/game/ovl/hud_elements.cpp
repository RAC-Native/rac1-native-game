// Ported overlay code: the HUD's elements (the slots 0x17dc50, see hud.cpp): their start (+0x10),
// update (+0x14) and draw (+0x18) functions. An element: +8 / +0xc its arguments (the maximum,
// the counted value's address), +0x40 its animation state (hud_anim_frame), +0x50 its x, +0x58
// / +0x5c its size, +0x70 / +0x71 the fade of its box / its contents, +0x74 / +0x78 the value
// shown / to show, +0x7c the time it stays up, +0x80 its animation counters.
#include <game/lib/libc.h>
#include <game/ovl/hud_o2.h>
#include <game/ovl/hud_slot7_tr.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

#include <vector>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;  // the hero
// a ratio clamped to 0..1 (the original's compares: NaN is kept)
// orig: (helper) repeated in 0x00239810 / 0x00239df8
float clamp01(float v) {
    float r = 1.0f;
    if (!(1.0f < v)) {
        r = v;
        if (v < 0.0f) r = 0.0f;
    }
    return r;
}
}  // namespace

// The element's size from its maximum (+8: its digits) and its layout flags (+0x60): the shown
// and target values (+0x74 / +0x78) from the counted value (+0xc), or 99999 without one.
// orig: 0x00235c80
void hud_element_size(uint32_t e) {
    const uint32_t c = lwu(e + 0xc);
    if (c == 0 || (c & 3) != 0) {
        sw(e + 0x74, 99999);
        sw(e + 0x78, 99999);
    } else {
        sw(e + 0x78, lwu(c));
        if (lw(e + 8) < lw(c)) sw(e + 0x78, lwu(e + 8));
        sw(e + 0x74, lwu(e + 0x78));
    }
    int32_t digits = 0;
    for (int32_t v = lw(e + 8); !(v < 10); v = ee_div(v, 10)) ++digits;
    const uint32_t flags = lwu(e + 0x60);
    const int32_t w = lw(e + 0x5c);
    if ((flags & 3) == 0 && (flags & 0xc) != 0) {
        sw(e + 0x5c, static_cast<uint32_t>((digits + 1) * 0xc + w));
        if (lw(e + 0x58) < 0xe) sw(e + 0x58, 0xe);
        return;
    }
    if (w < 0xc) sw(e + 0x5c, 0xc);
    sw(e + 0x58, static_cast<uint32_t>(lw(e + 0x58) + (digits + 1) * 0xe));
}

// The frame of an animation state `a` (+0 the animation, +2 its mode: 0 still, 1 loop, 2 back
// and forth, 3 back and forth then a random pause; +0xc its start): kept in +4.
// orig: 0x0023aaa8
int32_t hud_anim_frame(uint32_t a) {
    const uint32_t an = lwu(OV(0x0017e4f4)) + static_cast<uint32_t>(lh(a)) * 8;
    int32_t f = 0;
    if (lhu(an) != 0xffff) {
        const uint32_t mode = lbu(a + 2);
        const int32_t first = static_cast<int32_t>(lhu(an + 4));
        const int32_t n = static_cast<int32_t>(lhu(an + 2));
        const int32_t speed = static_cast<int32_t>(lbu(an + 7));
        const int32_t now = lw(OV(0x0015f5cc));
        if (mode == 1) {
            f = first + ee_rem(ee_div(now - lw(a + 0xc), speed), n);
        } else if (mode == 0) {
            f = first;
        } else if (mode == 2) {
            const int32_t r = ee_rem(ee_div(now - lw(a + 0xc), speed), n * 2 - 2);
            f = (r < n ? r : n * 2 - (r + 2)) + first;
        } else if (mode == 3) {
            const int32_t start = lw(a + 0xc);
            if (!(start < now)) {
                f = lw(a + 4);
            } else {
                const int32_t q = ee_div(now - start, speed);
                if (q < n * 2 - 2) {
                    f = (q < n ? q : n * 2 - (q + 2)) + first;
                } else {
                    f = first;
                    const int32_t k = frame_scale_i(10);
                    const int32_t r = rand_mod(frame_scale_i(0x1e));
                    sw(a + 0xc, static_cast<uint32_t>(static_cast<int32_t>(lhu(an + 2)) * 2 + k + r));
                }
            }
        } else {
            f = 0;
        }
    }
    sw(a + 4, static_cast<uint32_t>(f));
    return f;
}

// The health element's start: up 3 s (+1/2), its box 0x40 x 0x40 at x 0x20, the pulse counters
// 0x15fab0.
// orig: 0x002395b0
void hud_health_start(uint32_t e) {
    const int32_t t = frame_scale_i(0xb4);
    sh(e + 0x4a, 0);
    sw(e + 0x7c, static_cast<uint32_t>(t + 0x1e));
    sw(e + 0x5c, 0x40);
    sh(e + 0x48, 0x20);
    sw(e + 0x58, 0x40);
    hud_element_size(e);
    sw(e + 0x80, OV(0x0015fab0));
    sh(OV(0x0015fab6), 1);
    sh(OV(0x0015fab0), 0);
    sh(OV(0x0015fab2), 0);
    sh(OV(0x0015fab4), 0);
}

// The health element's frame: the value (clamped to 0..max) and the pulse (+0x80: phase, level
// +2 moving by +4 at a step +6 that turns at the ends); 2 s up on a change; the fades.
// orig: 0x00239630
void hud_health_update(uint32_t e) {
    const uint32_t b = e + 0x70;
    if (lwu(e + 0xc) != 0) {
        int32_t v = lw(lwu(e + 0xc));
        if (!(-1 < v)) v = 0;
        sw(e + 0x78, static_cast<uint32_t>(v));
        const uint32_t s = lwu(e + 0x80);
        const int32_t ph = lh(s) + 1;
        const uint32_t step = lhu(s + 4);
        const uint32_t sum = step + lhu(s + 6);
        sh(s + 2, lhu(s + 2) + step);
        sh(s + 4, sum);
        const int32_t sp = static_cast<int16_t>(sum);
        sh(s, static_cast<uint32_t>(ee_rem(ph, 0x3c)));
        if (!(sp < 0x3d)) sh(s + 4, 0x3c);
        else if (sp < -0x3c) sh(s + 4, static_cast<uint32_t>(-0x3c));
        if (!(lh(s + 2) < 0x800) && 0 < lh(s + 4)) sh(s + 6, 0xffff);
        if (!(lh(s + 2) < 0x1000)) sh(s + 2, 0xfff);
        if (lh(s + 2) < 0x7ff && lh(s + 4) < 0) sh(s + 6, 1);
        if (lh(s + 2) < 0) sh(s + 2, 0);
        if (lw(e + 8) < lw(e + 0x78)) sw(e + 0x78, lwu(e + 8));
    }
    const int32_t shown = lw(e + 0x74);
    if (shown != lw(e + 0x78) || shown == 1) {
        sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0x78)));
        sw(e + 0x74, lwu(e + 0x78));
    }
    if (lw(e + 0x7c) < frame_scale_i(5)) {
        sw(e + 0x6c, 1);
        if (lbu(b + 1) != 0) sb(b + 1, lbu(b + 1) - 1);
        else if (lbu(b) != 0) sb(b, lbu(b) - 1);
        else sw(e + 0x6c, 0xfffffffau);
    } else {
        const int32_t m = lw(OV(0x0015f7f0));
        if (static_cast<int32_t>(lbu(b)) < m) sb(b, lbu(b) + 1);
        else if (static_cast<int32_t>(lbu(b + 1)) < m) sb(b + 1, lbu(b + 1) + 1);
    }
    hud_anim_frame(e + 0x40);
}

// The health element: the box (two rows of 8 / 5 / the other counts' layouts 0x17e478 /
// 0x17e460 / 0x15f7f8) widening with the fade, the health balls (animation 0x7536, their
// phase), the full ones with their shine (0x1f) pulsing; none while the hero is in mode 2 or
// state 0x32.
// orig: 0x00239810
int32_t hud_health_draw(uint32_t e) {
    if (lbu(P + 0x20a4) == 2 || lw(P + 0x2084) == 0x32) return lw(e + 0x58);
    const int32_t x = lw(e + 0x50);
    const int32_t y = lw(0x0015ed80) == 0 ? lw(OV(0x0015f808)) + 0x12 : lw(OV(0x0015f808)) + 10;
    const int32_t count = lw(e + 0x78);
    if (lbu(e + 0x70) == 0) return lw(e + 0x58);
    const float fbox = clamp01(fpu_div(int_to_float(lbu(e + 0x70)), int_to_float(lw(OV(0x0015f7f0)))));
    const float fin = clamp01(fpu_div(int_to_float(lbu(e + 0x71)), int_to_float(lw(OV(0x0015f7f0)))));
    const int32_t alpha = float_to_int(fin * 128.0f);
    const int32_t boxa = float_to_int(static_cast<float>(lw(OV(0x0015f820))) * fbox);
    const uint32_t ctr = lwu(e + 0x80);
    int32_t ph = static_cast<int16_t>(lhu(ctr)) >> 1;
    if (count == 0) ph = 0x1e;
    int32_t max = lw(0x0015eda0);
    if (lbu(P + 0x20a4) == 1) max = lw(P + 0x22ac);
    uint32_t layout;
    int32_t half, half2;
    if (max == 8) {
        const float w = fbox * 48.0f;
        layout = OV(0x0017e478);
        half = float_to_int(w);
        half2 = float_to_int(w);
    } else if (max == 5) {
        layout = OV(0x0017e460);
        half = float_to_int(fbox * 32.0f);
        half2 = float_to_int(fbox * 16.0f);
    } else {
        half2 = 0;
        layout = OV(0x0015f7f8);
        half = float_to_int(fbox * 48.0f);
    }
    int32_t by = y + lw(OV(0x0015f810));
    const int32_t end = hud_frame(0x7580, 1), mid = hud_frame(0x7580, 0);
    const uint64_t ba = static_cast<uint64_t>(static_cast<int64_t>(boxa));
    hud_sprite_flip(end, x + half, by, lw(OV(0x0015f81c)), lw(OV(0x0015f81c)), ba);
    hud_sprite(mid, x - half, by, half << 1, lw(OV(0x0015f81c)), ba);
    hud_sprite(end, x - half - lw(OV(0x0015f81c)), by, lw(OV(0x0015f81c)), lw(OV(0x0015f81c)), ba);
    if (half2 != 0) {
        by += lw(OV(0x0015f80c));
        hud_sprite_flip(end, x + half2, by, lw(OV(0x0015f81c)), lw(OV(0x0015f81c)), ba);
        hud_sprite(mid, x - half2, by, half2 << 1, lw(OV(0x0015f81c)), ba);
        hud_sprite(end, x - half2 - lw(OV(0x0015f81c)), by, lw(OV(0x0015f81c)), lw(OV(0x0015f81c)), ba);
    }
    const uint64_t a = static_cast<uint64_t>(static_cast<int64_t>(alpha));
    for (int32_t i = 0; i < max;) {
        const int32_t f = hud_frame(0x7536, ph);
        const int32_t n = i + 1;
        const uint32_t p = static_cast<uint32_t>(i) * 4 + layout;
        hud_sprite_at(e, f, x + lh(p), y + lh(p + 2), 1, a);
        if (ph != 0x1e) {
            hud_sprite_at(e, hud_frame(0x7536, 0x1e), x + lh(p), y + lh(p + 2), 1, a);
            const int32_t shine = hud_frame(0x7536, 0x1f);
            const int32_t sy = y + lh(p + 2) - 0x11, sx = x + lh(p) + 0xe;
            const int32_t lvl = static_cast<int16_t>(lhu(ctr + 2)) >> 4;
            const int32_t sa = float_to_int(static_cast<float>(lvl) * fin);
            hud_sprite(shine, sx, sy, 0x22, 0x22, static_cast<uint64_t>(static_cast<int64_t>(sa)));
            ph = count == n ? 0x1e : ee_rem(ph + 6, 0x1e);
        }
        i = n;
    }
    return lw(e + 0x58);
}

// A counter element's start (bolts, ammo): up 2 s (+1/2), its box 0x20 x 0x20.
// orig: 0x0023a760
void hud_counter_start(uint32_t e) {
    sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0x78) + 0x1e));
    sw(e + 0x5c, 0x20);
    sw(e + 0x58, 0x20);
    hud_element_size(e);
}

// The bolts element's start: the counter's, its animation counter 0x15fab8.
// orig: 0x00239cc8
void hud_bolts_start(uint32_t e) {
    hud_counter_start(e);
    sw(e + 0x80, OV(0x0015fab8));
    sh(OV(0x0015fab8), 0);
}

// The bolts element's frame: the bolt's turn; 1.5 s up on a change of the bolts; the fades.
// orig: 0x00239d00
void hud_bolts_update(uint32_t e) {
    const uint32_t c = lwu(e + 0x80);
    sh(c, static_cast<uint32_t>(ee_rem(lh(c) + 1, 0x3c)));
    const uint32_t bolts = lwu(0x0015ed98);
    if (lwu(e + 0x74) != bolts) {
        sw(e + 0x74, bolts);
        sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0x5a)));
    }
    if (lw(e + 0x7c) < frame_scale_i(5)) {
        sw(e + 0x6c, 1);
        if (lbu(e + 0x71) != 0) sb(e + 0x71, lbu(e + 0x71) - 1);
        else if (lbu(e + 0x70) != 0) sb(e + 0x70, lbu(e + 0x70) - 1);
        else sw(e + 0x6c, 0xfffffffau);
    } else {
        if (static_cast<int32_t>(lbu(e + 0x70)) < lw(OV(0x0015f824))) sb(e + 0x70, lbu(e + 0x70) + 1);
        else if (static_cast<int32_t>(lbu(e + 0x71)) < lw(OV(0x0015f828))) sb(e + 0x71, lbu(e + 0x71) + 1);
    }
}

// The bolts element: its box (as wide as the number), the turning bolt (animation 0x754f), the
// number with its shadow, and the separators every 3 digits (' or . by the language 0x15ed88).
// orig: 0x00239df8
int32_t hud_bolts_draw(uint32_t e) {
    const uint32_t ctr = lwu(e + 0x80);
    if (lw(P + 0x2084) == 0x32 || lbu(e + 0x70) == 0) return lw(e + 0x58);
    const int32_t y = lw(0x0015ed80) != 0 ? 10 : 0x12;
    const int32_t x = lw(e + 0x50);
    const float fbox = clamp01(fpu_div(int_to_float(lbu(e + 0x70)), int_to_float(lw(OV(0x0015f824)))));
    const float fin = clamp01(fpu_div(int_to_float(lbu(e + 0x71)), int_to_float(lw(OV(0x0015f828)))));
    int32_t digits = 1;
    const int32_t a0 = float_to_int(fbox * 128.0f);
    const int32_t boxa = float_to_int(static_cast<float>(a0) * u2f(0x3f333333u));
    for (int32_t v = lw(0x0015ed98); !(v < 10); v = ee_div(v, 10)) ++digits;
    int32_t sep = 0;
    const int32_t w = float_to_int(static_cast<float>(lw(OV(0x0015f84c)) * digits) * fbox);
    const int32_t l = x - 0x1c - w;
    const uint64_t ba = static_cast<uint64_t>(static_cast<int64_t>(boxa));
    hud_sprite_flip(hud_frame(0x7580, 1), x - 0xc, y, 0x20, 0x20, ba);
    hud_sprite(hud_frame(0x7580, 0), l, y, w + 0x10, 0x20, ba);
    hud_sprite(hud_frame(0x7580, 1), l - 0x20, y, 0x20, 0x20, ba);
    hud_sprite_at(e, hud_frame(0x754f, static_cast<int16_t>(lhu(ctr)) >> 1), x - 0x20, y, 0, 0x80);
    const uint32_t col = rgba_lerp(lwu(OV(0x0015f82c)), lwu(OV(0x0015f830)), fin);
    const uint32_t shadow = rgba_lerp(0, 0x80000000u, fin);
    const uint64_t c64 = col, s64 = shadow;  // (zero-extended: ppacb leaves the upper bytes 0)
    const uint32_t buf = ee_stack_alloc(0x20);
    const std::vector<uint64_t> value(1, static_cast<uint64_t>(static_cast<int64_t>(lw(0x0015ed98))));
    libc_sprintf(buf, OV(0x0015f850), value);
    text_right(x + lw(OV(0x0015f834)) + 1, y + lw(OV(0x0015f838)) + 1, s64, buf, -1);
    text_right(x + lw(OV(0x0015f834)), y + lw(OV(0x0015f838)), c64, buf, -1);
    const int32_t lang = lw(0x0015ed88);
    if (lang != 3) sep = lang != 5 ? 1 : 0;
    const std::vector<uint64_t> none(1, 0);
    libc_sprintf(buf, sep != 0 ? OV(0x0015f858u) : OV(0x0015f860u), none);
    const int32_t dy = sep != 0 ? lw(OV(0x0015f840)) : lw(OV(0x0015f848));
    const int32_t dx = sep != 0 ? lw(OV(0x0015f83c)) : lw(OV(0x0015f844));
    for (int32_t k = 3; k < digits; k += 3) {
        const int32_t sx = x + dx, sy = y + dy;
        text_right(sx - lw(OV(0x0015f84c)) * k + 2, sy + 2, s64, buf, -1);
        text_right(sx - lw(OV(0x0015f84c)) * k, sy, c64, buf, -1);
    }
    ee_stack_free(0x20);
    return lw(e + 0x58);
}

// A plain counter element's start: up 3 s (+1/2), at its place.
// orig: 0x00235d80
void hud_plain_start(uint32_t e) {
    const int32_t t = frame_scale_i(0xb4);
    sh(e + 0x48, 0);
    sh(e + 0x4a, 0);
    sw(e + 0x7c, static_cast<uint32_t>(t + 0x1e));
    hud_element_size(e);
}

// A plain counter element's frame: the shown value (+0x74) counts toward the value (+0x78) in
// steps growing with the distance (the remainder kept in +0x73), 3 s up on a change; the fades.
// orig: 0x00235ea0
void hud_plain_update(uint32_t e) {
    const uint32_t b = e + 0x70;
    if (lwu(e + 0xc) != 0) {
        int32_t v = lw(lwu(e + 0xc));
        if (!(-1 < v)) v = 0;
        sw(e + 0x78, static_cast<uint32_t>(v));
        if (lw(e + 8) < v) sw(e + 0x78, lwu(e + 8));
    }
    if (lw(e + 0x74) != lw(e + 0x78)) {
        sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0xb4)));
        if (!(lw(e + 0x6c) < 0x18)) {
            int32_t d = lw(e + 0x74) - lw(e + 0x78);
            if (d < 0) d = -d;
            if (d != 0) {
                int32_t step = float_to_int(sqrt_f(fpu_div(int_to_float(d), 25.0f)) * 5.0f);
                if (step < ee_div(d * frame_scale_i(2), frame_scale_i(10))) step = ee_div(d * frame_scale_i(2), frame_scale_i(10));
                if (!(step < 0x7a)) step = 0x79;
                else if (!(0 < step)) step = 1;
                const uint32_t acc = (lbu(b + 3) + static_cast<uint32_t>(step)) & 0xff;
                sb(b + 3, acc);
                if (frame_scale_i(2) < static_cast<int32_t>(acc)) {
                    const int32_t n = ee_div(static_cast<int32_t>(lbu(b + 3)), frame_scale_i(2));
                    const int32_t shown = lw(e + 0x74);
                    sw(e + 0x74, static_cast<uint32_t>(lw(e + 0x78) < shown ? shown - n : shown + n));
                    sb(b + 3, lbu(b + 3) - static_cast<uint32_t>(n * frame_scale_i(2)));
                }
            }
        }
    }
    if (lw(e + 0x7c) < frame_scale_i(5)) {
        if (lbu(b + 1) != 0 || lbu(b) != 0) sw(e + 0x6c, 1);
        if (lbu(b + 1) != 0) sb(b + 1, lbu(b + 1) - 1);
        else if (lbu(b) != 0) sb(b, lbu(b) - 1);
    } else if (static_cast<int32_t>(lbu(b)) < frame_scale_i(8)) {
        sb(b, lbu(b) + 1);
    } else if (static_cast<int32_t>(lbu(b + 1)) < frame_scale_i(8)) {
        sb(b + 1, lbu(b + 1) + 1);
    }
    hud_anim_frame(e + 0x40);
}

// The weapon's ammo element's frame: the ammo (clamped to 0..max), kept up while shown; the
// fades.
// orig: 0x0023cdb8
void hud_ammo_update(uint32_t e) {
    const int32_t v = lw(lwu(e + 0xc)), max = lw(e + 8);
    sw(e + 0x74, static_cast<uint32_t>(v));
    if (max < v) sw(e + 0x74, static_cast<uint32_t>(max));
    else if (v < 0) sw(e + 0x74, 0);
    if (lw(e + 0x7c) < frame_scale_i(5)) {
        sw(e + 0x6c, 1);
        if (lbu(e + 0x71) != 0) sb(e + 0x71, lbu(e + 0x71) - 1);
        else if (lbu(e + 0x70) != 0) sb(e + 0x70, lbu(e + 0x70) - 1);
        else sw(e + 0x6c, 0xfffffffau);
    } else {
        sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(5)));
        if (static_cast<int32_t>(lbu(e + 0x70)) < lw(OV(0x0015f91c))) sb(e + 0x70, lbu(e + 0x70) + 1);
        else if (static_cast<int32_t>(lbu(e + 0x71)) < lw(OV(0x0015f920))) sb(e + 0x71, lbu(e + 0x71) + 1);
    }
}

// The weapon's ammo element: its box (wider from 100 rounds), the weapon's icon (frame 3 of
// the element's animation, kept in +0x44) and "ammo/max" (0x15f7c8), in its colours or those
// of no ammo.
// orig: 0x0023a7a8
int32_t hud_ammo_draw(uint32_t e) {
    const int32_t y = lw(0x0015ed80) != 0 ? 10 : 0x12;
    const int32_t x = lw(e + 0x50);
    if (lbu(e + 0x70) == 0) return lw(e + 0x58);
    const float fbox = clamp01(fpu_div(int_to_float(lbu(e + 0x70)), int_to_float(lw(OV(0x0015f91c)))));
    const float fin = clamp01(fpu_div(int_to_float(lbu(e + 0x71)), int_to_float(lw(OV(0x0015f920)))));
    const int32_t a = float_to_int(fbox * 128.0f);
    const int32_t boxa = float_to_int(static_cast<float>(a) * u2f(0x3f333333u));
    const int32_t wide = lw(e + 8) < 100 ? lw(OV(0x0015f924)) : lw(OV(0x0015f928));
    const int32_t w = x + float_to_int(static_cast<float>(wide) * fbox);
    const uint64_t ba = static_cast<uint64_t>(static_cast<int64_t>(boxa));
    hud_sprite(hud_frame(0x7580, 1), x - 0x1c, y, 0x20, 0x20, ba);
    hud_sprite(hud_frame(0x7580, 0), x + 4, y, w, 0x20, ba);
    hud_sprite_flip(hud_frame(0x7580, 1), x + 4 + w, y, 0x20, 0x20, ba);
    const int32_t icon = hud_frame(lwu(e), 3);
    sw(e + 0x44, static_cast<uint32_t>(icon));
    hud_sprite_at(e, icon, x, y, 0, static_cast<uint64_t>(static_cast<int64_t>(a)));
    const int32_t ammo = lw(e + 0x74);
    const uint32_t buf = ee_stack_alloc(0x40);
    const std::vector<uint64_t> args = {static_cast<uint64_t>(static_cast<int64_t>(ammo)), static_cast<uint64_t>(static_cast<int64_t>(lw(e + 8)))};
    libc_sprintf(buf, OV(0x0015f7c8), args);
    const uint32_t col = ammo != 0 ? rgba_lerp(lwu(OV(0x0015f92c)), lwu(OV(0x0015f930)), fin) : rgba_lerp(lwu(OV(0x0015f934)), lwu(OV(0x0015f938)), fin);
    const uint32_t shadow = rgba_lerp(0, 0x80000000u, fin);
    text_right(x + lw(OV(0x0015f93c)) + wide + 1, y + lw(OV(0x0015f940)) + 1, shadow, buf, -1);
    text_right(x + lw(OV(0x0015f93c)) + wide, y + lw(OV(0x0015f940)), col, buf, -1);
    ee_stack_free(0x40);
    return lw(e + 0x58);
}

// The element's offset while it slides in or out (+0x7c: out), step `k` +- `d` of its 24 (the
// curves 0x17e400 in / 0x17e3a0 out), toward its side (+0x60: 1 up, 2 down, 4 left, 8 right)
// by its size (+0x58 / +0x5c) and a margin; added to the place (EE addresses px / py).
// orig: 0x00235ad8
void hud_element_offset(uint32_t e, uint32_t px, uint32_t py, int32_t k, int32_t d) {
    int32_t dx = 0, dy = 0;
    k = lw(e + 0x7c) != 0 ? k - d : k + d;
    if (!(-1 < k)) k = 0;
    if (!(k < 0x18)) k = 0x17;
    const uint32_t curve = lw(e + 0x7c) != 0 ? OV(0x0017e3a0u) : OV(0x0017e400u);
    const float f = lwc1(curve + static_cast<uint32_t>(k) * 4);
    const uint32_t side = lwu(e + 0x60);
    if ((side & 1) != 0) dy = -float_to_int(f * (int_to_float(lw(e + 0x5c)) + 52.0f) + 0.5f);
    else if ((side & 2) != 0) dy = float_to_int(f * (int_to_float(lw(e + 0x5c)) + 52.0f) + 0.5f);
    else if ((side & 4) != 0) dx = -float_to_int(f * (int_to_float(lw(e + 0x58)) + 20.0f) + 0.5f);
    else if ((side & 8) != 0) dx = float_to_int(f * (int_to_float(lw(e + 0x58)) + 20.0f) + 0.5f);
    sw(px, lwu(px) + static_cast<uint32_t>(dx));
    sw(py, lwu(py) + static_cast<uint32_t>(dy));
}

// The element of hero mode 2 (P+0x20a4): a 256 x 64 frame (0x7558) with its bar (frame 1, the
// value +0x74 of +8 over 221 pixels) and a 32 x 32 mark shown while P+0x1636; its width.
// orig: 0x0023a640
int32_t hud_mode2_draw(uint32_t e) {
    const uint32_t xy = ee_stack_alloc(0x10);  // the place goes by address
    sw(xy, lwu(e + 0x50));
    sw(xy + 4, lwu(e + 0x54));
    sw(e + 0x58, 0x100);
    sw(e + 0x5c, 0x40);
    hud_element_place(e, xy, xy + 4);
    hud_element_offset(e, xy, xy + 4, lw(e + 0x6c), 0);
    const int32_t bar = lw(e + 0x74) * 0xdd / lw(e + 8) + 0x1b;
    hud_sprite_texels(hud_frame(0x7558, 1), lw(xy), lw(xy + 4), bar, 0x40, 0x80);
    hud_sprite(hud_frame(0x7558, 0), lw(xy), lw(xy + 4), 0x100, 0x40, 0x80);
    const uint64_t a = lh(0x00140986) != 0 ? 0x80u : 0u;
    hud_sprite(hud_frame(0x7558, 2), lw(xy), lw(xy + 4), 0x20, 0x20, a);
    ee_stack_free(0x10);
    return lw(e + 0x58);
}

namespace {
// orig: (none) — the draw functions as indirect-call targets (their result unused there)
void hud_health_draw_fn(uint32_t e) { (void)hud_health_draw(e); }
// orig: (none)
void hud_bolts_draw_fn(uint32_t e) { (void)hud_bolts_draw(e); }
// orig: (none)
void hud_ammo_draw_fn(uint32_t e) { (void)hud_ammo_draw(e); }
// orig: (none)
void hud_mode2_draw_fn(uint32_t e) { (void)hud_mode2_draw(e); }
}  // namespace

// orig: (none) — the element functions for the slots' indirect calls.
void hud_element_fns_register() {
    ovl_register_moby_fn(0x002395b0, hud_health_start);
    ovl_register_moby_fn(0x00239630, hud_health_update);
    ovl_register_moby_fn(0x00239810, hud_health_draw_fn);
    ovl_register_moby_fn(0x0023a760, hud_counter_start);
    ovl_register_moby_fn(0x00239cc8, hud_bolts_start);
    ovl_register_moby_fn(0x00239d00, hud_bolts_update);
    ovl_register_moby_fn(0x00239df8, hud_bolts_draw_fn);
    ovl_register_moby_fn(0x00235d80, hud_plain_start);
    ovl_register_moby_fn(0x00235ea0, hud_plain_update);
    ovl_register_moby_fn(0x0023cdb8, hud_ammo_update);
    ovl_register_moby_fn(0x0023a7a8, hud_ammo_draw_fn);
    ovl_register_moby_fn(0x0023a640, hud_mode2_draw_fn);
    ovl_register_moby_fn(0x00235dc0, hud_slot7_start_tr);  // (slot 7, the Metal Detector's)
    ovl_register_moby_fn(0x0023a218, hud_slot7_update_tr);
    // (its draw, the Metal Detector's display: every level has the same code; Batalia, Drek's fleet and the Veldin finale keep the
    // ports of their own copies, registered for the level, which the dispatch tries first)
    ovl_register_moby_fn(0x0023a2d0, hud_slot7_draw_tr);
    hud_o2_fns_register();
}

}  // namespace rac1
