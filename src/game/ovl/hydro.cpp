// Ported overlay code: the Hydrodisplacer (item 0x16, hand class 1251, update 0x2df9a0) and scripted camera end (0x2eac18).
#include <game/ovl/hydro.h>

#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ovl.h>
#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t HERO_ROT = 0x0013f3e0u;
}  // namespace

// orig: 0x002e02e0
int32_t hydro_2e02e0() {
    const uint32_t moby_base = lwu(OV(0x0015ffd8u));
    const uint32_t ground_moby = lwu(P + 0x2fc);
    const uint32_t has_water = lbu(P + 0x20b0);
    const uint32_t pool_idx = lwu(lwu(ground_moby + 0x78));
    const uint32_t pool_moby = moby_base + (pool_idx << 8);
    if (has_water == 0) {
        return (lbu(pool_moby + 0xbc) & 0x2u) != 0 ? 1 : 0;
    }
    return (lbu(pool_moby + 0xbc) & 0x1u) != 0 ? 1 : 0;
}

// orig: 0x001eb448
void hydro_1eb448() {
    const uint32_t cam_state = OV(0x00166c80u);
    sh(lwu(cam_state + 0x180) + 0x7e, 1);
    sb(lwu(cam_state + 0x180) + 0x7d, 0);
}

// orig: 0x002eac18
void hydro_2eac18(uint32_t p0) {
    const uint32_t cam_state = OV(0x00166c80u);
    const uint32_t cam = lwu(cam_state + 0x180);
    hydro_1eb448();
    if (p0 == 0) {
        Mat4 rot{};
        euler_to_mat4_vu(rot, V(HERO_ROT));
        mat3_copy(M(cam), rot);
        Vec4 back{}, up{};
        vec3_set_length(back, rot.r[0], -1.0f);
        vec3_set_length(up, rot.r[2], u2f(0x3f99999au));
        vec3_add(V(cam + 0x30), V(HERO_POS), up);
        vec3_add(V(cam + 0x30), V(cam + 0x30), back);
        sh(lwu(cam_state + 0x180) + 0x8a, 1);
    } else if (p0 == 2) {
        const float rate = u2f(0x3c9374bcu);
        sb(cam_state + 0x273, 0);
        sh(cam + 0x7e, 2);
        swc1(cam_state + 0x288, rate);
        swc1(cam_state + 0x294, rate);
    } else if (p0 == 1) {
        sh(cam + 0x7e, 3);
    } else if (p0 == 4) {
        const float rate = u2f(0x3c9374bcu);
        sb(cam_state + 0x273, 0);
        sh(cam + 0x7e, 5);
        swc1(cam_state + 0x288, rate);
        swc1(cam_state + 0x294, rate);
    }
}

// orig: 0x002e01b0
void hydro_2e01b0(uint32_t p0) {
    const uint32_t pv = lwu(p0 + 0x78);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4))) == 0) return;

    Vec4 pos{};
    moby_chain_point_v(p0, 0, pos);

    const float dt = lwc1(0x0015ed6cu);
    const float spd = rand_range_f(dt, fpu_mul(dt, 3.0f));
    Vec4 vel{};
    vec3_set_length(vel, V(p0 + 0xd0), -spd);

    const float k08 = u2f(0x3f4ccccdu);
    const float hx = fpu_mul(lwc1(P + 0x100), k08);
    const float hy = fpu_mul(lwc1(P + 0x104), k08);
    const float dt2 = lwc1(0x0015ed6cu);
    vel.x = fpu_add(vel.x, hx);
    const float z_lo = fpu_mul(dt2, -0.5f);
    vel.y = fpu_add(vel.y, hy);
    const float z_hi = fpu_add(dt2, dt2);
    const float rz = rand_range_f(z_lo, z_hi);
    vel.z = fpu_add(vel.z, fpu_add(lwc1(P + 0x108), rz));

    const uint32_t life = static_cast<uint32_t>(rand_range(0x5a, 0x78));
    spawn_particle_23_v(pos, vel, 2, life);

    const int32_t lo = frame_scale_i(5);
    const int32_t hi = frame_scale_i(0xf);
    sw(pv + 4, static_cast<uint32_t>(rand_range(lo, hi)));
}

// orig: 0x002e0340
void hydro_2e0340(uint32_t p0) {
    if (lbu(p0 + 0x20) == 4) {
        sw(OV(0x0015f404u), 0);
        sw(OV(0x0015f3fcu), 0);
        hydro_2eac18(3);
    }
    if (lw(0x001413dcu) == 0x13) {
        (void)hero_set_state(0, 1);
    }
}

// orig: 0x002df9a0
void hydro_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(P + 0x10b4) == 3) {
        hydro_2e0340(m);
        return;
    }

    if (lbu(P + 0x20b0) != 0) {
        if (lw(OV(0x0015f5c4u)) == 0 && static_cast<uint32_t>(lw(P + 0x2084) - 0x38) >= 2u) {
            hydro_2e01b0(m);
        }
        if (lbu(P + 0x20b0) != 0) {
            const float k64 = lwc1(0x0015ed64u);
            const float k = fpu_mul(k64, u2f(0x3ca3d70au));
            const float d = fpu_mul(k64, u2f(0x3e99999au));
            const uint32_t hst = static_cast<uint32_t>(lw(P + 0x2084) - 0x38);
            sh(P + 0x1990, 1);
            swc1(P + 0x199c, u2f(0x40133333u));
            swc1(P + 0x1994, k);
            swc1(P + 0x1998, d);
            if (hst >= 2u) {
                const float inv_scale = fpu_mul(fpu_div(1024.0f, lwc1(m + 0x2c)), u2f(0xbe4ccccdu));
                swc1(P + 0x1af8, d);
                sh(P + 0x1a40, 2);
                sh(P + 0x1af0, 3);
                swc1(P + 0x1af4, k);
                swc1(P + 0x1a44, k);
                swc1(P + 0x1a48, d);
                swc1(P + 0x1ae4, inv_scale);
                swc1(P + 0x1a34, inv_scale);
            }
        }
    }

    switch (lbu(m + 0x20)) {
    case 0: {
        (void)moby_sound_start(0, 0, m);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {
        if ((lbu(m + 0x70) & 0x2u) != 0) {
            sb(m + 0x20, 2);
        }
        break;
    }
    case 2: {
        const int32_t win = frame_scale_i(8);
        if (pad_button_history(lwu(P + 0x10a0), win, nullptr) == 0) break;
        if ((lwu(0x0013cae0u) & lwu(P + 0x10a0)) == 0) break;
        const uint32_t gm = lwu(P + 0x2fc);
        if (gm == 0 || lh(P + 0x30e) != 0 || lh(gm + 0xa6) != 0x155) break;
        if (hydro_2e02e0() == 0) break;
        if (lwu(P + 0x208c) >= 2u) break;

        sb(m + 0x20, 3);
        (void)hero_set_state(0x38, 0);
        const uint32_t has_water = lbu(P + 0x20b0);
        const float blend = static_cast<float>(frame_scale_i(6));
        hero_anim_set(has_water != 0 ? 0x83u : 0x46u, 0, blend);
        break;
    }
    case 3: {
        const uint32_t gm = lwu(P + 0x2fc);
        if (gm == 0 || lh(gm + 0xa6) != 0x155) {
            hydro_2e0340(m);
            return;
        }
        if ((lwu(P + 0xa98) & 0x2u) == 0) break;

        const uint32_t gpv = lwu(gm + 0x78);
        const uint32_t pool_moby = lwu(OV(0x0015ffd8u)) + (lwu(gpv) << 8);
        (void)hero_set_state(0x39, 0);
        const float blend = static_cast<float>(frame_scale_i(6));
        hero_anim_set(lbu(P + 0x20b0) != 0 ? 0x84u : 0x47u, 0, blend);
        (void)moby_sound_start(lbu(P + 0x20b0) != 0 ? 0x15 : 0x14, 0, lwu(P + 0x2080));

        if (lw(gpv + 4) != -1 && lw(gpv + 8) != -1) {
            sb(m + 0xbc, 0);
            sb(m + 0x20, 4);
            const uint32_t cam_pos = OV(0x00166dc0u);
            sw(pv + 0x10, 0);
            camera_scripted_set(cam_pos, cam_pos + 0x10u, 1, 0, 0);
            if (lbu(P + 0x20b0) != 0) {
                sw(pv + 8, lwu(gpv + 4));
                sw(pv + 0xc, lwu(gpv + 8));
            } else {
                sw(pv + 0xc, lwu(gpv + 4));
                sw(pv + 8, lwu(gpv + 8));
            }
        } else {
            sb(m + 0x20, 5);
            sw(pv, static_cast<uint32_t>(float_to_int(frame_scale_f(lwc1(OV(0x00161bb4u))))));
            if (lbu(P + 0x20b0) == 0) {
                sb(pool_moby + 0xbc, 8);
                sb(P + 0x20b0, 1);
            } else {
                sb(pool_moby + 0xbc, 4);
                sb(P + 0x20b0, 0);
            }
        }
        break;
    }
    case 4: {
        const uint32_t gm = lwu(P + 0x2fc);
        if (gm == 0 || lh(gm + 0xa6) != 0x155) {
            hydro_2e0340(m);
            return;
        }
        const uint32_t sub_st = lbu(m + 0xbc);
        if (sub_st == 0) {
            const float step = fpu_div(1.0f, static_cast<float>(frame_scale_i(0xf)));
            (void)approach_f(OV(0x0015f3fcu), 1.0f, step);
            if (!(1.0f <= lwc1(OV(0x0015f3fcu)))) break;

            const uint32_t moby_base = lwu(OV(0x0015ffd8u));
            const uint32_t gm2 = lwu(P + 0x2fc);
            const uint32_t pool_idx = lwu(lwu(gm2 + 0x78));
            sb(m + 0xbc, 1);
            const uint32_t pool_moby = moby_base + (pool_idx << 8);
            uid5_set_pvar_80(lvd::box(lvd::BOXES, lw(pv + 8)) + 0x30u);
            uid5_set_pvar_90(lvd::box(lvd::BOXES, lw(pv + 8)) + 0x70u);
            const uint32_t has_water = lbu(P + 0x20b0);
            sw(OV(0x0015f404u), 1);
            if (has_water == 0) {
                sb(pool_moby + 0xbc, 8);
                sb(P + 0x20b0, 1);
            } else {
                sb(pool_moby + 0xbc, 4);
                sb(P + 0x20b0, 0);
            }
        } else if (sub_st == 1) {
            const float cur_t = lwc1(pv + 0x10);
            if (cur_t == 0.0f && 0.0f < lwc1(OV(0x0015f3fcu))) {
                const float step = fpu_div(1.0f, static_cast<float>(frame_scale_i(0xf)));
                (void)approach_f(OV(0x0015f3fcu), 0.0f, step);
                break;
            }
            if (cur_t < 1.0f) {
                const float accel = fpu_mul(lwc1(0x0015ed70u), u2f(0x3f2a7efau));
                const float vmax = fpu_mul(lwc1(0x0015ed6cu), 0.5f);
                (void)seek_1d_v(*static_cast<float*>(ee_ram_ptr(pv + 0x10)),
                                *static_cast<float*>(ee_ram_ptr(pv + 0x14)), 1.0f, accel, accel, vmax);

                const uint32_t n_from = lvd::box(lvd::BOXES, lw(pv + 8));
                const uint32_t n_to = lvd::box(lvd::BOXES, lw(pv + 0xc));
                Vec4 pos{};
                vec3_lerp(pos, V(n_from + 0x30), V(n_to + 0x30), lwc1(pv + 0x10));

                Vec4 rot{};
                vec_clear(rot);
                {
                    const uint32_t f = lvd::box(lvd::BOXES, lw(pv + 8));
                    const uint32_t t = lvd::box(lvd::BOXES, lw(pv + 0xc));
                    const float dy = fpu_mul(angle_sub(lwc1(t + 0x74), lwc1(f + 0x74)), lwc1(pv + 0x10));
                    const uint32_t fb = lvd::box(lvd::BOXES, lw(pv + 8));
                    rot.y = angle_add(lwc1(fb + 0x74), dy);
                }
                {
                    const uint32_t f = lvd::box(lvd::BOXES, lw(pv + 8));
                    const uint32_t t = lvd::box(lvd::BOXES, lw(pv + 0xc));
                    const float dz = fpu_mul(angle_sub(lwc1(t + 0x78), lwc1(f + 0x78)), lwc1(pv + 0x10));
                    const uint32_t fb = lvd::box(lvd::BOXES, lw(pv + 8));
                    rot.z = angle_add(lwc1(fb + 0x78), dz);
                }

                const uint32_t tmp = ee_stack_alloc(0x20);
                V(tmp) = pos;
                V(tmp + 0x10u) = rot;
                uid5_set_pvar_80(tmp);
                uid5_set_pvar_90(tmp + 0x10u);
                ee_stack_free(0x20);
            } else if (lwc1(OV(0x0015f3fcu)) < 1.0f) {
                const float step = fpu_div(1.0f, static_cast<float>(frame_scale_i(0xf)));
                (void)approach_f(OV(0x0015f3fcu), 1.0f, step);
            } else {
                sb(m + 0xbc, 2);
                sw(OV(0x0015f404u), 0);
                hydro_2eac18(3);
            }
        } else if (sub_st == 2) {
            if (0.0f < lwc1(OV(0x0015f3fcu))) {
                const float step = fpu_div(1.0f, static_cast<float>(frame_scale_i(0xf)));
                (void)approach_f(OV(0x0015f3fcu), 0.0f, step);
            } else {
                sb(m + 0x20, 2);
                (void)hero_set_state(0x3a, 0);
                const float blend = static_cast<float>(frame_scale_i(6));
                hero_anim_set(lbu(P + 0x20b0) != 0 ? 0x48u : 0x85u, 0, blend);
            }
        }
        break;
    }
    case 5: {
        const uint32_t gm = lwu(P + 0x2fc);
        if (gm != 0 && lh(gm + 0xa6) == 0x155 &&
            timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv))) == 0) {
            break;
        }
        hydro_2e0340(m);
        sb(m + 0x20, 2);
        break;
    }
    default:
        break;
    }
}

// orig: (none)
void hydro_fns_register() {
    ovl_register_moby_fn(0x002df9a0, hydro_update);
}

}  // namespace rac1
