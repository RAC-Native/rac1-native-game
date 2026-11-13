// Ported overlay code: R.Y.N.O. homing rocket projectile (class 0x1c9, update 0x002cd1f0).
#include <game/ovl/ryno_fx.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// R.Y.N.O. rocket projectile update (class 0x1c9): quadratic lead pursuit, helical corkscrew
// wobble around the spine at `pv + 0x10` (clamped if wall-blocked), smoke trail, and impact blast.
// orig: 0x002cd1f0
void ryno_2cd1f0_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    const float gs = lwc1(0x0015ed6cu);
    const float dt = lwc1(0x0015ed60u);
    const uint32_t cls_info = lwu(m + 0x24);
    const float cur_scale = lwc1(m + 0x2c);
    const float max_spd = fpu_mul(gs, 20.0f);
    const float scale_rate = fpu_mul(dt, u2f(0x3d4ccccdu));
    const float dt64 = lwc1(0x0015ed64u);
    const float cls_scale = lwc1(cls_info + 0x24);
    const float spd_rate = fpu_mul(dt, u2f(0x3dcccccd));

    swc1(m + 0x2c, fpu_add(cur_scale, fpu_mul(fpu_sub(cls_scale, cur_scale), scale_rate)));
    const float cur_spd = lwc1(pv + 0x30);
    swc1(pv + 0x30, fpu_add(cur_spd, fpu_mul(fpu_mul(fpu_sub(max_spd, cur_spd), spd_rate), dt64)));

    const int32_t arm_threshold = frame_scale_i(0xfa) - frame_scale_i(5);
    if (lw(pv + 0x24) < arm_threshold) {
        const uint32_t target = lwu(pv + 0x28);
        bool target_valid = false;
        float wobble_dt = lwc1(0x0015ed60u);
        if (target != 0 && static_cast<int32_t>(lh(target + 0xa6)) == lw(pv + 0x58)) {
            const uint32_t tst = lbu(target + 0x20);
            if (tst != 0xfe) {
                if (tst != 0xfd) {
                    target_valid = true;
                } else {
                    wobble_dt = lwc1(0x0015ed60u);
                }
            }
        }

        if (!target_valid) {
            const float amp_inc = fpu_mul(wobble_dt, u2f(0x3dcccccd));
            const float amp = lwc1(pv + 0x44);
            sw(pv + 0x28, 0);
            swc1(pv + 0x44, fpu_add(amp, amp_inc));

            const int32_t drift_threshold = frame_scale_i(0xfa) - frame_scale_i(0x64);
            if (lw(pv + 0x24) < drift_threshold) {
                const float s = sin_vu(lwc1(pv + 0x48));
                const float dp = fpu_mul(s, fpu_mul(lwc1(0x0015ed60u), u2f(0x3d23d70au)));
                swc1(m + 0x44, angle_add(lwc1(m + 0x44), dp));
            }
            const float c = cos_vu(lwc1(pv + 0x48));
            const float dy = fpu_mul(c, fpu_mul(lwc1(0x0015ed60u), u2f(0x3d4ccccdu)));
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), dy));
        } else {
            Vec4 tgt_pos = V(target + 0x10);
            tgt_pos.z = fpu_add(tgt_pos.z, lwc1(pv + 0x2c));

            Vec4 tgt_vel{};
            vec3_sub(tgt_vel, tgt_pos, V(pv));
            Vec4 to_tgt{};
            vec3_sub(to_tgt, tgt_pos, V(pv + 0x10));
            V(pv) = tgt_pos;

            const float v_sq = vec3_length_sq_vu(tgt_vel);
            float b_term = fpu_mul(
                fpu_add(fpu_add(fpu_mul(tgt_vel.x, to_tgt.x), fpu_mul(tgt_vel.y, to_tgt.y)), fpu_mul(tgt_vel.z, to_tgt.z)),
                -2.0f);
            const float r_spd = lwc1(pv + 0x30);
            float a_term = fpu_sub(fpu_mul(r_spd, r_spd), v_sq);
            const float neg_d_sq = -vec3_length_sq_vu(to_tgt);
            const float disc = fpu_sub(fpu_mul(b_term, b_term), fpu_mul(fpu_mul(a_term, 4.0f), neg_d_sq));
            const float root = sqrt_f(disc);
            b_term = -b_term;
            a_term = fpu_add(a_term, a_term);

            const float t1 = fpu_div(fpu_add(b_term, root), a_term);
            const float t2 = fpu_div(fpu_sub(b_term, root), a_term);
            float t_hit;
            if (0.0f < t1 && 0.0f < t2) {
                t_hit = !(t2 < t1) ? t2 : t1;  // matches original bc1f polarity at 0x2cd4dc
            } else if (0.0f < t1) {
                t_hit = t1;
            } else if (0.0f < t2) {
                t_hit = t2;
            } else {
                t_hit = -1.0f;
            }

            if (0.0f < t_hit) {
                const float turn_rate = u2f(0x433c7eddu);
                Vec4 lead{};
                vec3_scale(lead, tgt_vel, t_hit);
                vec3_add(lead, lead, to_tgt);

                const float yaw_goal = atan2_f(lead.x, lead.y);
                const float k_yaw = fpu_mul(lwc1(0x0015ed70u), turn_rate);
                swc1(m + 0x48, angle_seek_rate(pv + 0x34, lwc1(m + 0x48), yaw_goal, k_yaw, 0.0f, 0.0f));

                const float pitch_goal = -atan2_f(vec2_length(lead), lead.z);
                const float k_pitch = fpu_mul(lwc1(0x0015ed70u), turn_rate);
                swc1(m + 0x44, angle_seek_rate(pv + 0x38, lwc1(m + 0x44), pitch_goal, k_pitch, 0.0f, 0.0f));
            }

            const float dist = vec3_length(to_tgt);
            if (dist < 6.0f) {
                swc1(pv + 0x44, fpu_mul(lwc1(pv + 0x44), fpu_div(dist, 6.0f)));
            } else {
                const float step = fpu_mul(lwc1(0x0015ed60u), u2f(0x3dcccccd));
                swc1(pv + 0x44, fpu_add(lwc1(pv + 0x44), step));
            }
        }
    }

    // Smoke trail particle (type 4)
    Vec4 vel{};
    vec_from_spherical_v(vel, lwc1(pv + 0x30), lwc1(m + 0x48), -lwc1(m + 0x44));

    Vec4 rnd{0.0f, 0.0f, 0.0f, 0.0f};
    rnd.x = rand_range_f(-1.0f, 1.0f);
    rnd.y = rand_range_f(-1.0f, 1.0f);
    rnd.z = rand_range_f(-1.0f, 1.0f);
    Vec4 pvel = rnd;
    const float rlen = fpu_mul(rand_range_f(u2f(0x3dcccccd), u2f(0x3e4ccccd)), lwc1(0x0015ed6cu));
    vec3_set_length(pvel, pvel, rlen);

    const float back_hi = lwc1(0x0015ed6cu);
    const float back_lo = fpu_mul(back_hi, u2f(0x3dcccccd));
    Vec4 ppos{};
    vec3_set_length(ppos, vel, -rand_range_f(back_lo, back_hi));
    vec3_add(pvel, pvel, ppos);

    const float along = fpu_mul(rand_range_f(0.0f, 1.0f), lwc1(pv + 0x30));
    vec3_set_length(ppos, vel, along);
    vec3_add(ppos, ppos, V(m + 0x10));

    const uint32_t plife = static_cast<uint32_t>(frame_scale_i(rand_range(0x1c, 0x28)));
    const uint32_t prot = static_cast<uint32_t>(rand_range(0x28, 0x46));
    spawn_particle_4_v(ppos, pvel, 0x6f00afffu, 0x100000ffu, plife, 0x28u, prot, 1);

    // Advance spine pv+0x10 and resolve wobbled world position m+0x10
    const Vec4 prev_pos = V(m + 0x10);
    vec3_add(V(pv + 0x10), V(pv + 0x10), vel);

    swc1(pv + 0x48, angle_add(lwc1(pv + 0x48), lwc1(pv + 0x50)));
    const float ph4c = angle_add(lwc1(pv + 0x4c), lwc1(pv + 0x54));
    swc1(pv + 0x4c, ph4c);
    const float ph48_mod = fpu_mul(sin_vu(ph4c), u2f(0x3e19999au));
    const float next_ph48 = angle_add(lwc1(pv + 0x48), ph48_mod);
    const bool clamp_amp = u2f(0x3f666666u) < lwc1(pv + 0x44);
    swc1(pv + 0x48, next_ph48);
    if (clamp_amp) {
        swc1(pv + 0x44, u2f(0x3f666666u));
    }

    uint32_t dampened = 0;
    for (;;) {
        const float s48 = sin_vu(lwc1(pv + 0x48));
        const float c4c_y = cos_vu(lwc1(pv + 0x4c));
        const float oy = fpu_mul(fpu_mul(lwc1(pv + 0x44), s48), c4c_y);

        const float c48 = cos_vu(lwc1(pv + 0x48));
        const float c4c_z = cos_vu(lwc1(pv + 0x4c));
        const float oz = fpu_mul(fpu_mul(lwc1(pv + 0x44), c48), c4c_z);

        Vec4 offset{0.0f, oy, oz, 0.0f};
        Mat4 rot{};
        euler_to_mat4_vu(rot, V(m + 0x40));
        mat4_transform(offset, offset, rot);
        vec3_add(V(m + 0x10), V(pv + 0x10), offset);

        if (segment_collide_v(prev_pos, V(m + 0x10), 6u, m, nullptr) == 0) break;

        dampened = 1u;
        const float new_amp = fpu_mul(lwc1(pv + 0x44), u2f(0x3f333333u));
        swc1(pv + 0x44, new_amp);
        if (!(u2f(0x3dcccccd) < new_amp)) break;
    }

    if (dampened != 0) {
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(m + 0x10), prev_pos.x), fpu_sub(lwc1(m + 0x14), prev_pos.y)));
        swc1(m + 0x44, -atan2_f(vec2_distance(prev_pos, V(m + 0x10)), fpu_sub(lwc1(m + 0x18), prev_pos.z)));
        sw(pv + 0x34, 0);
        sw(pv + 0x38, 0);
    }

    if (lwc1(m + 0x10) < 0.0f || lwc1(m + 0x14) < 0.0f || lwc1(m + 0x18) < 0.0f) {
        moby_retire(m);
        return;
    }

    Vec4 hit_dir = vel;
    vec2_set_length(hit_dir, hit_dir, 1.0f);
    const uint32_t rec[9] = {
        f2u(hit_dir.x),
        f2u(hit_dir.y),
        f2u(1.0f),
        0x45afdf66u,
        m,
        0x00830000u,
        0x03u | (0x03u << 8) | (static_cast<uint32_t>(lhu(m + 0xa6)) << 16),
        f2u(3.0f),
        1u,
    };

    if (segment_collide_v(prev_pos, V(m + 0x10), 0u, m, rec) != 0) {
        const uint32_t outb = OV(0x00173e40u);
        const uint32_t hit = lwu(outb + 0x18);
        if (hit != 0) {
            if (hit != lwu(0x001413d0u) && hit != lwu(pv + 0x20)) {
                const int32_t hit_cls = lh(hit + 0xa6);
                if (hit_cls != 0x1c9 && hit_cls != 0) {
                    sb(m + 0xbc, 2);
                    V(m + 0x10) = V(outb + 0x20);
                    explosion_effect(m, 0u, 0u, 0xa, 3, 9, 0, 1, 1, -1, 0u, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
                    moby_retire(m);
                    return;
                }
            }
        } else if (lw(outb + 0x1c) > 0) {
            sb(m + 0xbc, 1);
            V(m + 0x10) = V(outb + 0x20);
            explosion_effect(m, 0u, 0u, 0xa, 3, 9, 0, 1, 1, -1, 0u, 0.0f, 1.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
            moby_retire(m);
            return;
        }
    } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x24))) != 0 ||
               200.0f < vec2_distance(V(m + 0x10), V(lwu(0x001413d0u) + 0x10))) {
        moby_retire(m);
        return;
    }

    // Periodic random flip of corkscrew angular velocities
    const int32_t step = frame_scale_i(0x20);
    if (ee_rem(lw(pv + 0x24), step) == 0) {
        if ((static_cast<uint32_t>(libc_rand()) & 1u) != 0) {
            const float mul50 = rand_range_f(u2f(0xbf333333u), -1.5f);
            swc1(pv + 0x50, fpu_mul(lwc1(pv + 0x50), mul50));
        }
        if ((static_cast<uint32_t>(libc_rand()) & 1u) != 0) {
            const float mul54 = rand_range_f(u2f(0xbf19999au), u2f(0xbfcccccd));
            swc1(pv + 0x54, fpu_mul(lwc1(pv + 0x54), mul54));
        }
    }
}

// orig: (none)
void ryno_fx_fns_register() {
    ovl_register_moby_fn(0x002cd1f0, ryno_2cd1f0_update);
}

}  // namespace rac1
