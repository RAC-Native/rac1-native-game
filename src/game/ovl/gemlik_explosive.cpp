// Gemlik Base's (level 13) classes 1261 and 1634: a charge that blows up when it is hit hard enough, its blast, and
// the pieces of its debris. Level 13
// only: the addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in
// gemlik_mobys.cpp.
//
// Class 1261's pvars: +0x20 its hit record (+0x20 its health, +0x26 the flinch's timer), +0x29 cleared every frame,
// +0x60 its colour pulse, +0x70 the blast's frames. State 0: it waits, its wobble (+0x40 / +0x44) decaying; a hit
// with the reactions 1 / 2 zeroes its health; a hit that takes the rest of its health blows it up (its bolts, the
// debris 0x3073c8, a puff, a sound: state 1), a lesser one takes its damage and sets it wobbling (2 degrees either
// way). State 1, the blast: every frame the mobys in a sphere 1 above it (its radius from L13:0x161e30 to 0x161e34 over
// L13:0x161e2c frames) are hit (L13:0x161e38 damage); past half the time it is hidden (and puffs), before that it
// fades; at the end a puff and it is retired.
// Its debris (0x3073c8): ten slow pieces and four fast ones (class 1634, 0x662, gml_30c138_l13: kinds 0 / 1) flung
// about the up axis, one toward the camera, sparks of the two colour tables L13:0x1f5560 / 0x1f5578 (more the nearer
// the camera), flashes (two more when the camera is farther than 9).
// Class 1634, a piece of the debris: pvars +0 its velocity (a frame's move; gravity 14.6), +0x10 / +0x14 its spins,
// +0x18 its frames left and +0x1a its life (16 bits), +0x1c its scale; the moby's +0xbc its kind (1: the fast ones,
// with two trails of smoke each frame (particle 2, the settings L13:0x161f08 ..) and stopped by what they meet). It
// shrinks over the last quarter of its life; retired at its life's end or below the space's floor (0 on any axis).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/variants_l13.h>

namespace rac1 {

using namespace gml;

// A piece of debris (class 1634, 0x662) at `pos` flying along `vel` (EE vectors) for `life` frames (16 bits), of kind
// `kind` (0 the slow ones, smaller; 1 the fast ones): its spins, its colour, its scale. The piece, or 0.
// orig: L13:0x0030c138
uint32_t gml_30c138_l13(uint32_t pos, uint32_t vel, int32_t life, uint32_t kind) {
    const uint32_t s = moby_spawn(0x662);
    if (s == 0) return 0;
    sb(s + 0x31, 1);
    sh(s + 0x32, 0xff);
    const uint32_t pv = lwu(s + 0x78);
    sb(s + 0x30, 0xff);
    sb(s + 0x23, static_cast<uint32_t>(rand_range(0x40, 0x80)));
    moby_set_color_hi(s, 0x20, 0x60, 0x10);
    lq_sq(s + 0x10, pos);
    lq_sq(pv + 0, vel);
    const float two_pi = u2f(0x40c90fdbu), four_pi = u2f(0x41490fdbu);
    swc1(pv + 0x10, rand_range_f(fpu_mul(dt(), two_pi), fpu_mul(dt(), four_pi)));
    swc1(pv + 0x14, rand_range_f(fpu_mul(dt(), two_pi), fpu_mul(dt(), four_pi)));
    sh(pv + 0x1a, static_cast<uint32_t>(life));
    sh(pv + 0x18, static_cast<uint32_t>(life));
    if (kind == 0) swc1(s + 0x2c, fpu_mul(lwc1(s + 0x2c), rand_range_f(0.5f, 0.75f)));
    swc1(pv + 0x1c, lwc1(s + 0x2c));
    sb(s + 0xbc, kind);
    swc1(s + 0x2c, fpu_mul(lwc1(s + 0x2c), rand_range_f(2.0f, 5.0f)));
    moby_update_transform(s);
    return s;
}

// Class 1261's debris at its place (`m` the charge).
// orig: L13:0x003073c8
void gml_3073c8_l13(uint32_t m) {
    const Frame f(400);
    const uint32_t sp = f.sp;
    const uint32_t to_cam = sp, drift = sp + 0x10, up = sp + 0x20, along = sp + 0x30, at = sp + 0x40, vel = sp + 0x50,
                   rnd = sp + 0x60;
    const float scale = 1.5f;
    sq_zero(up);
    swc1(up + 8, fpu_add(dt(), dt()));
    lq_sq(drift, up);  // (0, 0, 2 dt): the pieces' common drift, 1.5 times that
    sq_zero(up);
    swc1(up + 8, 1.0f);
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
    vec3_scale(V(drift), V(drift), scale);
    vec3_sub(V(to_cam), V(L13_ADDR(0x001670c0)), V(at));
    const float d = vec3_length(V(to_cam));
    // a piece's velocity: a random direction across the up axis, turned up by a random amount, `lo` .. `hi` a second
    auto fling = [&](float lo, float hi) {
        sq_zero(rnd);
        swc1(rnd + 0, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 4, rand_range_f(-1.0f, 1.0f));
        lq_sq(vel, rnd);
        vec3_set_length(V(along), V(up), vec3_dot(V(vel), V(up)));
        vec3_sub(V(vel), V(vel), V(along));
        vec3_set_length(V(along), V(up), rand_range_f(0.0f, 1.0f));
        vec3_add(V(vel), V(vel), V(along));
        const float k = fpu_mul(scale, rand_range_f(lo, hi));
        vec3_set_length(V(vel), V(vel), fpu_mul(k, dt()));
        vec3_add(V(vel), V(vel), V(drift));
    };
    for (int k = 0; k < 10; ++k) {
        fling(3.5f, 6.5f);
        const int32_t lo = frame_scale_i(0x3c), hi = frame_scale_i(0x78);
        gml_30c138_l13(at, vel, rand_range(lo, hi), 0);
    }
    const float d2 = fpu_add(d, d);
    for (int k = 0; k < 4; ++k) {
        fling(6.5f, 10.0f);
        const int32_t lo = frame_scale_i(0x3c), hi = frame_scale_i(0x5a);
        gml_30c138_l13(at, vel, rand_range(lo, hi), 1);
    }
    {  // one toward the camera
        sq_zero(rnd);
        swc1(rnd + 0, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 4, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 8, rand_range_f(-1.0f, 1.0f));
        swc1(to_cam + 8, fpu_add(lwc1(to_cam + 8), fpu_mul(d, 0.5f)));
        lq_sq(vel, rnd);
        vec3_set_length(V(vel), V(vel), fpu_mul(fpu_div(d, 5.0f), dt()));
        vec3_set_length(V(to_cam), V(to_cam), fpu_mul(d2, dt()));
        vec3_add(V(vel), V(vel), V(to_cam));
        vec3_clamp_length_vu(V(vel), V(vel), fpu_mul(dt(), 10.0f));
        const int32_t lo = frame_scale_i(0x3c), hi = frame_scale_i(0x5a);
        gml_30c138_l13(at, vel, rand_range(lo, hi), 1);
    }
    // sparks of the two colour tables L13:0x1f5560 / 0x1f5578 (six words each), more the nearer the camera
    const int32_t n = d < 6.0f ? float_to_int(d) + 1 : 4;
    const float near = d < 7.0f ? fpu_sub(7.0f, d) : 0.0f;
    const float size = fpu_mul(scale, 400000.0f);
    for (int32_t k = n; k > 0; --k) {
        float speed = fpu_mul(scale, rand_range_f(8.0f, 10.0f));
        for (uint32_t w = 0; w < 0x18; w += 4) sw(sp + 0x70 + w, lwu(L13_ADDR(0x001f5560) + w));
        for (uint32_t w = 0; w < 0x18; w += 4) sw(sp + 0x90 + w, lwu(L13_ADDR(0x001f5578) + w));
        speed = fpu_sub(fpu_mul(speed, dt()), fpu_mul(near, dt()));
        const uint32_t c0 = sp + 0x70 + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const uint32_t c1 = sp + 0x90 + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const int32_t life = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
        const int32_t count = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
        spawn_particle_0b(at, drift, lwu(c0), lwu(c1), static_cast<uint32_t>(life), static_cast<uint32_t>(count), 0, 0, size, speed);
    }
    if (9.0f < d) {
        fx_4a8_spawn(m, at, drift, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7f, 0x7f, 0x7f, 0x20, fpu_mul(scale, 4.0f));
        fx_4a8_spawn(m, at, drift, static_cast<uint32_t>(frame_scale_i(0x18)), 0x20, 0x7f, 0, 0x20, fpu_mul(scale, 4.0f));
    }
    fx_4a8_spawn(m, at, drift, static_cast<uint32_t>(frame_scale_i(0x14)), 0x3f, 0x7f, 0, 0x30, fpu_mul(scale, 4.0f));
    fx_4a8_spawn(m, at, drift, static_cast<uint32_t>(frame_scale_i(0x1b)), 0x10, 0x60, 0, 0x40, fpu_mul(scale, 3.5f));
    fx_4a8_spawn(m, at, drift, static_cast<uint32_t>(frame_scale_i(0x1d)), 0, 0x20, 0, 0x20, fpu_mul(scale, 3.0f));
}

// orig: L13:0x0030c3b8
void gml_class1634_update_l13(uint32_t m) {
    const Frame f(336);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, from = sp + 0x10, vel = sp + 0x30, dir = sp + 0x40;
    lq_sq(from, pos);
    if (lbu(m + 0xbc) == 1) {  // its two trails: two puffs of each
        const float deg = u2f(0x3c8efa35u);
        // a puff at `at` (about the piece by `jitter`), rising `rise` (0.9 .. 1.1 / `lo` .. `hi` of it), along its
        // heading turned by up to `spread` degrees (L13 data), with the settings at `c` (L13:0x161f08 + ..)
        auto puff = [&](uint32_t at, float jitter, uint32_t rise, uint32_t spread, uint32_t speed, uint32_t up, uint32_t t,
                        float lo, float hi, float hi_t, uint32_t c, bool idle_rand) {
            lq_sq(at, pos);
            swc1(at + 0, fpu_add(lwc1(at + 0), rand_range_f(-jitter, jitter)));
            swc1(at + 4, fpu_add(lwc1(at + 4), rand_range_f(-jitter, jitter)));
            swc1(at + 8, fpu_add(lwc1(at + 8), rand_range_f(-jitter, jitter)));
            if (idle_rand) {  // (the original draws a number here and drops it)
                const float k = lwc1(L13_ADDR(0x00161f58));
                rand_range_f(-k, k);
            }
            sq_zero(vel);
            {
                const float r = fpu_mul(lwc1(rise), dt());
                const float rhi = fpu_mul(r, hi), rlo = fpu_mul(r, lo);
                swc1(vel + 8, rand_range_f(rlo, rhi));
            }
            const float k = lwc1(spread);
            const float turn = fpu_mul(rand_range_f(-k, k), deg);
            swc1(dir + 0, fpu_mul(cos_vu(angle_add(atan2_f(lwc1(pv + 0), lwc1(pv + 4)), turn)), fpu_mul(lwc1(speed), dt())));
            {
                const float s0 = sin_vu(angle_add(atan2_f(lwc1(pv + 0), lwc1(pv + 4)), turn));
                const float sp_ = fpu_mul(lwc1(speed), dt());
                sw(dir + 8, 0);
                const float u = fpu_mul(lwc1(up), dt());
                const float uhi = fpu_mul(u, hi), ulo = fpu_mul(u, lo);
                swc1(dir + 4, fpu_mul(s0, sp_));
                swc1(dir + 8, rand_range_f(ulo, uhi));
            }
            const float l0 = lwc1(t + 0);
            const int32_t n = float_to_int(frame_scale_f(rand_range_f(l0, fpu_add(l0, l0))));
            const float l1 = lwc1(t + 4);
            const int32_t h32 = float_to_int(frame_scale_f(rand_range_f(l1, fpu_mul(l1, hi_t))));
            const float l2 = lwc1(t + 8);
            const int32_t h34 = float_to_int(frame_scale_f(rand_range_f(l2, fpu_mul(l2, hi_t))));
            spawn_particle_02(at, vel, dir, lwu(c), lwu(c + 4), static_cast<uint32_t>(n), static_cast<uint32_t>(h32), static_cast<uint32_t>(h34), -1);
        };
        swc1(vel + 0xc, lwc1(L13_ADDR(0x00161f5c)));
        swc1(dir + 0xc, lwc1(L13_ADDR(0x00161f60)));
        for (int k = 0; k < 2; ++k)
            puff(sp + 0x20, u2f(0x3d851eb8u), L13_ADDR(0x00161f30), L13_ADDR(0x00161f50), L13_ADDR(0x00161f24), L13_ADDR(0x00161f28),
                 L13_ADDR(0x00161f18), u2f(0x3f666666u), u2f(0x3f8ccccdu), u2f(0x3f99999au), L13_ADDR(0x00161f08), false);
        swc1(vel + 0xc, lwc1(L13_ADDR(0x00161f64)));
        swc1(dir + 0xc, lwc1(L13_ADDR(0x00161f68)));
        for (int k = 0; k < 2; ++k)
            puff(sp + 0x50, u2f(0x3dcccccdu), L13_ADDR(0x00161f44), L13_ADDR(0x00161f54), L13_ADDR(0x00161f48), L13_ADDR(0x00161f4c),
                 L13_ADDR(0x00161f34), u2f(0x3f333333u), 1.5f, u2f(0x3f99999au), L13_ADDR(0x00161f10), true);
    }
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    vec3_add(V(pos), V(pos), V(pv));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(dt_sq(), u2f(0x4169999au))));
    if (lwc1(m + 0x10) < 0.0f || lwc1(m + 0x14) < 0.0f || lwc1(m + 0x18) < 0.0f) {
        moby_retire(m);
        return;
    }
    {  // its last quarter: it shrinks
        const int32_t left = lh(pv + 0x18), quarter = lh(pv + 0x1a) / 4;
        if (left < quarter) swc1(m + 0x2c, fpu_div(fpu_mul(lwc1(pv + 0x1c), int_to_float(left)), int_to_float(lh(pv + 0x1a) / 4)));
    }
    if (tick16(pv + 0x18) != 0) {
        moby_retire(m);
        return;
    }
    if (lbu(m + 0xbc) == 1 && segment_collide_v(V(from), V(pos), 2, lwu(0x001413d0), nullptr) != 0) {
        particle34_puff_2(m);
        moby_retire(m);
    }
}

// orig: L13:0x00307b10
void gml_class1261_update_l13(uint32_t m) {
    const Frame f(176);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hs = pv + 0x20, pulse = pv + 0x60;
    switch (lbu(m + 0x20)) {
    case 0: {  // waiting
        sb(pv + 0x29, 0);
        swc1(m + 0x40, fpu_mul(lwc1(m + 0x40), u2f(0x3f333333u)));
        swc1(m + 0x44, fpu_mul(lwc1(m + 0x44), u2f(0x3f333333u)));
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330001), 0);
        if (hit != 0) debug_printf_stub();  // (the format L13:0x1f5590 with its class, uid and the hit's damage)
        int32_t st = lw(sp + 0x10);
        const int32_t react = moby_hit_react_r(m, hit, hs, 0, st, nullptr, nullptr, 4);
        sw(sp + 0x10, static_cast<uint32_t>(st));
        if (react == 1 || react == 2) sw(hs, 0);  // (the table L13:0x1f55c0: reactions 1 and 2 (0x00307c20))
        if (st >= 2) {
            const float dmg = lwc1(hit + 0x2c), health = lwc1(hs);
            if (health <= dmg) {  // blown up
                sw(hs, 0);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                moby_drop_bolts(m, 0, -1);
                sb(pv + 0x67, 0x78);
                color_pulse_start(m, pulse);
                sw(pv + 0x70, 0);
                gml_3073c8_l13(m);
                particle34_puff_1(m);
                moby_sound_start(0, 0, m);
                sb(m + 0x20, 1);
            } else {  // its damage taken: it wobbles
                swc1(hs, fpu_sub(health, dmg));
                sb(pv + 0x67, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pulse);
                const float two_deg = u2f(0x3d0efa35u);
                swc1(m + 0x40, rand_range_f(-two_deg, two_deg));
                swc1(m + 0x44, rand_range_f(-two_deg, two_deg));
            }
        }
        sb(m + 0xa4, 0xff);
        color_pulse_update(m, pulse);
        return;
    }
    case 1: {  // the blast
        lq_sq(sp, m + 0x10);
        swc1(sp + 8, fpu_add(lwc1(sp + 8), 1.0f));
        const float t = fpu_div(int_to_float(lw(pv + 0x70)), int_to_float(frame_scale_i(lw(L13_ADDR(0x00161e2c)))));
        const float r = lerp_f(lwc1(L13_ADDR(0x00161e30)), lwc1(L13_ADDR(0x00161e34)), t);
        const int32_t n = mobys_in_sphere(sp, r, 0x10, m, 0);
        moby_list_hit(m, sp, coll_hit_list(), n, 0, 0x830001, 2, 1, lwc1(L13_ADDR(0x00161e38)), 1.0f, 1.0f);
        const float half = frame_scale_f(fpu_mul(static_cast<float>(lw(L13_ADDR(0x00161e2c))), 0.5f));
        if (half < static_cast<float>(lw(pv + 0x70))) {
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            particle34_puff_1(m);
        } else {
            sb(m + 0x23, static_cast<uint32_t>(static_cast<int32_t>(lbu(m + 0x23)) * 7 / 8));
        }
        const int32_t frames = lw(pv + 0x70) + 1;
        sw(pv + 0x70, static_cast<uint32_t>(frames));
        if (frame_scale_i(lw(L13_ADDR(0x00161e2c))) < frames) {
            particle34_puff_1(m);
            moby_retire(m);
        }
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
