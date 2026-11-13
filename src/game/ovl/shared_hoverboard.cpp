// The hoverboard races' shared code, one port of the copies Rilgar and Kalebo III carry: class 439 (the board), the
// boost pads' test and the race's HUD element (its start, update, draw and reset callbacks). The copies are the same code relinked (compared with the relocated fields masked and normalised); the
// pads' test reads its controller's pvars at offsets that differ between the two levels' controllers (parameters here).
// Level-own words: the race flag 0x15f6a8 (at the same address on both levels, not one of Veldin's) and the board's dust
// (below); the rest is the engine's (OV: the frame counter 0x15f5cc) or the base ELF's (bare: the hero 0x13f350.., the
// help records 0x141968.., the frame time 0x15ed6c). Header shared_hoverboard.h.
//
// The board's pvars: its last position +0 and its home +0x10, the race's flag +0x50 (it does not go home). The HUD's
// layout and its formats are the engine's (OV: 0x15f778.., 0x15fa04..0x15fa60, the laps' words 0x15fae0..0x15faec).
#include <game/ovl/shared_hoverboard.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350;

// the board's dust: its colour +0, +4, and +0x1c a count (an integer word)
uint32_t dust_words() { return on_level(RILGAR) ? L5_ADDR(0x0016190c) : L16_ADDR(0x00161968); }
// the race's flag (set while a race is on)
uint32_t race_flag() { return on_level(RILGAR) ? L5_ADDR(0x0015f6a8) : L16_ADDR(0x0015f6a8); }

uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }

// the HUD's text (Veldin's 0x1fb470, each level's copy: the HUD font)
void hud_text(int32_t x, int32_t y, uint64_t colour, uint32_t str) { text_draw(x, y, colour, str, -1, effect_tex0(3), OV(0x001c3890)); }

// a line of the race's HUD in `buf`: its shadow (one right and one down, black) at `x`, `y`, and the text in `colour`
void hud_line(int32_t x, int32_t y, uint64_t colour, uint32_t buf) {
    hud_text(x + 1, y + 1, 0x80000000ull, buf);  // (built with a doubleword shift: the upper word 0)
    hud_text(x, y, colour, buf);
}

}  // namespace

// State 0 (waiting): the hero within 5.5, after the first frames (the frame counter 4 or more) and not in the modes 0x15
// / 0x16, steps on it: the hero's board (+0x86c) is it, the hero's state 0x6b (mode 1), its home kept (unless in a race,
// +0x50), it hides (state 1). State 1 (ridden): the help message 0x138f (record 0x5f) when the race flags say so; once
// the hero's mode is not 0x16 it is back (state 0, shown dim, at its home unless in a race). In every state: its
// movement since the last frame (+0); under 2 a frame (and +0xbc not 2) it throws dust from one of its two joints
// (alternate frames): from a still board a puff (particle 4), from a moving one a trail (particle 4, a bluish one) and a
// streak (particle 0x15). +0xbc cleared.
// orig: L5:0x002f87a8 L16:0x002c7788
void class439_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t move = sp, joint = sp + 0x10, vel = sp + 0x20, side = sp + 0x30, slow = sp + 0x40;
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        if (vec3_distance(V(0x0013f3d0), V(pos)) < 5.5f && !(lw(OV(0x0015f5cc)) < 4)) {
            const int32_t mode = lw(0x001413dc);
            if (mode != 0x15 && mode != 0x16) {
                sw(HERO + 0x86c, m);
                hero_set_state(0x6b, 1);
                if (lw(pv + 0x50) == 0) lq_sq(pv + 0x10, pos);
                sb(m + 0x20, 1);
                sb(m + 0x30, 0xff);
            }
        }
    } else if (st == 1) {
        if (lw(pv + 0x50) == 0 && lbu(0x0013d388) != 0 && lhu(0x00141938) == 0 && lw(race_flag()) != 0 &&
            frame_scale_i(0x4b0) < lw(HERO + 0x19c) && lhu(0x00141c60) == 0)
            sub_00203908(0x138f, 0x5f);
        if (lw(HERO + 0x208c) != 0x16) {
            sb(m + 0x30, 0x10);
            if (lw(pv + 0x50) == 0) lq_sq(pos, pv + 0x10);
            sb(m + 0x20, 0);
        }
    }

    vec3_sub(V(move), V(pos), V(pv));
    lq_sq(pv, pos);
    const float moved = vec3_length(V(move));
    if (lbu(m + 0xbc) != 2 && moved < 2.0f) {
        moby_chain_point(m, lwu(OV(0x0015f5cc)) & 1u, joint);
        const float a = rand_angle();
        const float k = fpu_mul(lwc1(0x0015ed6c), 0.25f);
        vec3_set_length(V(vel), V(m + 0xe0), fpu_mul(k, cos_vu(a)));
        vec3_set_length(V(side), V(m + 0xd0), fpu_mul(k, sin_vu(a)));
        vec3_add(V(vel), V(vel), V(move));
        vec3_add(V(vel), V(vel), V(side));
        lq_sq(vel, move);  // (the drift just built is dropped: the dust moves with the board)
        const uint32_t dust = dust_words();
        const float count = static_cast<float>(lw(dust + 0x1c));
        swc1(vel + 0xc, count);
        if (lbu(m + 0xbc) == 0) {
            const int32_t life = rand_range(frame_scale_i(0xa), frame_scale_i(0xd));
            spawn_particle_4(joint, vel, lwu(dust), lwu(dust + 4), static_cast<uint32_t>(life), 0xf, 0xf, 1);
        } else {
            vec3_scale(V(slow), V(move), u2f(0x3f6b851fu));
            const int32_t life = rand_range(frame_scale_i(0xf), frame_scale_i(0x12));
            spawn_particle_4(joint, slow, 0x5032f0d2u, lwu(dust + 4), static_cast<uint32_t>(life), 0x1e, 0x14, 1);
            const int32_t life2 = rand_range(frame_scale_i(8), frame_scale_i(0x11));
            spawn_particle_15_v(V(joint), V(vel), lwu(dust), lwu(dust + 4), static_cast<uint32_t>(life2), 1, 15000.0f);
        }
    }
    sb(m + 0xbc, 0);
    ee_stack_free(0xc0);
}

// Once the hero has raced 3 s (+0x894, frame-scaled) and the controller's wait is over: each boost pad of class 0x473 in
// the hero's board's first group (pvars +0x44) within 8 of the controller's point (0.8 up) and inside the pad's box
// (0.5 x 2.5 x 2.5 in the pad's frame), and each of class 0x474 in its second group (+0x48) within 8 of the point and
// inside 2.8 x 1.4 x 0.5, adds 2 s to the boost and waits 1 s.
// orig: L5:0x00307010 L16:0x002cfc00
void hoverboard_boost_pads(uint32_t m, uint32_t at, uint32_t boost, uint32_t wait) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(HERO + 0x894) < frame_scale_i(0xb4)) return;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + wait))) == 0) return;
    const uint32_t board = lwu(HERO + 0x86c);
    if (board == 0) return;
    const uint32_t bpv = lwu(board + 0x78);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t d = sp, from = sp + 0x10, frame = sp + 0x20, it0 = sp + 0x60, it1 = sp + 0x64;
    // the pad `p` within 8 of `point` and inside the box: a boost
    auto over = [&](uint32_t p, uint32_t point, float bx, float by, float bz) {
        vec3_sub(V(d), V(p + 0x10), V(point));
        if (8.0f < vec3_length(V(d))) return;
        mat4_identity(M(frame));
        mat3_transpose(M(frame), M(p + 0xc0));
        mat4_transform(V(d), V(d), M(frame));
        if (!(abs_f(lwc1(d)) < bx) || !(abs_f(lwc1(d + 4)) < by) || !(abs_f(lwc1(d + 8)) < bz)) return;
        sh(pv + boost, lhu(pv + boost) + static_cast<uint32_t>(frame_scale_i(0x78)));
        sw(pv + wait, static_cast<uint32_t>(frame_scale_i(0x3c)));
    };
    const int32_t group = lw(bpv + 0x44);
    if (group != -1) {
        group_first(it0, group, 0, 0);
        while (lwu(it0) != 0) {
            const uint32_t p = lwu(it0);
            if (lh(p + 0xa6) == 0x473) {
                lq_sq(from, pv + at);
                swc1(from + 8, fpu_add(lwc1(from + 8), u2f(0x3f4ccccdu)));
                over(p, from, 0.5f, 2.5f, 2.5f);
            }
            group_next(it0, lwu(it0), 0, 0);
        }
    }
    const int32_t group2 = lw(bpv + 0x48);
    if (group2 != -1) {
        group_first(it1, group2, 0, 0);
        while (lwu(it1) != 0) {
            const uint32_t p = lwu(it1);
            if (lh(p + 0xa6) == 0x474) over(p, pv + at, u2f(0x40333333u), u2f(0x3fb33333u), 0.5f);
            group_next(it1, lwu(it1), 0, 0);
        }
    }
    ee_stack_free(0xf0);
}

// The race's HUD element, start: 3.5 s to show (frame-scaled 3 s and a half), at its place, the laps' four words (0x15faec
// down to 0x15fae0) -1, its size.
// orig: L5:0x00262ae8 L16:0x002351f8
void race_hud_start(uint32_t e) {
    const int32_t life = frame_scale_i(0xb4) + 0x1e;
    sh(e + 0x48, 0);
    sw(e + 0x7c, static_cast<uint32_t>(life));
    sh(e + 0x4a, 0);
    for (uint32_t k = 0; k < 0x10; k += 4) sw(OV(0x0015faec) - k, 0xffffffffu);
    hud_element_size(e);
}

// Its update: while the hero is on the board (mode 0x16) it stays (half a second more, its fade +0x6c 5), else it fades
// out (-6).
// orig: L5:0x002661e8 L16:0x002388f8
void race_hud_update(uint32_t e) {
    if (lw(HERO + 0x208c) == 0x16) {
        sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sw(e + 0x6c, 5);
    } else {
        sw(e + 0x6c, 0xfffffffau);
    }
}

// Its reset: 3.5 s to show, its size 0x80 x 0x80, at its place.
// orig: L5:0x002661a0 L16:0x002388b0
void race_hud_reset(uint32_t e) {
    sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(0xb4) + 0x1e));
    sw(e + 0x5c, 0x80);
    sh(e + 0x4a, 0);
    sw(e + 0x58, 0x80);
    sh(e + 0x48, 0);
}

// Its draw (while its fade +0x6c is above 0): in a race (0x13d388) a box and the lap "<text 0x50a6>: <the hero's lap
// +0x8a8>"; always the time "<text 0x5241>:  m:ss:hh" of the hero's race (+0x894: 50 or 60 frames a second by the
// region's frame rate 0x15ed80), each line centred on 0x15fa20 at its height from the screen's bottom (0x13e504), in
// two colours with a shadow.
// orig: L5:0x00266710 L16:0x00238e20
void race_hud_draw(uint32_t e) {
    if (!(lw(e + 0x6c) > 0)) return;
    const bool fifty = lw(0x0015ed80) != 0;
    const int32_t bottom = lw(0x0013e504);
    const int32_t margin = fifty ? 0xa : 0x12;
    const int32_t box_y = bottom - (lw(OV(0x0015fa08)) + margin);
    const int32_t lap_y = bottom - (lw(OV(0x0015fa24)) + margin);
    const int32_t time_y = bottom - (lw(OV(0x0015fa34)) + margin);
    const int32_t t = lw(HERO + 0x894);
    const int32_t per_minute = fifty ? 3000 : 3600;
    const int32_t minutes = ee_div(t, per_minute);
    const int32_t per_second = ee_div(per_minute, 60);
    const int32_t rest = t - minutes * per_minute;
    const int32_t seconds = ee_div(rest, per_second);
    const int32_t hundredths = ee_div((rest - seconds * per_second) * 100, per_second);
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t buf = sp;
    const uint64_t colour1 = sx64(lw(OV(0x0015fa18))), colour2 = sx64(lw(OV(0x0015fa1c)));
    if (lbu(0x0013d388) != 0) {
        hud_box(lw(OV(0x0015fa04)), box_y, lw(OV(0x0015fa0c)), lw(OV(0x0015fa10)), sx64(lw(OV(0x0015fa14))));
        int32_t x = lw(OV(0x0015fa20));
        const int32_t lap = lw(HERO + 0x8a8);
        libc_sprintf(buf, OV(0x0015f778), {sub_00203ad8(TXT(0x50a6)), sx64(lap)});  // ("%s: %d": its width centres it)
        x -= text_width_small(buf, -1) >> 1;
        libc_sprintf(buf, OV(0x0015f9c8), {sub_00203ad8(TXT(0x50a6))});  // ("%s: ")
        hud_line(x, lap_y, colour1, buf);
        x += text_width_small(buf, -1);
        libc_sprintf(buf, OV(0x0015fa40), {sx64(lw(HERO + 0x8a8))});  // (" %d")
        hud_line(x, lap_y, colour2, buf);
    }
    int32_t x = lw(OV(0x0015fa20));
    libc_sprintf(buf, OV(0x0015fa48), {sub_00203ad8(TXT(0x5241))});  // ("%s:  0:00:00")
    x -= text_width_small(buf, -1) >> 1;
    libc_sprintf(buf, OV(0x0015fa58), {sub_00203ad8(TXT(0x5241))});  // ("%s:  ")
    hud_line(x, time_y, colour1, buf);
    x += text_width_small(buf, -1);
    libc_sprintf(buf, OV(0x0015fa60), {sx64(minutes), sx64(seconds), sx64(hundredths)});  // ("%d:%02d:%02d")
    hud_line(x, time_y, colour2, buf);
    ee_stack_free(0x80);
}

}  // namespace rac1
