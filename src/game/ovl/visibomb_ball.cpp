// Ported overlay code: the Visibomb Gun's guided rocket (class 0xac, update 0x2b6140),
// its exhaust nozzle / tail-fin animation, detonation particles, and camera/fog teardown.
#include <game/ovl/visibomb_ball.h>

#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/visibomb.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t PAD = 0x0013c940u;
constexpr uint32_t SND_CHANNELS = 0x0013e550u;

void stop_ball_sound(uint32_t m, uint32_t slot_addr) {
    const int32_t slot = lw(slot_addr);
    if (slot != -1) {
        const uint32_t ch = SND_CHANNELS + static_cast<uint32_t>(slot) * 0x70u;
        if (lwu(ch + 0x88) == m && lbu(ch + 0x74) != 0) {
            sub_0028d918(slot);
        }
    }
    sw(slot_addr, 0xffffffffu);
}
}  // namespace

// orig: 0x002b6140
void visibomb_ball_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    sw(OV(0x0015f608u), 1u);
    sw(OV(0x0015f458u), 1u);
    visibomb_203b18();
    sw(OV(0x0017e508u), 1u);
    hud_message_force(6, 0);

    sw(pv + 0x10, lwu(pv + 0x10) - 1u);
    const int32_t min_life = static_cast<int32_t>(0u - static_cast<uint32_t>(frame_scale_i(0x64)));
    if (!(min_life < lw(pv + 0x10))) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }

    const int32_t hstate = lw(P + 0x2084);
    if (hstate == 0x16 || hstate == 0x3d || hstate == 0x72) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }

    const int32_t gstate = lw(P + 0x208c);
    if (gstate == 2 || gstate == 4 || gstate == 3 || gstate >= 13 ||
        gstate == 7 || gstate == 0xa || gstate == 0x10 || gstate == 5) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }

    if (hstate == 0x65) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }
    if (gameplay102_on(8) && static_cast<uint32_t>(hstate) == 0x32u) {  // enhancement: 1.02 Gameplay, fix 8 (fixes102.h)
        visibomb_ball_2b5b20(m, pv);
        return;
    }
    if (lwu(P + 0x300) == 0) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }

    const uint32_t ride = lwu(P + 0x2fc);
    if (ride != 0) {
        const int16_t rcls = lh(ride + 0xa6);
        if (rcls == 0x13e || rcls == 0x46f) {
            visibomb_ball_2b5b20(m, pv);
            return;
        }
    }

    if (lw(pv + 0x10) < 0) {
        const uint32_t cam = lwu(pv + 0x14);
        if (cam != 0) visibomb_2eb648(cam, m);
        visibomb_ball_2b57f0(m, 0u);
        visibomb_ball_2b5ba0(m, pv);
        return;
    }

    const float gs = lwc1(0x0015ed6cu);
    float target_spd = fpu_mul(lwc1(OV(0x0016149cu)), gs);
    if ((lwu(PAD + 0x1a0) & 0x40u) != 0) {
        target_spd = fpu_mul(lwc1(OV(0x001614a0u)), gs);
    }

    float spd48 = lwc1(pv + 0x48);
    const float accel48 = lwc1(OV(0x001614a4u));
    float spd4 = lwc1(pv + 0x4);
    const float max_raw = lwc1(OV(0x001614a0u));
    const float accel4 = lwc1(OV(0x001614a8u));
    const float max_spd = fpu_mul(max_raw, gs);

    spd48 = fpu_add(spd48, fpu_mul(fpu_sub(target_spd, spd48), accel48));
    swc1(pv + 0x48, spd48);
    spd4 = fpu_add(spd4, fpu_mul(fpu_sub(spd48, spd4), accel4));
    swc1(pv + 0x4, spd4);
    if (max_spd < spd4) {
        swc1(pv + 0x4, max_spd);
    }

    {
        const float gs2 = lwc1(0x0015ed6cu);
        const float lo_spd = fpu_mul(lwc1(OV(0x0016149cu)), gs2);
        const float hi_spd = fpu_mul(lwc1(OV(0x001614a0u)), gs2);
        const float cur48 = lwc1(pv + 0x48);
        const float pitch_k = lwc1(OV(0x00161498u));
        const float ratio = fpu_div(lo_spd, fpu_sub(hi_spd, lo_spd));
        const int32_t snd_pitch = float_to_int(fpu_mul(fpu_sub(cur48, ratio), pitch_k));
        metal_28df38(lwu(pv + 0x2c), static_cast<uint32_t>(snd_pitch));
    }

    if (lw(pv + 0x10) > 0) {
        const int32_t steer_start =
            static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(0xbb8)) - static_cast<uint32_t>(frame_scale_i(5)));
        if (lw(pv + 0x10) < steer_start) {
            const float dt = lwc1(0x0015ed60u);
            const float stick_y = lwc1(P + 0x1d24);
            const float stick_x = lwc1(P + 0x1d20);
            float turn_x = lwc1(pv + 0x18);
            float turn_y = lwc1(pv + 0x1c);

            const float k_in = fpu_mul(dt, u2f(0x3dcccccdu));
            const float k_roll = fpu_mul(dt, u2f(0x3d75c28fu));
            const float k_ang = fpu_mul(dt, u2f(0x3c490fdbu));

            turn_y = fpu_add(turn_y, fpu_mul(fpu_sub(stick_y, turn_y), k_in));
            turn_x = fpu_add(turn_x, fpu_mul(fpu_sub(stick_x, turn_x), k_in));
            swc1(pv + 0x1c, turn_y);
            swc1(pv + 0x18, turn_x);

            const float cur_roll = lwc1(m + 0x40);
            const float cur_pitch = lwc1(m + 0x44);
            swc1(m + 0x40, fpu_add(cur_roll, fpu_mul(fpu_sub(turn_x, cur_roll), k_roll)));

            const float new_pitch = angle_sub(cur_pitch, fpu_mul(lwc1(pv + 0x1c), k_ang));
            swc1(m + 0x44, new_pitch);
            if (u2f(0x3fc90fdbu) <= new_pitch) {
                swc1(m + 0x44, u2f(0x3fc8ef16u));
            } else if (new_pitch <= u2f(0xbfc90fdbu)) {
                swc1(m + 0x44, u2f(0xbfc8ef16u));
            }

            const float dt2 = lwc1(0x0015ed60u);
            const float dyaw = fpu_mul(lwc1(pv + 0x18), fpu_mul(dt2, u2f(0x3c490fdbu)));
            swc1(m + 0x48, angle_sub(lwc1(m + 0x48), dyaw));
            floor_light_add(m, u2f(0x3e4ccccdu));
        }
    }

    Vec4 step{};
    vec_from_spherical_v(step, lwc1(pv + 0x4), lwc1(m + 0x48), -lwc1(m + 0x44));
    const Vec4 old_pos = V(m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), step);

    const int32_t grace_end =
        static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(0xbb8)) - static_cast<uint32_t>(frame_scale_i(0x3c)));
    if (grace_end < lw(pv + 0x10)) {
        const float floor_z = floor_probe(0.5f, m + 0x10, 0u);
        const float min_z = fpu_add(floor_z, lwc1(pv + 0x44));
        if (lwc1(m + 0x18) < min_z) {
            const uint32_t hit_m = lwu(OV(0x00173e58u));
            bool push_up = false;
            if (hit_m == 0) {
                push_up = true;
            } else {
                const uint32_t cls_info = lwu(hit_m + 0x24);
                if (cls_info != 0 && lh(cls_info + 0x46) == 5) {
                    push_up = true;
                }
            }
            if (push_up) {
                const float dz = approach_f(m + 0x18, min_z, fpu_mul(lwc1(0x0015ed6cu), 6.0f));
                float dpitch = atan2_f(lwc1(pv + 0x4), dz);
                const float max_dp = fpu_mul(lwc1(0x0015ed6cu), u2f(0x3fc90fdbu));
                if (max_dp < dpitch) dpitch = max_dp;
                swc1(m + 0x44, fpu_sub(lwc1(m + 0x44), dpitch));
            }
        }
    }

    const float lo_b = 4.5f;
    const float hi_b = u2f(0x447ea000u);
    const float px = lwc1(m + 0x10);
    if (px < lo_b || hi_b < px) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }
    const float py = lwc1(m + 0x14);
    if (py < lo_b || hi_b < py) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }
    const float pz = lwc1(m + 0x18);
    if (pz < lo_b || hi_b < pz) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }

    visibomb_ball_2b5ba0(m, pv);

    {
        const float a0 = abs_f(fpu_div(lwc1(pv + 0x18), 3.0f));
        const float roll0 = lwc1(m + 0x40);
        const uint32_t fin0 = lwu(pv + 0x24) + 0x10;
        float qang0 = fpu_add(fpu_mul(lwc1(pv + 0x1c), 0.5f), a0);
        const float dr0 = fpu_mul(fpu_sub(lwc1(pv + 0x18), roll0), u2f(0x3f8ccccdu));
        qang0 = fpu_mul(-fpu_add(qang0, dr0), u2f(0x3f333333u));
        quat_from_axis_angle(V(fin0), 1, qang0);
    }
    {
        const float a1 = abs_f(fpu_div(lwc1(pv + 0x18), 3.0f));
        const float roll1 = lwc1(m + 0x40);
        const uint32_t fin1 = lwu(pv + 0x28) + 0x10;
        float qang1 = fpu_add(fpu_mul(lwc1(pv + 0x1c), 0.5f), a1);
        const float dr1 = fpu_mul(fpu_sub(lwc1(pv + 0x18), roll1), u2f(0x3f8ccccdu));
        qang1 = fpu_mul(-fpu_sub(qang1, dr1), u2f(0x3f333333u));
        quat_from_axis_angle(V(fin1), 1, qang1);
    }

    Vec4 rec_dir = step;
    vec2_set_length(rec_dir, rec_dir, 1.0f);
    const uint32_t rec[9] = {
        f2u(rec_dir.x),
        f2u(rec_dir.y),
        0x3f800000u,
        0x45afdf66u,
        m,
        0x00830000u,
        0x0203u | (static_cast<uint32_t>(lhu(m + 0xa6)) << 16),
        0x40c00000u,
        1u,
    };

    uint32_t ignore = m;
    if (static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(0xbb8)) - 0x28u) < lw(pv + 0x10)) {
        ignore = lwu(P + 0x2080);
    }

    bool hit_hero = false;
    if (segment_collide_v(old_pos, V(m + 0x10), 0u, ignore, rec) != 0) {
        const uint32_t outb = OV(0x00173e40u);
        const uint32_t hit_moby = lwu(outb + 0x18);
        bool valid_hit = false;
        if (hit_moby != 0) {
            sb(m + 0xbc, 2u);
            hit_hero = (hit_moby == lwu(P + 0x2080));
            V(m + 0x10) = V(outb + 0x20);
            valid_hit = true;
        } else if (lw(outb + 0x1c) > 0) {
            sb(m + 0xbc, 1u);
            V(m + 0x10) = V(outb + 0x20);
            valid_hit = true;
        }
        if (valid_hit) {
            stop_ball_sound(m, pv + 0x2c);
        }
    } else if (lw(pv + 0x10) == 0) {
        visibomb_ball_2b5b20(m, pv);
        return;
    }

    const int32_t det_allow =
        static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(0xbb8)) - static_cast<uint32_t>(frame_scale_i(0x3c)));
    if (lw(pv + 0x10) < det_allow && (lwu(PAD + 0x1a4) & 0x20u) != 0) {
        sb(m + 0xbc, 1u);
        stop_ball_sound(m, pv + 0x2c);
    }

    if (lbu(m + 0xbc) != 0) {
        if (!hit_hero) {
            explosion_effect(m, 0u, 0u, 0xa, 3, 0x10, 1, 1, 1, -1, 0u, 0.0f, 0.0f, 4.0f, 2.0f, 0.0f, 1.0f, 20.0f);
            const int32_t hit_cnt = mobys_in_sphere_v(V(m + 0x10), 4.0f, 0x10u, m, nullptr);
            const Vec4 hit_pos = V(m + 0x10);
            moby_list_hit_v(m, hit_pos, OV(0x00177f00u), hit_cnt, 0u, 0x00830000u, 3u, 2u, 6.0f, 1.0f, 1.0f);
        }

        sh(m + 0x34, lhu(m + 0x34) | 1u);
        const float dt = lwc1(0x0015ed60u);
        swc1(pv + 0x4, fpu_mul(dt, u2f(0x3ca3d70au)));
        swc1(pv + 0x18, fpu_mul(lwc1(pv + 0x18), u2f(0x3c23d70au)));

        const int32_t early_window =
            static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(0xbb8)) - static_cast<uint32_t>(frame_scale_i(0x1e)));
        int32_t next_life = -1;
        if (early_window < lw(pv + 0x10)) {
            next_life = static_cast<int32_t>(0u - static_cast<uint32_t>(frame_scale_i(0x64)));
        }
        sw(pv + 0x10, static_cast<uint32_t>(next_life));

        const Vec4 saved_pos = V(m + 0x10);
        for (int i = 4; i >= 0; --i) {
            V(m + 0x10) = saved_pos;
            metal_25f8e0(m + 0x10, 1.5f);
            visibomb_ball_2b5d00(m, 1u, 0u, 0u);
            V(m + 0x10) = saved_pos;
        }
        sb(m + 0xbc, 0u);
    }

    const uint32_t cam = lwu(pv + 0x14);
    if (cam != 0) {
        visibomb_2eb648(cam, m);
    }
}

// orig: 0x002b57f0
void visibomb_ball_2b57f0(uint32_t p0, uint32_t p1) {
    const uint32_t pv = lwu(p0 + 0x78);
    sw(OV(0x0015f30cu), 0u);

    const uint32_t f0 = lwu(pv + 0x30);
    const uint32_t f1 = lwu(pv + 0x34);
    const uint32_t f2 = lwu(pv + 0x38);
    const uint32_t f3 = lwu(pv + 0x3c);
    sw(OV(0x0015f448u), f0);
    sw(OV(0x0015f44cu), f1);
    sw(OV(0x0015f450u), f2);
    sw(OV(0x0015f454u), f3);

    sb(OV(0x0015f444u), lbu(pv + 0x40));
    sb(OV(0x0015f445u), lbu(pv + 0x41));
    const uint8_t b2 = lbu(pv + 0x42);
    sw(OV(0x00169ff8u), 1u);
    sb(OV(0x0015f446u), b2);

    if (p1 != 0) {
        swc1(OV(0x00160f80u), u2f(0x48fa0000u));
        swc1(OV(0x00160fe0u), u2f(0x44340000u));
        swc1(OV(0x001604a4u), u2f(0x43fa0000u));
        sw(OV(0x0015fff0u), 0x1f4u);
        sw(OV(0x0016017cu), 0x1f4000u);
    }

    const uint32_t env = OV(0x0016ca40u);
    bg_color_set(lwu(env + 0x23c), lwu(env + 0x240), lwu(env + 0x244));
}

// orig: 0x002b5b20
void visibomb_ball_2b5b20(uint32_t p0, uint32_t p1) {
    const uint32_t hand = lwu(p1 + 0x0);
    const uint32_t hand_pv = lwu(hand + 0x78);
    if (hand_pv != 0) {
        sw(hand_pv + 0x4, 0u);
    }

    sw(P + 0x1fe0, 0u);
    visibomb_ball_2eba18(lwu(p1 + 0x14));
    sw(p1 + 0x14, 0u);
    hero_set_state(0u, 1);
    visibomb_ball_2b57f0(p0, 1u);
    visibomb_ball_203b78();
    moby_retire(p0);
    sw(OV(0x00167014u), 0u);
}

// orig: 0x002b5ba0
void visibomb_ball_2b5ba0(uint32_t p0, uint32_t p1) {
    const uint32_t nozzle = lwu(p1 + 0x20);
    if (nozzle == 0) return;

    const Vec4 local_off{0.0f, u2f(0x3de147aeu), 0.0f, 0.0f};
    const float target = u2f(0x3ee66666u);
    const uint32_t npv = lwu(nozzle + 0x78);
    const float cur = lwc1(npv + 0xc);
    const float dt = lwc1(0x0015ed60u);
    if (target < cur) {
        swc1(npv + 0xc, fpu_sub(cur, fpu_mul(dt, u2f(0x3d4ccccdu))));
    } else if (cur < target) {
        swc1(npv + 0xc, fpu_add(cur, fpu_mul(dt, u2f(0x3d4ccccdu))));
    }

    const uint32_t n0 = lwu(p1 + 0x20);
    swc1(n0 + 0x48, fpu_mul(lwc1(0x0015ed60u), u2f(0xbfc90fdbu)));

    Mat4 rot{};
    euler_to_mat4_vu(rot, V(lwu(p1 + 0x20) + 0x40));
    Mat4 base{};
    mat3_copy_affine(base, M(p0 + 0xc0));
    Mat4 comb{};
    mat4_mul(comb, base, rot);
    mat3_copy(M(lwu(p1 + 0x20) + 0xc0), comb);

    Vec4 world_off{};
    mat4_transform(world_off, local_off, comb);
    vec3_add(V(lwu(p1 + 0x20) + 0x10), world_off, V(p0 + 0x10));
    moby_update_transform_kept(lwu(p1 + 0x20));
}

// orig: 0x002b5d00
void visibomb_ball_2b5d00(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) {
    (void)p2;
    (void)p3;

    Vec4 ppos = V(p0 + 0x10);
    ppos.z = fpu_add(ppos.z, u2f(0x3e99999au));
    spawn_particle_0d_v(ppos, 1, 0x40808080u, u2f(0x3e99999au), u2f(0x3f8147aeu), u2f(0x3f89999au), u2f(0x3d4ccccdu),
                        u2f(0x47435000u));
    for (int i = 4; i >= 0; --i) {
        spawn_particle_0d_v(ppos, 1, 0x40808080u, u2f(0x3e99999au), u2f(0x3f8147aeu), u2f(0x3f88f5c3u), u2f(0x3d4ccccdu),
                            u2f(0x47435000u));
    }

    if (p1 == 0) return;

    Vec4 to_cam{};
    vec3_sub(to_cam, V(OV(0x00166dc0u)), V(p0 + 0x10));
    Vec4 zero_vel{};
    vec_clear(zero_vel);

    const float dist = vec3_length(to_cam);
    int32_t count = 10;
    if (dist < 8.0f) {
        count = float_to_int(dist) + 2;
    }

    float close_sub = 0.0f;
    if (dist < 7.0f) {
        close_sub = fpu_sub(7.0f, dist);
    }

    if (count > 0) {
        for (int32_t rem = count; rem != 0; --rem) {
            const float r = rand_range_f(8.0f, 10.0f);
            const float gs = lwc1(0x0015ed6cu);
            const float spd = fpu_sub(fpu_mul(r, gs), fpu_mul(close_sub, gs));
            const uint32_t c_idx = static_cast<uint32_t>(rand_mod(6));
            const uint32_t a3_idx = static_cast<uint32_t>(rand_mod(6));
            const int32_t lo1 = frame_scale_i(0xf);
            const int32_t hi1 = frame_scale_i(0x14);
            const uint32_t p_cnt = static_cast<uint32_t>(rand_range(lo1, hi1));
            const int32_t lo2 = frame_scale_i(0x19);
            const int32_t hi2 = frame_scale_i(0x1e);
            const uint32_t b38 = static_cast<uint32_t>(rand_range(lo2, hi2));
            const uint32_t col = lwu(OV(0x001e9a58u) + (c_idx << 2));
            const uint32_t col2 = lwu(OV(0x001e9a70u) + (a3_idx << 2));
            spawn_particle_0b_v(V(p0 + 0x10), zero_vel, col, col2, p_cnt, b38, 0u, 0u, u2f(0x48c35000u), spd);
        }
    }

    if (lwc1(OV(0x0015f5d0u)) < u2f(0x3f733333u)) {
        shard_spawn_v(p0, V(p0 + 0x10), zero_vel, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7fu, 0x7fu, 0x7fu, 0x20u, 3.0f);
        shard_spawn_v(p0, V(p0 + 0x10), zero_vel, static_cast<uint32_t>(frame_scale_i(0x18)), 0x7fu, 0x20u, 0u, 0x20u, 2.0f);
    }
    shard_spawn_v(p0, V(p0 + 0x10), zero_vel, static_cast<uint32_t>(frame_scale_i(0x14)), 0x7fu, 0x40u, 0u, 0x30u, 3.0f);
    shard_spawn_v(p0, V(p0 + 0x10), zero_vel, static_cast<uint32_t>(frame_scale_i(0x1b)), 0x60u, 0x10u, 0u, 0x40u, 2.0f);
    shard_spawn_v(p0, V(p0 + 0x10), zero_vel, static_cast<uint32_t>(frame_scale_i(0x1d)), 0x20u, 0u, 0u, 0x20u, 1.0f);
}

// orig: 0x00203b78
void visibomb_ball_203b78() {
    sh(OV(0x00179448u), 0u);
}

// orig: 0x002eba18
void visibomb_ball_2eba18(uint32_t p0) {
    const uint32_t cam_g = OV(0x00166c80u);
    const uint32_t hero = lwu(P + 0x2080);
    const float dx = fpu_sub(lwc1(hero + 0x10), lwc1(cam_g + 0x140));
    const float dy = fpu_sub(lwc1(hero + 0x14), lwc1(cam_g + 0x144));
    const float yaw_to = atan2_f(dx, dy);
    const float adist = angle_dist(lwc1(cam_g + 0x158), yaw_to);

    if (adist < u2f(0x3f0efa35u) && vec3_distance(V(P + 0x80), V(cam_g + 0x140)) < 8.0f) {
        const float k = u2f(0x3c9374bcu);
        sb(cam_g + 0x273, 0u);
        sh(p0 + 0x7e, 2u);
        swc1(cam_g + 0x288, k);
        swc1(cam_g + 0x294, k);
    } else {
        sh(p0 + 0x7e, 4u);
    }
    sb(p0 + 0x7d, 0u);
}

// orig: (none) -- the ports for the engine's indirect calls (a moby class's update, a draw callback)
void visibomb_ball_fns_register() {
    ovl_register_moby_fn(0x002b6140, visibomb_ball_update);
}

}  // namespace rac1
