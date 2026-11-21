// Gemlik Base's (level 13) shots of Qwark's ships in the space fight: class 82, a homing missile, and class 83, a mine
// that drifts toward Ratchet's ship. Level 13 only: the addresses are Gemlik's (`orig: L13:`), the level's own data
// L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp.
//
// Class 82's pvars: +0x00 its velocity (a frame's move), +0x10 where its target was last frame, +0x20 the ship that
// fired it, +0x24 its target (0 once lost: it then flies straight), +0x28 its frames left, +0x2c its speed (it speeds
// up to +0x38 while the target is within 60 degrees of its heading), +0x30 / +0x34 its yaw's / pitch's turning speeds.
// No states: every frame it grows to its class's scale, trails smoke and sparks, steers (aiming ahead of Ratchet's
// ship by the ship's own lead), moves, and explodes on what it meets (not its maker, not the fight's ships).
//
// Class 83's pvars: +0x20 its health, +0x60 its velocity, +0x70 its frames left, +0x74 the ship that dropped it. It
// grows to twice its class's scale, is pulled toward Ratchet's ship within 60 (while drawn), drifts away from its maker
// in its first 9 frames, then slows down; once 8 away from its maker it explodes on a moby or the world it touches, or
// when a hit takes its health.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/devastator.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
// inside the fight's space (2 .. 1021 on each axis)
bool in_space(uint32_t m) {
    for (uint32_t k = 0; k < 0xc; k += 4) {
        const float c = lwc1(m + 0x10 + k);
        if (c < 2.0f || 1021.0f < c) return false;
    }
    return true;
}
}  // namespace

// orig: L13:0x002c1528
void gml_class82_update_l13(uint32_t m) {
    const Frame f(336);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t prev = sp + 0x20, nose = sp + 0x30, pt = sp + 0x40, vel = sp + 0x50, tmp = sp + 0x60;
    lq_sq(prev, m + 0x10);
    {  // its scale eased to its class's
        const float s = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(s, fpu_mul(fpu_sub(lwc1(lwu(m + 0x24) + 0x24), s), 0.1f)));
    }
    {  // a hit by anything but a missile of its own kind: it explodes
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0xa30000), 0);
        if (hit != 0) {
            const uint32_t by = lwu(hit + 0x20);
            if (by != 0 && lh(by + 0xa6) != 0x52) {
                explosion_effect(m, pv, 0, 5, 2, 7, 1, 0, 1, -1, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
                moby_retire(m);
                return;
            }
        }
    }
    // its smoke: a puff behind it drifting at random, and three along its last move with sparks around its axis
    sq_zero(tmp);  // (the vector's w 0)
    swc1(tmp + 0, rand_range_f(-1.0f, 1.0f));
    swc1(tmp + 4, rand_range_f(-1.0f, 1.0f));
    swc1(tmp + 8, rand_range_f(-1.0f, 1.0f));
    lq_sq(vel, tmp);
    {
        const float r = rand_range_f(0.1f, 0.2f);
        vec3_set_length(V(vel), V(vel), fpu_mul(r, dt()));
    }
    {
        const float r = rand_range_f(fpu_mul(dt(), 0.1f), dt());
        vec3_set_length(V(pt), V(pv), -r);
    }
    vec3_add(V(vel), V(vel), V(pt));
    dev_2712b8(m + 0x10, vel, static_cast<uint32_t>(frame_scale_i(6)), 0x7f, 0xb0b0b0, 3, 80000.0f, 1000.0f, 1.0f, u2f(0xb9d1b717u), 0.0f);
    float t = 0.0f;
    do {
        const float t0 = t;
        t = fpu_add(t, u2f(0x3eaaaaabu));
        vec3_set_length(V(pt), V(pv), fpu_mul(rand_range_f(t0, t), lwc1(pv + 0x2c)));
        vec3_add(V(pt), V(pt), V(m + 0x10));
        const uint32_t p = static_cast<uint32_t>(dev_2712b8(pt, vel, static_cast<uint32_t>(frame_scale_i(0x3c)), 0x7f, 0x606060, 3, 80000.0f, 1000.0f, 1.0f, u2f(0xb951b717u), 0.0f));
        if (p != 0) {
            sb(p + 3, 0x44);
            sb(p + 2, lbu(lwu(L13_ADDR(0x001b2380) + 0x5c)));
        }
        sq_zero(tmp);
        swc1(tmp + 8, 0.02f);
        mat3_transform(V(tmp), V(tmp), M(m + 0xc0));
        vec_rotate_axis(V(tmp), V(tmp), V(m + 0xc0), rand_angle());
        spawn_particle_15_v(V(pt), V(tmp), 0x4f007fff, 0x1fffffff, static_cast<uint32_t>(frame_scale_i(5)), 1, 40000.0f);
    } while (t < 1.0f);

    // steering: toward its target while the target stays within 3 of where it was last frame
    auto turn = [&](float yaw, float pitch) {
        const float acc = fpu_mul(dt_sq(), u2f(0x41490fdbu)), vmax = fpu_mul(dt(), u2f(0x41c90fdbu));
        yaw_seek(m + 0x48, pv + 0x30, yaw, acc, acc, vmax);
        const float acc2 = fpu_mul(dt_sq(), u2f(0x41490fdbu)), vmax2 = fpu_mul(dt(), u2f(0x41c90fdbu));
        yaw_seek(m + 0x44, pv + 0x34, pitch, acc2, acc2, vmax2);
    };
    const uint32_t tgt = lwu(pv + 0x24);
    if (tgt != 0 && alive(tgt) && vec3_distance(V(pv + 0x10), V(tgt + 0x10)) < 3.0f) {
        const uint32_t ship = lwu(HERO + 0x15f0);
        float yaw, flat, dz;
        if (ship != 0 && lh(ship + 0xa6) == 0x45 && lbu(lwu(pv + 0x20) + 0x20) != 5) {
            // Ratchet's ship: aimed ahead of it, along its heading, by its speed times the missile's flight time
            const uint32_t spv = lwu(ship + 0x78);
            const float a = angle_add(lwc1(ship + 0x48), lwc1(spv + 0x90));
            const float b = angle_add(lwc1(lwu(HERO + 0x15f0) + 0x44), lwc1(spv + 0x94));
            const float d = vec3_distance(V(m + 0x10), V(lwu(HERO + 0x15f0) + 0x10));
            vec_from_spherical_v(V(pt), fpu_mul(fpu_div(d, fpu_add(lwc1(pv + 0x2c), 24.0f)), lwc1(spv + 0x64)), a, -b);
            vec3_add(V(pt), V(pt), V(lwu(HERO + 0x15f0) + 0x10));
            yaw = atan2_f(fpu_sub(lwc1(pt + 0), lwc1(m + 0x10)), fpu_sub(lwc1(pt + 4), lwc1(m + 0x14)));
            flat = vec2_distance(V(m + 0x10), V(pt));
            dz = fpu_sub(lwc1(pt + 8), lwc1(m + 0x18));
        } else {
            const uint32_t t1 = lwu(pv + 0x24);
            yaw = atan2_f(fpu_sub(lwc1(t1 + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t1 + 0x14), lwc1(m + 0x14)));
            flat = vec2_distance(V(m + 0x10), V(lwu(pv + 0x24) + 0x10));
            dz = fpu_sub(lwc1(lwu(pv + 0x24) + 0x18), lwc1(m + 0x18));
        }
        const float pitch = -atan2_f(flat, dz);
        if (angle_dist(yaw, lwc1(m + 0x48)) < u2f(0x3f860a92u)) {  // within 60 degrees: it speeds up
            const float s = lwc1(pv + 0x2c);
            swc1(pv + 0x2c, fpu_add(s, fpu_mul(fpu_sub(lwc1(pv + 0x38), s), 0.05f)));
        }
        turn(yaw, pitch);
        lq_sq(pv + 0x10, lwu(pv + 0x24) + 0x10);
    } else {
        turn(lwc1(m + 0x48), lwc1(m + 0x44));
        sw(pv + 0x24, 0);
    }

    // its velocity along its heading; in its last two seconds eased (by the square of the time left) from the last one
    const int32_t left = lw(pv + 0x28);
    if (frame_scale_i(0x78) < left) {
        vec_from_spherical_v(V(pv), lwc1(pv + 0x2c), lwc1(m + 0x48), -lwc1(m + 0x44));
    } else {
        float k = fpu_div(int_to_float(left), int_to_float(frame_scale_i(0x78)));
        if (1.0f < k) k = 1.0f;
        else if (k < 0.0f) k = 0.0f;
        k = fpu_mul(k, k);
        vec_from_spherical_v(V(pt), fpu_mul(lwc1(pv + 0x2c), k), lwc1(m + 0x48), -lwc1(m + 0x44));
        vec3_scale(V(pv), V(pv), fpu_sub(1.0f, k));
        vec3_add(V(pv), V(pv), V(pt));
    }
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));

    if (!in_space(m)) {
        moby_retire(m);
        return;
    }
    // what it met on its way (its last move to its nose, then a sphere of 1.5 around it)
    moby_chain_point_v(m, 1, V(nose));
    bool met = segment_collide_v(V(prev), V(nose), 0, m, nullptr) != 0;
    if (!met) met = sphere_collide(m + 0x10, 1.5f, 0, m) != 0;
    if (met) {
        const uint32_t hm = coll_hit_moby();
        if (hm == 0 || (hm != lwu(pv + 0x20) && !shot_passes_through(lh(hm + 0xa6)))) {
            explosion_effect(m, pv, 0, 0x14, 5, 0x10, 0, 1, 1, -1, 0, 5.0f, 10.0f, 7.0f, 4.0f, 1.0f, 1.0f, 15.0f);
            moby_retire(m);
            return;
        }
    }
    // its time out, or no longer drawn while its maker is
    if (tick32(pv + 0x28) != 0) {
        moby_retire(m);
        return;
    }
    if (lbu(m + 0x31) != 0) return;
    const uint32_t owner = lwu(pv + 0x20);
    if (owner == 0 || lbu(owner + 0x31) == 0) return;
    moby_retire(m);
}

// orig: L13:0x002c2048
void gml_class83_update_l13(uint32_t m) {
    const Frame f(224);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t dir = sp + 0x20, rec = sp + 0x30;
    {  // its scale eased to twice its class's
        const float k = fpu_mul(step60(), 0.1f);
        const float s = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(s, fpu_mul(fpu_sub(fpu_add(lwc1(lwu(m + 0x24) + 0x24), lwc1(lwu(m + 0x24) + 0x24)), s), k)));
    }
    if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x52) != 2 && lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 0xa);
    const uint32_t vel = pv + 0x60;
    {  // pulled toward Ratchet's ship within 60 (while drawn)
        const uint32_t ship = lwu(HERO + 0x15f0);
        if (ship != 0) {
            const float d = vec3_distance(V(m + 0x10), V(ship + 0x10));
            if (d < 60.0f && lbu(m + 0x31) != 0) {
                float k = fpu_sub(1.0f, fpu_div(d, 60.0f));
                if (1.0f < k) k = 1.0f;
                k = fpu_mul(k, fpu_mul(dt(), 1.5f));
                vec3_sub(V(dir), V(lwu(HERO + 0x15f0) + 0x10), V(m + 0x10));
                vec3_set_length(V(dir), V(dir), k);
                vec3_add(V(vel), V(vel), V(dir));
            }
        }
    }
    if (lw(pv + 0x70) < frame_scale_i(0x168) - 9) {  // drifting, slowing down
        vec3_add(V(m + 0x10), V(m + 0x10), V(vel));
        vec3_scale(V(vel), V(vel), u2f(0x3f71eb85u));
    } else {  // (its first frames; the original also measures a push away from its maker it does not use)
        vec3_sub(V(dir), V(m + 0x10), V(lwu(pv + 0x74) + 0x10));
        vec3_set_length(V(dir), V(dir), fpu_mul(dt(), 20.0f));
        vec3_add(V(m + 0x10), V(m + 0x10), V(vel));
    }
    if (!in_space(m)) {
        moby_retire(m);
        return;
    }
    const uint32_t owner = lwu(pv + 0x74);
    if (owner != 0 && alive(owner) && 8.0f < vec3_distance(V(m + 0x10), V(owner + 0x10))) {
        // what it touches: mobys within 1.7 of a point 0.75 above it, the world, a hit that takes its health
        sub_00259888(16.0f, rec, static_cast<int32_t>(m), 0x30000, vel);
        lq_sq(dir, m + 0x10);
        swc1(dir + 8, fpu_add(lwc1(dir + 8), 0.75f));
        uint32_t met = static_cast<uint32_t>(mobys_in_sphere(dir, 1.7f, 0x10, m, rec));
        met |= static_cast<uint32_t>(sphere_collide(dir, 1.7f, 4, m));
        const uint32_t hit = sub_0025a420(m, 0xffffffffu, 0);
        if (hit != 0) {
            const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit + 0x2c));
            swc1(pv + 0x20, health);
            if (health <= 0.0f) {
                met |= 0x2000;
                coll_hit_set_moby(0);
            }
        }
        const uint32_t hm = coll_hit_moby();
        if (hm != 0 && shot_passes_through(lh(hm + 0xa6))) met = 0;
        if (met != 0) {
            if (lbu(m + 0x31) != 0) {
                if ((met & 0x2000u) != 0)  // shot down: a small blast
                    explosion_effect(m, vel, 0, 5, 2, 7, 3, 1, 1, -1, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
                else
                    explosion_effect(m, vel, 0, 0x14, 5, 0x10, 0, 1, 1, -1, 0, 4.0f, 16.0f, 7.0f, 4.0f, 1.0f, 1.0f, 15.0f);
            }
            moby_retire(m);
            return;
        }
    }
    if (tick32(pv + 0x70) == 0) return;
    if (lbu(m + 0x31) != 0) return;
    moby_retire(m);
}

}  // namespace rac1
