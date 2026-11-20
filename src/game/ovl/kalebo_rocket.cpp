// Kalebo III's (level 16) class 1475: a homing rocket. It speeds up to 30 a second, turns after its first 7 frames toward
// its target (pvar +0x8: 0.7 above its place), trails smoke (two puffs a frame of the level's trail particle, the first one
// set to the level's smoke texture L16 0x1b255c) and sparks (particle type 0x15), and blows up on what it meets (its hit
// record: kinds 0x830000, damage 5627.92) other than Ratchet and its owner (+0x0). It is retired when its time (+0x4) runs
// out, 60 or more from Ratchet, or below 2 on any axis. Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare.
// Registered in kalebo_mobys.cpp; checked by batch 250 (class1475, rocket_*).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/devastator.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// Class 1475: state 0 flies, state 1 blows up.
// orig: L16:0x002a3f38
void kal_class1475_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 1) {
        explosion_effect(m, L16_ADDR(0x0015f580), 0, 3, 3, 5, 0, 0, 0, -1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 4.0f, u2f(0x3f333333u), 7.0f);
        moby_retire(m);
        return;
    }
    if (state != 0) return;

    const kalebo::Frame f(320);
    const uint32_t dir = f.sp + 0x20, at = f.sp + 0x30, drift = f.sp + 0x40, tmp = f.sp + 0x50, rec = f.sp + 0x60;
    const uint32_t pos = m + 0x10;
    sh(pv + 6, lhu(pv + 6) + 1u);
    approach_f(pv + 0x10, fpu_mul(lwc1(0x0015ed6c), 30.0f), fpu_mul(lwc1(0x0015ed70), 70.0f));
    const uint32_t target = lwu(pv + 8);
    if (target != 0 && frame_scale_i(7) < lh(pv + 6)) {  // the turn toward the target
        V(dir) = V(target + 0x10);
        swc1(dir + 8, fpu_add(lwc1(dir + 8), u2f(0x3f333333u)));
        const float yaw = atan2_f(fpu_sub(lwc1(dir), lwc1(m + 0x10)), fpu_sub(lwc1(dir + 4), lwc1(m + 0x14)));
        const float pitch = atan2_f(vec2_distance(V(pos), V(dir)), fpu_sub(lwc1(dir + 8), lwc1(m + 0x18)));
        const float k = u2f(0x3cf5c28fu), damp = u2f(0x3e99999au), rate = u2f(0x411996c7u);  // 0.03, 0.3, 9.5993
        spring_angle(m + 0x48, pv + 0x14, 0, yaw, k, damp, fpu_mul(lwc1(0x0015ed6c), rate));
        spring_angle(m + 0x44, pv + 0x18, 0, -pitch, k, damp, fpu_mul(lwc1(0x0015ed6c), rate));
    }
    vec_from_spherical_v(V(dir), lwc1(pv + 0x10), lwc1(m + 0x48), -lwc1(m + 0x44));

    // the smoke: a puff at a random place along the move, drifting a little back and to a random side
    V(tmp) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    swc1(tmp, rand_range_f(-1.0f, 1.0f));
    swc1(tmp + 4, rand_range_f(-1.0f, 1.0f));
    swc1(tmp + 8, rand_range_f(-1.0f, 1.0f));
    V(drift) = V(tmp);
    vec3_set_length(V(drift), V(drift), fpu_mul(rand_range_f(u2f(0x3dcccccdu), u2f(0x3e4ccccdu)), lwc1(0x0015ed6c)));
    const float dt = lwc1(0x0015ed6c);
    vec3_set_length(V(at), V(dir), -rand_range_f(fpu_mul(dt, u2f(0x3dcccccdu)), dt));
    vec3_add(V(drift), V(drift), V(at));
    vec3_set_length(V(at), V(dir), fpu_mul(rand_range_f(0.0f, 1.0f), lwc1(pv + 0x10)));
    vec3_add(V(at), V(at), V(pos));
    const int32_t puff = dev_2712b8(at, drift, static_cast<uint32_t>(frame_scale_i(0x3c)), 0x7f, 0x606060, 3, 40000.0f, 1000.0f, 1.0f,
                                    u2f(0xb951b717u), 0.0f);
    if (puff != 0) {
        sb(static_cast<uint32_t>(puff) + 3, 0x44);
        sb(static_cast<uint32_t>(puff) + 2, lbu(lwu(L16_ADDR(0x001b255c))));
    }
    dev_2712b8(pos, drift, static_cast<uint32_t>(frame_scale_i(6)), 0x7f, 0xb0b0b0, 3, 40000.0f, 1000.0f, 1.0f, u2f(0xb9d1b717u), 0.0f);

    // the spark: 0.02 up in the rocket's frame, turned about its axis at random
    V(tmp) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    swc1(tmp + 8, u2f(0x3ca3d70au));
    mat3_transform(V(tmp), V(tmp), M(m + 0xc0));
    vec_rotate_axis(V(tmp), V(tmp), V(m + 0xc0), rand_angle());
    spawn_particle_15_v(V(at), V(tmp), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(frame_scale_i(5)), 1, 20000.0f);

    // the move and its test
    V(at) = V(pos);
    vec3_add(V(pos), V(pos), V(dir));
    if (lwc1(m + 0x10) < 2.0f || lwc1(m + 0x14) < 2.0f || lwc1(m + 0x18) < 2.0f) {
        moby_retire(m);
        return;
    }
    sw(rec + 0x14, NOT_ADDR(0x830000u));
    sw(rec + 0x10, m);
    swc1(rec + 0x1c, 1.0f);
    sw(rec + 0x20, 1);
    V(rec) = V(dir);
    vec2_set_length(V(rec), V(rec), 1.0f);
    swc1(rec + 8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));  // the damage, 5627.92
    sb(rec + 0x18, 3);
    sh(rec + 0x1a, lhu(m + 0xa6));
    sb(rec + 0x19, 1);
    if (segment_collide_v(V(at), V(pos), 0, lwu(0x001413d0), static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0) {
        const uint32_t met = coll_hit_moby();
        if (met == 0) {
            if (static_cast<int32_t>(coll_hit_flags()) <= (0)) return;
        } else if (met == lwu(0x001413d0) || met == lwu(pv)) {
            return;
        }
        V(pos) = coll_hit_point();
        sb(m + 0x20, 1);
        return;
    }
    if (kalebo::tick16(pv + 4) != 0 || 60.0f < vec2_distance(V(pos), V(0x0013f3d0))) moby_retire(m);
}

}  // namespace rac1
