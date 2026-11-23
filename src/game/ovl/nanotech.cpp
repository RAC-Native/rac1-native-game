// The nanotech (health) orbs a nanotech crate (0x1f5) leaves: class 806, update 0x2d89f0. Eight
// orbits of glow particles (directions 0x1e3ca0, axes 0x1e3d20 shared by all, spun by the first
// one *(0x15f63c)) that fly to the hero and heal him (health 0x1415f8).
#include <cmath>

#include <game/collision/collision.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

// Orb i's position: its direction 0x1e3ca0[i] turned by the cluster's spin +0x2c about its
// axis +0x20 (at its particle's distance +0x28 while the cluster is held, state 2), from the
// centre +0x10 raised by the bob +0.
// orig: 0x002d8850
void nanotech_orb_pos_v(uint32_t m, Vec4& out, int32_t i) {
    const uint32_t pv = lwu(m + 0x78);
    Vec4 d;
    vec_rotate_axis(d, V(OV(0x001e3ca0) + static_cast<uint32_t>(i) * 0x10), V(pv + 0x20), lwc1(pv + 0x2c));
    const uint32_t o = lwu(pv + 0x30 + static_cast<uint32_t>(i) * 4);
    if (o != 0 && lbu(m + 0x20) == 2) vec3_set_length(d, d, lwc1(o + 0x28));
    vec3_add(out, V(pv + 0x10), d);
    out.z = out.z + lwc1(pv);
}

// Spins the eight shared orb directions 0x1e3ca0 about their axes 0x1e3d20 by *(gp-0x5210),
// keeping them *(gp-0x51ec) long.
// orig: 0x002d8908
void nanotech_orbs_spin() {
    for (uint32_t i = 0; i < 8; ++i) {
        Vec4& d = V(OV(0x001e3ca0) + i * 0x10);
        vec_rotate_axis(d, d, V(OV(0x001e3d20) + i * 0x10), lwc1(OV(0x00166c00u - 0x5210)));
        vec3_set_length(d, d, lwc1(OV(0x00166c00u - 0x51ec)));
    }
}

// Whether the hero is in sight of moby m (half a unit up on both, collision flags 0x12).
// orig: 0x002d8980
int32_t nanotech_sees_hero(uint32_t m) {
    Vec4 a = V(m + 0x10);
    a.z = a.z + 0.5f;
    Vec4 b = V(lwu(0x001413d0) + 0x10);
    b.z = b.z + 0.5f;
    return segment_collide_v(a, b, 0x12, 0, nullptr) == 0 ? 1 : 0;
}

// Keeps a nanotech cluster on what is under it (1.5 down): on a crate (class 500 range) it sits
// on the crate's top, on another moby it rides it at the offset +0x70 it had when it landed
// (+0xa set), on the level it rests; above the floor it falls (speed +0x50, 9.8 a second squared,
// at most 0.3 a frame, +0xbc set). The centre +0x10 follows, half a unit up.
// orig: 0x002d81b8
void nanotech_ground(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Vec4 a = V(m + 0x10);
    a.z = a.z + 0.5f;
    Vec4 b = a;
    b.z = b.z - 1.5f;
    const bool hit = segment_collide_v(a, b, 2, m, nullptr) != 0;
    const float g = hit ? coll_hit_point().z : b.z;
    if (hit) {
        const uint32_t h = coll_hit_moby();
        if (h != 0) {
            if (moby_in_range_class_500(h) != 0) {
                swc1(pv + 0x10, lwc1(h + 0x10));
                swc1(m + 0x10, lwc1(h + 0x10));
                swc1(pv + 0x14, lwc1(h + 0x14));
                swc1(m + 0x14, lwc1(h + 0x14));
                const float z = coll_hit_point().z;
                swc1(m + 0x18, z);
                swc1(pv + 0x18, z + 0.5f);
            } else if (lh(pv + 0xa) != 0) {
                const float x = lwc1(h + 0x10) + lwc1(pv + 0x70);
                swc1(pv + 0x10, x);
                swc1(m + 0x10, x);
                const float y = lwc1(h + 0x14) + lwc1(pv + 0x74);
                swc1(pv + 0x14, y);
                swc1(m + 0x14, y);
                const float z = lwc1(h + 0x18) + lwc1(pv + 0x78);
                swc1(m + 0x18, z);
                swc1(pv + 0x18, z + 0.5f);
            } else {
                sh(pv + 0xa, 1);
                swc1(pv + 0x70, lwc1(m + 0x10) - lwc1(h + 0x10));
                swc1(pv + 0x74, lwc1(m + 0x14) - lwc1(h + 0x14));
                swc1(pv + 0x78, lwc1(m + 0x18) - lwc1(h + 0x18));
            }
        }
    }
    const float z = lwc1(m + 0x18);
    if (z <= g) {
        sb(m + 0xbc, 0);
        if (lh(pv + 0xa) != 0) swc1(pv + 0x78, lwc1(pv + 0x78) + (g - z));
        swc1(m + 0x18, g);
        sw(pv + 0x50, 0);
        return;
    }
    sb(m + 0xbc, 1);
    swc1(m + 0x18, z - lwc1(pv + 0x50));
    const float v = lwc1(pv + 0x50) + lwc1(0x0015ed70) * u2f(0x411ccccdu);
    const float step = u2f(0x3e99999au) < v ? u2f(0x3e99999au) : v;
    swc1(pv + 0x50, step);
    if (lh(pv + 0xa) != 0) swc1(pv + 0x78, lwc1(pv + 0x78) - step);
}

// Two glints at a nanotech cluster flying out sideways (a random turn about the camera
// direction of the hero's +0x290 axis scaled *(gp-0x51a0)..*(gp-0x519c) a second): colours
// *(gp-0x5194) / *(gp-0x5190), sizes *(gp-0x518c) / *(gp-0x5188), *(gp-0x5198) frames.
// orig: 0x002d8418
void nanotech_glints(uint32_t m) {
    Vec4 d;
    vec3_sub(d, V(OV(0x00166dc0)), V(m + 0x10));
    const float dt = lwc1(0x0015ed6c);
    const float r = rand_range_f(lwc1(OV(0x00166c00u - 0x51a0)) * dt, lwc1(OV(0x00166c00u - 0x519c)) * dt);
    Vec4 v;
    vec3_scale(v, V(0x0013f5e0), r);
    Vec4 vel;
    vec_rotate_axis(vel, v, d, rand_angle());
    uint32_t life = lbu(OV(0x00166c00u - 0x5198));
    uint32_t fr = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
    spawn_particle_3c_v(V(m + 0x10), vel, lwu(OV(0x00166c00u - 0x5194)), life, fr, 1, lwc1(OV(0x00166c00u - 0x518c)));
    life = lbu(OV(0x00166c00u - 0x5198));
    fr = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
    spawn_particle_3c_v(V(m + 0x10), vel, lwu(OV(0x00166c00u - 0x5190)), life, fr, 1, lwc1(OV(0x00166c00u - 0x5188)));
}

// The trail of a nanotech cluster flying to the hero: four pairs of glows (towards the camera,
// 0.1 apart) for the timers +0x58.. still running, faded by their remaining time, their frames
// +0x60.. cycling by *(gp-0x5170)..; 1 when a timer is still beyond 48 frames.
// orig: 0x002d8510
int32_t nanotech_trail(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t busy = 0;
    Vec4 at;
    vec3_sub(at, V(OV(0x00166dc0)), V(m + 0x10));
    vec3_set_length(at, at, u2f(0xbe99999au));
    Vec4 step;
    vec3_set_length(step, at, u2f(0x3dcccccdu));
    vec3_add(at, at, V(m + 0x10));
    const float z0 = at.z;
    for (uint32_t k = 0; k < 4; ++k) {
        int16_t t = static_cast<int16_t>(lh(pv + 0x58 + k * 2));
        const int32_t done = timer_tick_i16(t);
        sh(pv + 0x58 + k * 2, static_cast<uint16_t>(t));
        if (done != 0) continue;
        const int32_t left = lh(pv + 0x58 + k * 2);
        if (frame_scale_i(0x30) < left) busy = 1;
        const uint32_t frp = pv + 0x60 + k * 4;
        float f = lwc1(frp) + lwc1(OV(0x00166c00u - 0x5170) + k * 4);
        if (255.0f <= f) f = f - 255.0f;
        else if (f <= 0.0f) f = f + 255.0f;
        swc1(frp, f);
        float u = int_to_float(frame_scale_i(0xff) - lh(pv + 0x58 + k * 2));
        u = fpu_div(u, static_cast<float>(frame_scale_i(0xff)));
        uint32_t color = rgba_lerp(lwu(OV(0x00166c00u - 0x5184)), lwu(OV(0x00166c00u - 0x5180)), abs_f(0.5f - u));
        const int32_t alpha = static_cast<int32_t>(color) >> 24;
        const float n = int_to_float(frame_scale_i(lw(OV(0x00166c00u - 0x51a8))));
        const float fade = 1.0f - fpu_div(static_cast<float>(lh(pv + 0x58 + k * 2)), n);
        const uint32_t a = static_cast<uint32_t>(float_to_int(int_to_float(alpha) * (1.0f - fade))) << 24;
        color = a | (color & 0x00ffffffu);
        at.z = z0 + lwc1(OV(0x00166c00u - 0x5150) + k * 4);
        const uint32_t sz = OV(0x00166c00u - 0x5160) + k * 4;
        uint32_t p = spawn_particle_3b_v(at, color, static_cast<uint32_t>(float_to_int(lwc1(frp))) & 0xffu, lwu(OV(0x00166c00u - 0x5178)), 0, 2, 1, lwc1(sz) * fade);
        if (p != 0) sb(p + 2, lbu(lwu(OV(0x001b2080) + 0xf8)));
        p = spawn_particle_3b_v(at, a | 0x00ffffffu, static_cast<uint32_t>(float_to_int(lwc1(frp))) & 0xffu, lwu(OV(0x00166c00u - 0x5178)), lwu(OV(0x00166c00u - 0x517c)), 2, 1,
                                lwc1(sz) * (fade * u2f(0x3f333333u)));
        if (p != 0) sb(p + 2, lbu(lwu(OV(0x001b2080) + 0xf8)));
        vec3_add(at, at, step);
    }
    return busy;
}

namespace {
// the hero's health cap: 4 in the special mode P+0x20a4 1, else *(0x15eda0)
// orig: (helper) repeated in 0x002d89f0
int32_t hero_health_max() { return lbu(0x0013f350 + 0x20a4) == 1 ? 4 : lw(0x0015eda0); }

// where the orbs fly: the hero less *(gp-0x51ac) along his +0x290 axis
// orig: (helper) repeated in 0x002d89f0
Vec4 nanotech_target() {
    Vec4 t;
    vec3_scale(t, V(0x0013f5e0), -lwc1(OV(0x00166c00u - 0x51ac)));
    vec3_add(t, V(0x0013f3d0), t);
    return t;
}
}  // namespace

// The nanotech orbs (class 806, pvars: bob +0, its phase +4, the crate +0xc, centre +0x10,
// spin axis +0x20 / angle +0x2c, the eight orb particles +0x30.., fall +0x50, flight timer +0x54,
// healed +0x56, trail timers +0x58 / frames +0x60; the first cluster *(0x15f63c) counts the
// clusters flying +0x7c and spins the shared orbits). 0 set up: on the crate, the shared orbits
// from the first one's rows, a random spin. 1 rides the crate until it breaks (2), 2 rests on
// the ground; both bob, and with the hero hurt (or the special mode's refill due) within 10 and
// in sight they take off (3): each orb gets a curve to the hero. Drawn orbs sparkle (kind 0x3e).
// 3 the orbs fly the curves leaving sparkles; each arriving heals once (+1 up to the cap, sound
// 1); when the flight ends the orbs are freed and the cluster removed (the first one waits in 4).
// orig: 0x002d89f0
void nanotech_update(uint32_t m) {
    constexpr uint32_t P = 0x0013f350;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    const float pi = u2f(0x40490fdbu), half_pi = u2f(0x3fc90fdbu);
    auto master = [] { return lwu(OV(0x0015f63c)); };
    if (st >= 5) return;
    if (st == 4) {
        if (master() == m) nanotech_orbs_spin();
        return;
    }
    if (st == 3) {
        if (master() == m) nanotech_orbs_spin();
        if (lh(pv + 0x56) != 0) {
            vec3_scale(V(m + 0x10), V(0x0013f5e0), -lwc1(OV(0x00166c00u - 0x51ac)));
            vec3_add(V(m + 0x10), V(0x0013f3d0), V(m + 0x10));
            swc1(m + 0x4c, u2f(0x3f060a92u));
            if (nanotech_trail(m) != 0 && rand_mod(5) == 0) nanotech_glints(m);
        }
        if (lw(P + 0x22a8) == hero_health_max() && lw(P + 0x1c0) != 0 && lbu(0x0015edb6) != 0) {
            const int32_t t = frame_scale_i(lw(OV(0x00166c00u - 0x51c0)));
            if (lh(pv + 0x54) < t) {
                sh(pv + 0x54, static_cast<uint32_t>(t));
                for (uint32_t k = 0; k < 4; ++k) sh(pv + 0x58 + k * 2, lhu(pv + 0x58 + k * 2) + 1u);
            }
        }
        int16_t ft = static_cast<int16_t>(lh(pv + 0x54));
        const int32_t over = timer_tick_i16(ft);
        sh(pv + 0x54, static_cast<uint16_t>(ft));
        if (over != 0) {
            for (uint32_t i = 0; i < 8; ++i) {
                const uint32_t o = lwu(pv + 0x30 + i * 4);
                if (o == 0) continue;
                free_particle(o);
                sw(pv + 0x30 + i * 4, 0);
            }
            if (master() == m) {
                sb(m + 0x20, 4);
            } else {
                moby_retire(m);
            }
            return;
        }
        const Vec4 tgt = nanotech_target();
        const float jit = u2f(0x3c23d70au);
        auto sparkle = [&](const Vec4& at) {
            Vec4 q = at;
            q.x = rand_range_signed_f(0.0f, jit) + at.x;
            q.y = rand_range_signed_f(0.0f, jit) + at.y;
            q.z = rand_range_signed_f(0.0f, jit) + at.z;
            const uint32_t p = spawn_particle_3e_v(q, frame_scale_i(0x14), 0);
            if (p != 0) {
                sw(p + 4, lwu(OV(0x00166c00u - 0x5200)));
                swc1(p + 0xc, lwc1(OV(0x00166c00u - 0x5208)));
            }
        };
        for (uint32_t i = 0; i < 8; ++i) {
            const uint32_t slot = pv + 0x30 + i * 4;
            const uint32_t o = lwu(slot);
            if (o == 0) continue;
            int16_t t = static_cast<int16_t>(lh(o + 0x3e));
            const int32_t arrived = timer_tick_i16(t);
            sh(o + 0x3e, static_cast<uint16_t>(t));
            if (arrived != 0) {
                if (lh(pv + 0x56) == 0) {
                    const int32_t cap = hero_health_max();
                    const int32_t h = lw(P + 0x22a8);
                    if (h != 0) sw(P + 0x22a8, static_cast<uint32_t>(cap < h + 1 ? cap : h + 1));
                    const uint32_t mp = lwu(master() + 0x78);
                    const int32_t c = lw(mp + 0x7c) - 1;
                    sw(mp + 0x7c, static_cast<uint32_t>(c));
                    if (c < 0) sw(mp + 0x7c, 0);
                    moby_sound_play(1, 0, m, 0x1f5);
                    sh(pv + 0x56, 1);
                }
                free_particle(o);
                sw(slot, 0);
                continue;
            }
            Vec4 mid = V(o + 0x10);
            const float n = int_to_float(lh(o + 0x3c));
            const float u = 1.0f - fpu_div(static_cast<float>(lh(o + 0x3e)), n);
            Vec4 a, d, b;  // the curve: start a = +0x30 (b - +0x24 / d - +0x24 its end tangents)
            a = Vec4{lwc1(o + 0x30), lwc1(o + 0x34), lwc1(o + 0x38), 0.0f};
            const Vec4 c24 = Vec4{lwc1(o + 0x24), lwc1(o + 0x28), lwc1(o + 0x2c), 0.0f};
            b = Vec4{a.x - c24.x, a.y - c24.y, a.z - c24.z, 0.0f};
            d = Vec4{tgt.x - c24.x, tgt.y - c24.y, tgt.z - c24.z, 0.0f};
            swc1(o + 0x10, cubic_interp(b.x, a.x, tgt.x, d.x, u));
            swc1(o + 0x14, cubic_interp(b.y, a.y, tgt.y, d.y, u));
            swc1(o + 0x18, cubic_interp(b.z, a.z, tgt.z, d.z, u));
            sparkle(V(o + 0x10));
            vec3_add(mid, V(o + 0x10), mid);
            vec3_scale(mid, mid, 0.5f);
            sparkle(mid);
        }
        return;
    }
    if (st == 0) {
        sb(m + 0x20, 1);
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        sb(m + 0xbc, 0);
        const uint32_t c = lwu(pv + 0xc);
        lq_sq(m + 0x10, c + 0x10);
        lq_sq(m + 0x40, c + 0x40);
        lq_sq(pv + 0x10, m + 0x10);
        sw(pv, 0);
        sh(pv + 8, 0);
        swc1(pv + 0x18, lwc1(pv + 0x18) + 0.5f);
        swc1(pv + 4, rand_range_f(lwc1(pv), pi));
        sh(pv + 0x56, 0);
        sh(pv + 0xa, 0);
        for (uint32_t i = 0; i < 4; ++i) swc1(pv + 0x60 + i * 4, static_cast<float>(rand_mod(0xff)));
        for (uint32_t i = 0; i < 4; ++i) sh(pv + 0x58 + i * 2, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00166c00u - 0x51a8)))));
        for (uint32_t i = 0; i < 8; ++i) sw(pv + 0x30 + i * 4, 0);
        if (master() == 0) {
            euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
            sw(OV(0x0015f63c), m);
            sb(m + 0x30, 0xff);
            sw(pv + 0x7c, 0);
            const Vec4& r0 = V(m + 0xc0);
            const Vec4& r1 = V(m + 0xd0);
            const Vec4& r2 = V(m + 0xe0);
            const float len = lwc1(OV(0x00166c00u - 0x51ec));
            auto dir = [](uint32_t i) -> Vec4& { return V(OV(0x001e3ca0) + i * 0x10); };
            auto axis = [](uint32_t i) -> Vec4& { return V(OV(0x001e3d20) + i * 0x10); };
            vec3_set_length(dir(0), r0, len);
            axis(0) = r2;
            vec3_set_length(dir(1), r1, len);
            axis(1) = r0;
            vec3_set_length(dir(2), r2, len);
            axis(2) = r1;
            vec3_add(dir(3), r0, r2);
            vec3_set_length(dir(3), dir(3), len);
            vec3_cross_ba(axis(3), dir(3), r1);
            vec3_add(dir(4), r0, r1);
            vec3_set_length(dir(4), dir(4), len);
            vec3_cross_ba(axis(4), dir(4), r2);
            vec3_sub(dir(5), r1, r2);
            vec3_set_length(dir(5), dir(5), len);
            vec3_cross_ba(axis(5), dir(5), r0);
            vec3_sub(dir(6), r0, r2);
            vec3_set_length(dir(6), dir(6), len);
            vec3_cross_ba(axis(6), dir(6), r1);
            vec3_sub(dir(7), r0, r1);
            vec3_set_length(dir(7), dir(7), len);
            vec3_cross_ba(axis(7), dir(7), r2);
        }
        const float a = rand_range_f(0.0f, u2f(0x40c90fdbu));
        const float b = rand_range_f(u2f(0xbfc90fdbu), half_pi);
        vec_from_spherical_v(V(pv + 0x20), 1.0f, a, b);
        swc1(pv + 0x2c, rand_range_signed_f(u2f(0x3f490fd8u), half_pi));
    }
    if (st == 0 || st == 1) {
        const uint32_t c = lwu(pv + 0xc);
        lq_sq(m + 0x10, c + 0x10);
        lq_sq(pv + 0x10, c + 0x10);
        swc1(pv + 0x18, lwc1(pv + 0x18) + 0.5f);
        if (static_cast<int8_t>(lbu(c + 0x20)) < 0 || c == m) {
            sb(m + 0x20, 2);
            sw(pv + 0x50, 0);
        }
    }
    if (lbu(m + 0x20) == 2) {
        lq_sq(pv + 0x10, m + 0x10);
        swc1(pv + 0x18, lwc1(pv + 0x18) + 0.5f);
        nanotech_ground(m);
        for (uint32_t i = 0; i < 8; ++i) {
            const uint32_t o = lwu(pv + 0x30 + i * 4);
            if (o == 0) continue;
            const uint32_t q = o + 0x20;
            const float f = sin_vu(lwc1(q + 0xc));
            swc1(q + 8, lwc1(OV(0x00166c00u - 0x51a4)) * f + lwc1(OV(0x00166c00u - 0x51ec)));
            swc1(q + 0xc, angle_add(lwc1(q + 0xc), lwc1(OV(0x001e14a0) + i * 4)));
        }
    }
    const int32_t cap = hero_health_max();
    const uint32_t ms = master();
    if (lbu(m + 0x20) != 1 && ms != 0 && lwu(ms + 0x78) != 0 && lw(P + 0x22a8) != 0) {
        const uint32_t mp = lwu(ms + 0x78);
        const bool wants = lw(P + 0x22a8) + lw(mp + 0x7c) < cap || (lbu(0x0015edb6) != 0 && lw(P + 0x1c0) == 0);
        if (wants && vec3_distance(V(m + 0x10), V(P + 0x80)) < 10.0f && nanotech_sees_hero(m) != 0) {
            sb(m + 0x20, 3);
            sw(mp + 0x7c, lwu(mp + 0x7c) + 1u);
            if (lbu(0x0015edb6) != 0) sw(P + 0x1c0, static_cast<uint32_t>(frame_scale_i(600)));
            sh(pv + 0x54, static_cast<uint32_t>(frame_scale_i(lw(OV(0x00166c00u - 0x51c4)))));
            Vec4 t;
            vec3_scale(t, V(P + 0x290), -lwc1(OV(0x00166c00u - 0x51ac)));
            vec3_add(t, V(P + 0x80), t);
            Vec4 side;
            vec3_scale(side, V(P + 0x290), -lwc1(OV(0x00166c00u - 0x51b0)));
            Vec4 d;
            vec3_sub(d, t, V(m + 0x10));
            d.z = d.z - 0.5f;
            Vec4 c;
            vec3_cross_ba(c, d, side);
            vec3_cross_ba(side, c, d);
            vec_rotate_axis(side, side, d, u2f(0xbfc90fdbu));
            vec3_set_length(side, side, lwc1(OV(0x00166c00u - 0x51b0)));
            Vec4 half;
            vec3_scale(half, d, 0.5f);
            for (uint32_t i = 0; i < 8; ++i) {
                const uint32_t o = lwu(pv + 0x30 + i * 4);
                if (o == 0) continue;
                Vec4 off;
                vec_rotate_axis(off, side, d, static_cast<float>(static_cast<int32_t>(i)) * u2f(0x3ee5a84cu));
                Vec4 cp;
                vec3_add(cp, half, off);
                swc1(o + 0x24, cp.x);
                swc1(o + 0x28, cp.y);
                swc1(o + 0x2c, cp.z);
                const uint32_t q = lwu(pv + 0x30 + i * 4);
                swc1(o + 0x30, lwc1(q + 0x10));
                swc1(o + 0x34, lwc1(q + 0x14));
                swc1(o + 0x38, lwc1(q + 0x18));
                const int32_t life = frame_scale_i(rand_range(lw(OV(0x00166c00u - 0x51bc)), lw(OV(0x00166c00u - 0x51b8))));
                sh(o + 0x3e, static_cast<uint32_t>(life));
                sh(o + 0x3c, static_cast<uint32_t>(life));
            }
        }
    }
    swc1(pv, lwc1(OV(0x00166c00u - 0x51f0)) * sin_vu(lwc1(pv + 4)));
    swc1(pv + 4, angle_add(lwc1(pv + 4), lwc1(OV(0x00166c00u - 0x51f4))));
    if (master() == m) nanotech_orbs_spin();
    Vec4 at = V(m + 0x10);
    at.z = at.z + 0.5f;
    at.w = 1.0f;
    if (point_on_screen_v(at, 64.0f) < 0) return;
    sub_001f93e0(OVF(0x002d9810), m);
    const int32_t life = frame_scale_i(10.0f < vec2_distance(V(m + 0x10), V(OV(0x00166dc0))) ? 5 : 0x28);
    for (uint32_t i = 0; i < 8; ++i) {
        Vec4 pos;
        nanotech_orb_pos_v(m, pos, static_cast<int32_t>(i));
        const uint32_t p = spawn_particle_3e_v(pos, life, 2);
        if (p != 0) {
            const uint32_t q = p + 0x20;
            swc1(q + 0x10, lwc1(p + 0x10) - lwc1(m + 0x10));
            swc1(q + 0x14, lwc1(p + 0x14) - lwc1(m + 0x14));
            swc1(q + 0x18, lwc1(p + 0x18) - lwc1(m + 0x18));
            sw(q + 4, lbu(m + 0x20) == 2 ? m : lwu(pv + 0xc));
            sw(p + 4, lwu(OV(0x00166c00u - 0x5200)));
            swc1(p + 0xc, lwc1(OV(0x00166c00u - 0x5208)));
        }
        const uint32_t slot = pv + 0x30 + i * 4;
        const uint32_t o = lwu(slot);
        if (o == 0) {
            const uint32_t n = spawn_particle_3e_v(pos, 0, 1);
            sw(slot, n);
            if (n != 0) {
                sw(n + 0x2c, 0);
                swc1(n + 0x28, lwc1(OV(0x00166c00u - 0x51ec)));
                swc1(lwu(slot) + 0xc, lwc1(OV(0x00166c00u - 0x5204)));
                sw(lwu(slot) + 4, lwu(OV(0x00166c00u - 0x5200)));
            }
        } else {
            V(o + 0x10) = pos;
            sb(o + 8, lbu(o + 8) + lbu(OV(0x00166c00u - 0x520c)));
        }
    }
}

}  // namespace rac1
