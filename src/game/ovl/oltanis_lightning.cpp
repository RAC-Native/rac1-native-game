// Oltanis (level 14), class 684: the level's arrival scene and its lightning. On its first frame it starts the arrival
// cutscene (scene 0) unless the "arrival scene seen" byte 0x13d3f0 is set (then it waits in state 1 for good), hides the
// ship, and shows the held moby again once the scene is over (game mode 0x15f5c4 no longer 2). From then on (state 3),
// every so often (0x161c54 frames) a bolt of lightning strikes one of the points of its path (+0x00) that is on screen
// (up to the first five found): the bolt's state is the level's (0x161c50 .. 0x161cfc: its timers, its ends 0x161cc0 /
// 0x161cd0, its flash) and its points are in the level's data (0x1dfd80: 40 points, 0x1e0000: their timers, 0x1e0050 /
// 0x1e0350: three branches of 16 points and their timers, 0x161cf8: where they leave the bolt). The bolt flashes, rumbles
// (the level's sound 0), smokes where it struck while bright, and is drawn by L14:0x2edc18 (still generated).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/shared_level_helpers.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

// orig: L14:0x00294028 -- the level's sound `idx` (from the level's base 0x15f574) at moby `m`; its channel, or -1
int32_t level_sound_at(int32_t idx, uint32_t flags, uint32_t m) {
    const int32_t n = idx + lw(L14_ADDR(0x0015f574));
    if (!(n < lw(L14_ADDR(0x0015f5f0)))) return -1;
    const int32_t ch = sound_start(lwu(L14_ADDR(0x0015f5f4)) + (static_cast<uint32_t>(n) << 5), flags, m, 0, 0x400);
    if (ch >= 0) {
        const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
        sh(slot + 0x7e, static_cast<uint16_t>(n));
        sw(slot + 0x88, m);
    }
    return ch;
}

// orig: L14:0x002ee6b0 -- `n` puffs of smoke at `pos` (each two particles 0x35 of opposite spins); the place itself is
// jittered by up to 0.15 a puff
void lightning_smoke(uint32_t pos, int32_t n) {
    for (int32_t k = n; k > 0; --k) {
        const float size = rand_range_f(lwc1(L14_ADDR(0x00161cac)), lwc1(L14_ADDR(0x00161cb0)));
        int32_t spin = rand_mod(2);
        if (spin == 0) spin = -1;
        const float yaw = rand_angle();
        const float pitch = rand_range_f(u2f(0x3eb2b8c2u), u2f(0x3fc49809u));
        const float speed = rand_range_f(lwc1(L14_ADDR(0x00161ca0)), lwc1(L14_ADDR(0x00161ca4)));
        const uint32_t vel = ee_stack_alloc(0x10);
        vec_from_spherical_v(V(vel), speed, yaw, pitch);
        swc1(pos + 0x0, fpu_add(lwc1(pos + 0x0), rand_range_signed_f(0.0f, u2f(0x3e19999au))));
        swc1(pos + 0x4, fpu_add(lwc1(pos + 0x4), rand_range_signed_f(0.0f, u2f(0x3e19999au))));
        swc1(pos + 0x8, fpu_add(lwc1(pos + 0x8), rand_range_signed_f(0.0f, u2f(0x3e19999au))));
        uint32_t p = spawn_particle_35(pos, static_cast<uint32_t>(frame_scale_i(60)), 0x7f7f2020u, 0, static_cast<uint32_t>(spin), vel,
                                       fpu_mul(size, 0.1f), size, lwc1(L14_ADDR(0x00161ca8)));
        if (p != 0) sb(p + 0x9, static_cast<uint8_t>(float_to_int(4.0f) + 0x70));
        spin = -spin;
        p = spawn_particle_35(pos, static_cast<uint32_t>(frame_scale_i(60)), 0x7f7f7f7fu, 1, static_cast<uint32_t>(spin), vel,
                              fpu_mul(size, u2f(0x3d8f5c29u)), fpu_mul(size, u2f(0x3f333333u)), lwc1(L14_ADDR(0x00161ca8)));
        if (p != 0) sb(p + 0x9, static_cast<uint8_t>(float_to_int(4.0f) + 0x70));
        ee_stack_free(0x10);
    }
}

uint32_t bolt_point(int32_t k) { return L14_ADDR(0x001dfd80) + (static_cast<uint32_t>(k) << 4); }
uint32_t bolt_timer(int32_t k) { return L14_ADDR(0x001e0000) + (static_cast<uint32_t>(k) << 1); }
uint32_t branch_point(int32_t b, int32_t k) { return L14_ADDR(0x001e0050) + (static_cast<uint32_t>(b) << 8) + (static_cast<uint32_t>(k) << 4); }
uint32_t branch_timer(int32_t b, int32_t k) { return L14_ADDR(0x001e0350) + (static_cast<uint32_t>(b) << 5) + (static_cast<uint32_t>(k) << 1); }

// a timer counted up to the bolt's life 0x161c70 at most
void count_up_to_life(uint32_t t) {
    sh(t, static_cast<uint16_t>(lhu(t) + 1));
    if (frame_scale_i(lw(L14_ADDR(0x00161c70))) < lh(t)) sh(t, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x00161c70)))));
}

// orig: L14:0x002ed530 -- the bolt: laid out once per strike (its 40 points from 0x161cc0 toward 0x161cd0 with a random
// sideways walk, the points' timers staggered, three branches), then each frame the timers counted, the bolt's tip
// pulled in, the flash and the fade, the rumble once, the smoke while bright, the draw callback queued
void lightning_bolt(uint32_t m) {
    if (lw(L14_ADDR(0x00161c58)) == 0) {
        sw(L14_ADDR(0x00161cf4), 1);
        sw(L14_ADDR(0x00161c58), 1);
        sw(L14_ADDR(0x00161ce8), m);
        sw(L14_ADDR(0x00161c6c), 0);
        Vec4 span{};
        vec3_sub(span, V(L14_ADDR(0x00161cd0)), V(L14_ADDR(0x00161cc0)));
        sb(L14_ADDR(0x00161ce0), 0);
        swc1(L14_ADDR(0x00161c74), fpu_div(vec3_length(span), int_to_float(40)));
        for (int32_t k = 0; k < 40; ++k) {
            const uint32_t p = bolt_point(k);
            swc1(p + 0x8, 0.0f);
            swc1(p + 0x4, 0.0f);
            swc1(p + 0xc, 1.0f);
            swc1(p + 0x0, fpu_mul(lwc1(L14_ADDR(0x00161c74)), static_cast<float>(k)));
            if (k < 39) {  // (point 0 walks from the word before the array, as the original does)
                const float walk = fpu_add(lwc1(p - 0x8), rand_range_signed_f(lwc1(L14_ADDR(0x00161c8c)), lwc1(L14_ADDR(0x00161c90))));
                const float most = lwc1(L14_ADDR(0x00161c94));
                if (most < walk) swc1(p + 0x8, most);
                else if (walk < -most) swc1(p + 0x8, -most);
                else swc1(p + 0x8, walk);
            }
            const float along = fpu_div(int_to_float(k), int_to_float(40));
            const float delay = static_cast<float>(-frame_scale_i(lw(L14_ADDR(0x00161c70))));
            sh(bolt_timer(k), static_cast<uint16_t>(float_to_int(fpu_add(fpu_mul(delay, along), 0.0f))));
        }
        vec3_lerp(V(bolt_point(38)), V(bolt_point(37)), V(bolt_point(39)), 0.5f);
        const int32_t life = frame_scale_i(rand_range(lw(L14_ADDR(0x00161c98)), lw(L14_ADDR(0x00161c9c))));
        sw(L14_ADDR(0x00161cb4), static_cast<uint32_t>(life));
        sw(L14_ADDR(0x00161cec), static_cast<uint32_t>(life));
        const int32_t tail = frame_scale_i(lw(L14_ADDR(0x00161c70)));
        swc1(L14_ADDR(0x00161cf0), fpu_div(1.0f, int_to_float(lw(L14_ADDR(0x00161cb4)) - tail)));
        float side = rand_mod(2) != 0 ? 1.0f : -1.0f;
        for (int32_t b = 0; b < 3; ++b) {  // the branches: from a random point 5 .. 22, alternately to either side
            const int32_t from = rand_range(5, 0x16);
            const float next_side = -side;
            sw(L14_ADDR(0x00161cf8) + (static_cast<uint32_t>(b) << 2), static_cast<uint32_t>(from));
            sh(branch_timer(b, 0), lhu(bolt_timer(from)));
            float len = rand_range_f(1.0f, 10.0f);
            float dir = fpu_mul(rand_range_f(u2f(0x3f060a92u), u2f(0x3fb2b8c2u)), side);
            swc1(branch_point(b, 0) + 0x0, fpu_mul(len, cos_vu(dir)));
            swc1(branch_point(b, 0) + 0x8, fpu_mul(len, sin_vu(dir)));
            for (int32_t k = 1; k < 16; ++k) {
                len = rand_range_f(1.0f, 10.0f);
                const float turn = fpu_mul(rand_range_signed_f(0.0f, 40.0f), u2f(0x3c8efa35u));
                const float bent = angle_add(dir, turn);
                // the turn is taken back when it would bend the branch past 80 degrees or across the bolt's line
                if (u2f(0x3fb2b8c2u) < abs_f(bent) || (0.0f < dir && bent < 0.0f) || (dir < 0.0f && 0.0f < bent)) dir = angle_add(dir, -turn);
                else dir = bent;
                swc1(branch_point(b, k) + 0x0, fpu_mul(len, cos_vu(dir)));
                swc1(branch_point(b, k) + 0x8, fpu_mul(len, sin_vu(dir)));
                sh(branch_timer(b, k), lhu(bolt_timer(lw(L14_ADDR(0x00161cf8) + (static_cast<uint32_t>(b) << 2)) + k)));
            }
            side = next_side;
        }
    }
    if (m != lwu(L14_ADDR(0x00161ce8))) return;

    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(L14_ADDR(0x00161cb4))));
    for (int32_t k = 0; k < 40; ++k) {
        count_up_to_life(bolt_timer(k));
        if (k != 0 && !(k < 35)) {  // the last five points pulled toward the line
            const uint32_t p = bolt_point(k);
            const float t = fpu_div(int_to_float(k - 0x23), 5.0f);
            const float z = lwc1(p + 0x8);
            swc1(p + 0x8, fpu_add(z, fpu_mul(fpu_sub(0.0f, z), t)));
        }
    }
    frame_scale_i(lw(L14_ADDR(0x00161c70)));  // (the original computes it here and drops it)
    for (int32_t b = 0; b < 3; ++b)
        for (int32_t k = 0; k < 16; ++k) count_up_to_life(branch_timer(b, k));
    const int32_t tail = frame_scale_i(lw(L14_ADDR(0x00161c70)));
    if (!(lw(L14_ADDR(0x00161cb4)) < lw(L14_ADDR(0x00161cec)) - tail)) {  // (the bolt still growing: not drawn bright)
        sb(L14_ADDR(0x00161ce0), 0);
    } else {
        // the flash for the first second of a long bolt: up to a full turn of a sine
        if (frame_scale_i(60) < lw(L14_ADDR(0x00161cec)) && lwc1(L14_ADDR(0x00161c6c)) < u2f(0x40c90fdbu)) {
            sh(L14_ADDR(0x00161ce2), static_cast<uint16_t>(float_to_int(fpu_mul(sin_vu(lwc1(L14_ADDR(0x00161ce4))), 128.0f))));
            const float phase = angle_add(lwc1(L14_ADDR(0x00161ce4)), lwc1(L14_ADDR(0x00161c68)));
            const float step = fpu_mul(lwc1(L14_ADDR(0x00161c68)), lwc1(0x0015ed60u));
            swc1(L14_ADDR(0x00161ce4), phase);
            swc1(L14_ADDR(0x00161c6c), fpu_add(lwc1(L14_ADDR(0x00161c6c)), step));
        } else {
            sh(L14_ADDR(0x00161ce2), 0);
        }
        const float fade = fpu_add(fpu_mul(fpu_mul(int_to_float(lw(L14_ADDR(0x00161cb4))), lwc1(L14_ADDR(0x00161cf0))), -255.0f), 255.0f);
        sb(L14_ADDR(0x00161ce0), static_cast<uint8_t>(lbu(L14_ADDR(0x00161ce2)) + float_to_int(fade)));
        if (lw(L14_ADDR(0x00161cf4)) != 0) {  // the rumble, once a strike
            level_sound_at(0, 0, m);
            sw(L14_ADDR(0x00161cf4), 0);
        }
        if (lbu(L14_ADDR(0x00161ce0)) < 50) lightning_smoke(L14_ADDR(0x00161cc0), 3);
    }
    draw_callback_add(L14_ADDR(0x002edc18), m);
}

}  // namespace

// orig: L14:0x002ed280
void olt_class684_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        if (lw(0x0015ed84u) == 0xe) sh(0x00141628u, 1);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sw(L14_ADDR(0x00161c58), 0);
        sw(L14_ADDR(0x00161c50), 0);
    }
    const uint8_t st = lbu(m + 0x20);
    if (st == 1) {  // the arrival scene, once
        if (lw(L14_ADDR(0x0015f5c4)) != 0 || lbu(0x0013d3f0u) != 0) return;
        cutscene_start(0);
        sb(0x0013d3f0u, 1);
        sb(m + 0x20, 2);
        ship_hide();
        return;
    }
    if (st == 2) {  // the scene playing
        if (lw(L14_ADDR(0x0015f5c4)) == 2) return;
        sb(m + 0x20, 3);
        held_moby_show();
        return;
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(L14_ADDR(0x00161c50)))) == 0) return;
    if (lw(L14_ADDR(0x00161c58)) == 0) {  // a new strike: one of the first five points of the path on screen
        const uint32_t path = lvd::path(lw(pv + 0x0));
        int32_t seen[5] = {};
        int32_t count = 0;
        const uint32_t at = ee_stack_alloc(0x10);  // (point_on_screen takes the point by address)
        for (int32_t k = 0; k < lw(path) && count < 5; ++k) {
            V(at) = V(path + 0x10u + (static_cast<uint32_t>(k) << 4));
            swc1(at + 0x8, fpu_add(lwc1(at + 0x8), lwc1(L14_ADDR(0x00161c64))));
            swc1(at + 0xc, lwc1(L14_ADDR(0x00161c60)));
            if (point_on_screen(at, 255.0f) > 0) seen[count++] = k;
        }
        ee_stack_free(0x10);
        if (count > 0) {
            const int32_t pick = seen[rand_mod(count)];
            V(L14_ADDR(0x00161cc0)) = V(path + 0x10u + (static_cast<uint32_t>(pick) << 4));
            swc1(L14_ADDR(0x00161ccc), 1.0f);
            V(L14_ADDR(0x00161cd0)) = V(L14_ADDR(0x00161cc0));
            swc1(L14_ADDR(0x00161cd8), lwc1(L14_ADDR(0x00161c5c)));
            swc1(L14_ADDR(0x00161cd0), fpu_add(lwc1(L14_ADDR(0x00161cd0)), rand_range_signed_f(0.0f, 20.0f)));
            swc1(L14_ADDR(0x00161cd4), fpu_add(lwc1(L14_ADDR(0x00161cd4)), rand_range_signed_f(0.0f, 20.0f)));
            V(m + 0x10) = V(L14_ADDR(0x00161cc0));
        }
    }
    lightning_bolt(m);
    if (lw(L14_ADDR(0x00161cb4)) == 0) {  // the bolt over: the next one after 0x161c54 frames
        sw(L14_ADDR(0x00161c50), static_cast<uint32_t>(frame_scale_i(lw(L14_ADDR(0x00161c54)))));
        sw(L14_ADDR(0x00161c58), 0);
    }
}

}  // namespace rac1
