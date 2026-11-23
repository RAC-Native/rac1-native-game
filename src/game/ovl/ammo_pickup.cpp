// The ammo pickups: the update 0x2c53c0 shared by the item pickup classes (the ammo a
// crate or an enemy drops: pickup_spawn 0x2c52a8 sets its amount +0, item +8, timer +0x20 and start
// height +0x28) and the moby bounding-sphere gap 0x250430 it measures the hero with. Before this
// port the update was a TODO_PORT stub: the pickups hung where they were spawned and only the
// wrench (wrench_pull_pickups) could take them.
#include <cmath>

#include <game/collision/collision.h>
#include <game/ovl/ammo_pickup.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// How far apart the bounding spheres (+0: centre xyz, radius w) of mobys a and b are: the centres'
// distance less both radii, in 1/1024 (VU0: (x^2 + y^2) + 1 z^2, its square root by Q, both terms
// scaled by 1/1024 before the subtraction). Negative when they overlap.
// orig: 0x00250430
float moby_sphere_gap(uint32_t a, uint32_t b) {
    const Vec4& p = V(a);
    const Vec4& q = V(b);
    const float x = p.x - q.x, y = p.y - q.y, z = p.z - q.z;
    const float d2 = (x * x + y * y) + 1.0f * (z * z);
    const float d = std::sqrt(std::fabs(d2));
    const float k = u2f(0x3a800000u);
    const float r = k * (p.w + q.w);
    return k * d - r;
}

// The pickup's update (pvars: amount +0, item +8, its timer +0x20 (16 frames from the spawn),
// velocity +0x10, the rise +0x24, start height +0x28; moby: state +0x20, brightness +0x23, scale
// +0x2c). Its timer ticks. While falling, resting or growing (states 1, 3, 4) it moves: when it has
// a velocity a sphere probe 0.4 around half a unit above it lifts it out of what it is in, gravity
// (half before the step, half after: 4.9 * the frame scale each) and the step, stopped (velocity
// cleared) at what the segment from 0.1 above it hits (flags 2). By state:
//   0: set up, state 1;
//   1: brightens to 128 (in 30 frames); once its timer is out and it lies still, it is taken when
//      the hero (0x13f350: position +0x80, the z reach +0x228c, the xy reach +0x2288, health
//      +0x22a8) is near enough and its weapon short of ammo (or it fell 2 below where it started):
//      the ammo added (ammo_add), the HUD message, the gain counted at 0x13de08, it grows a little
//      and flies to the hero (state 2), the pickup sound 0xd5 at most every 10 frames (0x161838).
//      Not taken: it brightens by one; touching the hero (sphere gap <= -0.2): state 3, +6 a
//      60-frame timer;
//   2: rises (+0x24 down to 0 by 24 a frame), flies to the hero's 0x13f420 at a speed going to
//      64 a second, shrinking as it gets near; gone when it shrank away or arrives;
//   3: pulses (brightness 10..20, a 60-frame cos); back to 1 when the hero stops touching it or
//      its weapon is short of ammo again;
//   4: grows to the class's scale (3% a frame), then state 1 with the class's +0x10 at +0x94.
// orig: 0x002c53c0
void ammo_pickup_update(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    {
        int32_t t = lw(p + 0x20);
        timer_tick_i32(t);
        sw(p + 0x20, static_cast<uint32_t>(t));
    }
    uint32_t st = lbu(m + 0x20);
    if (st == 1 || st == 4 || st == 3) {
        Vec4& vel = V(p + 0x10);
        if (vec3_length(vel) != 0.0f) {
            Vec4 probe = V(m + 0x10);
            probe.z = probe.z + 0.5f;
            if (sphere_collide_v(probe, u2f(0x3ecccccdu), 2, 0) != 0) {
                lq_sq(m + 0x10, coll_hit_pushed_addr());
                swc1(m + 0x18, lwc1(m + 0x18) - 0.5f);
            }
        }
        const float g = lwc1(0x0015ed70);
        const float half = u2f(0x409ccccdu);
        swc1(p + 0x18, lwc1(p + 0x18) - g * half);
        Vec4 to;
        vec3_add(to, V(m + 0x10), vel);
        swc1(p + 0x18, lwc1(p + 0x18) - lwc1(0x0015ed70) * half);
        Vec4 from = V(m + 0x10);
        from.z = from.z + u2f(0x3dcccccdu);
        if (segment_collide_v(from, to, 2, 0, nullptr) != 0) {
            vec_clear(vel);
            lq_sq(m + 0x10, coll_hit_point_addr());
        } else {
            V(m + 0x10) = to;
        }
        st = lbu(m + 0x20);
    }
    switch (st) {
    case 0:
        sb(m + 0x20, 1);
        return;
    case 1: {
        float b = static_cast<float>(static_cast<int32_t>(lbu(m + 0x23)));
        approach_f_v(b, 128.0f, static_cast<float>(0x80 / frame_scale_i(0x1e)));
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(b)));
        if (0 < lw(p + 0x20)) return;
        if (vec3_length(V(p + 0x10)) != 0.0f) return;
        const uint32_t P = 0x0013f350;
        bool take = true;
        if (lwc1(P + 0x228c) < abs_f(lwc1(m + 0x18) - lwc1(P + 0x88))) {
            take = false;
        } else if (lwc1(P + 0x2288) < vec2_distance(V(m + 0x10), V(P + 0x80))) {
            take = false;
        }
        const uint32_t w = lwu(p + 8);
        const int32_t before = lw(0x0013d428 + w * 4);
        if (!(before < static_cast<int32_t>(lhu(OV(0x001c40b0) + w * 0x18 + 0xe)))) take = false;
        if (lwc1(m + 0x18) < lwc1(p + 0x28) - 2.0f) take = true;
        if (lw(P + 0x22a8) == 0) take = false;
        if (!take) {
            const uint32_t br = lbu(m + 0x23);
            if (br < 0xff) sb(m + 0x23, br + 1);
            if (moby_sphere_gap(m, lwu(P + 0x2080)) <= u2f(0xbe4ccccdu)) {
                sb(m + 0x20, 3);
                sh(p + 6, static_cast<uint32_t>(frame_scale_i(0x3c)));
            }
            return;
        }
        ammo_add(w, lw(p));
        const uint32_t w1 = lwu(p + 8);
        const int32_t now = lw(0x0013d428 + w1 * 4);
        if (before < now) {
            const int32_t got = now - before;
            const uint32_t rec = OV(0x00179ac0) + w1 * 0x4c;
            hud_message_show(got == 1 ? lh(rec + 0x36) : lh(rec + 0x34), static_cast<uint32_t>(got), -1);
        }
        const uint32_t w2 = lwu(p + 8);
        const uint32_t gain = 0x0013de08 + w2 * 4;
        sw(gain, static_cast<uint32_t>(lw(gain) + (lw(0x0013d428 + w2 * 4) - before)));
        sb(m + 0x20, 2);
        swc1(m + 0x2c, lwc1(m + 0x2c) * (lwc1(0x0015ed60) * u2f(0x3e4cccd0u) + 1.0f));
        vec_clear(V(p + 0x10));
        swc1(p + 0x24, lwc1(0x0015ed6c) * 8.0f);
        const int32_t since = abs_i(lw(OV(0x0015f5cc)) - lw(OV(0x00161838)));
        if (frame_scale_i(10) < since) {
            moby_sound_play(0, 0, m, 0xd5);
            sw(OV(0x00161838), lwu(OV(0x0015f5cc)));
        }
        return;
    }
    case 2: {
        const float k = 24.0f;
        approach_f(p + 0x24, 0.0f, lwc1(0x0015ed70) * k);
        swc1(m + 0x18, lwc1(m + 0x18) + lwc1(p + 0x24));
        float speed = vec3_length(V(p + 0x10));
        approach_f_v(speed, lwc1(0x0015ed6c) * 64.0f, lwc1(0x0015ed70) * k);
        const uint32_t hero = 0x0013f420;
        vec3_sub(V(p + 0x10), V(hero), V(m + 0x10));
        if (lwc1(m + 0x2c) == 0.0f || vec3_length(V(p + 0x10)) < speed) {
            moby_retire(m);
            return;
        }
        vec3_set_length(V(p + 0x10), V(p + 0x10), speed);
        vec3_add(V(m + 0x10), V(m + 0x10), V(p + 0x10));
        float near = vec3_distance(V(m + 0x10), V(hero));
        const float span = static_cast<float>(frame_scale_i(0x1e)) * speed + u2f(0x3c23d70au);
        near = fpu_div(near, span);
        if (1.0f < near) {
            near = 1.0f;
        } else if (near < u2f(0x3e99999au)) {
            near = u2f(0x3e99999au);
        }
        const float full = lwc1(lwu(m + 0x24) + 0x24);
        approach_f(m + 0x2c, full * near, full * u2f(0x3dcccccdu));
        return;
    }
    case 3: {
        const int32_t now = lw(OV(0x0015f5cc));
        float b = static_cast<float>(static_cast<int32_t>(lbu(m + 0x23)));
        float a = fpu_div(static_cast<float>(now % 0x3c), 60.0f);
        a = a * u2f(0x40c90fd0u);
        const float c = cos_vu(a - u2f(0x40490fd0u));
        const float target = static_cast<float>(float_to_int(c * 5.0f + 5.0f) + 10);
        approach_f_v(b, target, static_cast<float>(0x80 / frame_scale_i(0x1e)));
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(b)));
        if (!(0.0f <= moby_sphere_gap(m, lwu(0x001413d0)))) {
            const uint32_t w = lwu(p + 8);
            if (!(lw(0x0013d428 + w * 4) < static_cast<int32_t>(lhu(OV(0x001c40b0) + w * 0x18 + 0xe)))) return;
        }
        sb(m + 0x20, 1);
        return;
    }
    case 4: {
        const uint32_t cls = lwu(m + 0x24);
        float s = lwc1(m + 0x2c);
        s = s + (lwc1(cls + 0x24) - s) * (lwc1(0x0015ed60) * u2f(0x3cf5c28fu));
        swc1(m + 0x2c, s);
        const float full = lwc1(cls + 0x24);
        if (fpu_div(full - s, full) < u2f(0x3c23d70au)) {
            swc1(m + 0x2c, full);
            sb(m + 0x20, 1);
            sw(m + 0x94, lwu(cls + 0x10));
        }
        return;
    }
    default:
        return;
    }
}

// orig: (none) — the update for the classes' indirect call.
void ammo_pickup_fns_register() { ovl_register_moby_fn(0x002c53c0, ammo_pickup_update); }

}  // namespace rac1
