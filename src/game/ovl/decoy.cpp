// Ported overlay code: Decoy Glove (item 0x19, hand class 0x232, decoy classes 0xcb / gold 0x76c).
#include <game/ovl/decoy.h>

#include <game/enhance/advanced.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t PAD = 0x0013c940u;
constexpr uint32_t HERO_MAT = 0x0013f990u;
constexpr uint32_t GOLD_DECOY = 0x0013e539u;
}  // namespace

// orig: 0x002c3888
void decoy_2c3888(uint32_t p0, uint32_t p1, uint32_t p2) {
    const uint32_t dpv = lwu(p1 + 0x78);
    V(dpv + 0x10) = V(p2);

    const uint32_t plat = lwu(P + 0x2fc);
    if (plat != 0 && moby_pvar_word2(plat) != 0) {
        vec3_add(V(dpv + 0x10), V(dpv + 0x10), V(P + 0x100));
    }

    V(p1 + 0x10) = V(p0);
    const uint32_t hero = lwu(P + 0x2080);
    Vec4 start = V(hero + 0x10);
    start.z = lwc1(p0 + 0x8);

    vec3_sub(V(p0), V(p0), start);
    vec3_set_length(V(p0), V(p0), fpu_add(vec3_length(V(p0)), u2f(0x3e4ccccdu)));
    vec3_add(V(p0), V(p0), start);

    if (segment_collide_v(start, V(p0), 0, lwu(P + 0x2080), nullptr) == 0) {
        sb(p1 + 0x20, 2);
        return;
    }

    V(p1 + 0x10) = V(OV(0x00173e60u));
    vec_clear(V(dpv + 0x10));
    sb(p1 + 0x20, 4);
    sh(dpv + 0x44, static_cast<uint32_t>(frame_scale_i(0x1e)));
    swc1(p1 + 0x18, lwc1(OV(0x00173e40u) + 0x28));

    const Vec4 nrm = V(OV(0x00173e40u) + 0x40);
    const float k01 = u2f(0x3dcccccdu);
    float c = cos_vu(lwc1(p1 + 0x48));
    float s = sin_vu(lwc1(p1 + 0x48));
    const float rx = fpu_add(fpu_mul(nrm.x, c), fpu_mul(nrm.y, s));
    c = cos_vu(lwc1(p1 + 0x48));
    s = sin_vu(lwc1(p1 + 0x48));
    const float ry = fpu_sub(fpu_mul(nrm.y, c), fpu_mul(nrm.x, s));
    const float rz = nrm.z;
    const float h = sqrt_f(fpu_add(fpu_mul(rx, rx), fpu_mul(rz, rz)));

    swc1(dpv + 0x4c, -atan2_f(h, ry));
    swc1(dpv + 0x50, atan2_f(rz, rx));

    const float d0 = angle_sub(lwc1(dpv + 0x4c), lwc1(p1 + 0x40));
    swc1(p1 + 0x40, angle_add(lwc1(p1 + 0x40), fpu_mul(d0, fpu_mul(lwc1(0x0015ed60u), k01))));

    const float d1 = angle_sub(lwc1(dpv + 0x50), lwc1(p1 + 0x44));
    swc1(p1 + 0x44, angle_add(lwc1(p1 + 0x44), fpu_mul(d1, fpu_mul(lwc1(0x0015ed60u), k01))));
}

// orig: 0x002c35c0
int32_t decoy_2c35c0(uint32_t p0, uint32_t p1, uint32_t p2) {
    const uint32_t cls = (lbu(GOLD_DECOY) == 0) ? 0xcbu : 0x76cu;
    const uint32_t d = moby_spawn(cls);
    if (d == 0) return 0;

    sb(d + 0x30, 0xff);
    sh(d + 0x32, 0xff);
    sb(d + 0x31, 1);

    const uint32_t dpv = lwu(d + 0x78);
    sb(d + 0x20, 0);
    V(d + 0x10) = V(p1);
    V(dpv + 0x10) = V(p2);

    sw(dpv + 0x40, p0);
    sh(dpv + 0x44, 0);
    const float scale_fac = fpu_add(fpu_mul(lwc1(0x0015ed60u), -0.5f), 1.0f);
    sw(dpv + 0x54, 0);
    swc1(d + 0x2c, fpu_mul(lwc1(lwu(d + 0x24) + 0x24), scale_fac));

    const float hp = fpu_mul(fpu_add(int_to_float(static_cast<int32_t>(lbu(GOLD_DECOY))), 1.0f), 4.0f);
    swc1(dpv + 0x48, hp);

    if (lbu(d + 0x53) != 0) {
        moby_anim_set(d, 0, 0, frame_scale_i(0));
    }

    sw(d + 0x94, 0);
    sw(d + 0x98, 0xffffffffu);
    swc1(d + 0x48, rand_range_f(u2f(0xc0490fdbu), u2f(0x40490fdbu)));
    sw(dpv + 0x58, 0);
    sw(dpv + 0x5c, 0);

    if (lh(OV(0x001b0830u)) >= 6) {
        const uint32_t list = OV(0x001b0830u);
        int32_t count = lh(list);
        do {
            uint32_t oldest = 0;
            int32_t oldest_age = 0;
            if (count > 0) {
                const uint32_t pool = lwu(OV(0x0015ffd8u));
                int32_t rem = lh(list);
                uint32_t ep = list + 2u;
                do {
                    const uint32_t cand = pool + (static_cast<uint32_t>(lh(ep)) << 8);
                    --rem;
                    if (cand != 0) {
                        const int32_t ccls = lh(cand + 0xa6);
                        if ((ccls == 0xcb || ccls == 0x76c) && lbu(cand + 0x20) < 5u) {
                            const int32_t age = lw(lwu(cand + 0x78) + 0x54);
                            if (!(age < oldest_age)) {
                                oldest = cand;
                                oldest_age = age;
                            }
                        }
                    }
                    ep += 2u;
                } while (rem != 0);
            }
            if (oldest != 0) {
                moby_list_remove(oldest, OV(0x001b0830u));
                sb(oldest + 0x20, 6);
            }
            count = lh(OV(0x001b0830u));
        } while (!(lh(OV(0x001b0830u)) < 6));
    }

    moby_update_transform(d);
    moby_copy_hero_color(d);
    sh(dpv + 0xc, 4);
    sh(dpv + 0x0, 0);
    sb(dpv + 0x8, 0);
    sb(dpv + 0x9, 0);
    sh(dpv + 0xe, 0xf);
    sb(dpv + 0x7, 0x80);
    sub_0025f780(d, OV(0x001b0830u), 0x1f);

    if (lbu(P + 0x20a5) != 0 || lbu(P + 0x20af) != 0) {
        sh(d + 0x34, lhu(d + 0x34) | 0x41u);
    }
    return static_cast<int32_t>(d);
}

// orig: 0x002d1f30
void decoy_2d1f30(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14) {
    (void)pf13;
    const uint32_t tgt = ee_stack_alloc(0x10);

    const float dist = vec3_distance(V(p0), V(p1));
    if (pf14 < dist) {
        vec3_sub(V(tgt), V(p1), V(p0));
        vec3_set_length(V(tgt), V(tgt), fpu_sub(pf14, u2f(0x3c23d70au)));
        vec3_add(V(tgt), V(tgt), V(p0));
    } else {
        V(tgt) = V(p1);
    }

    float yaw = atan2_f(fpu_sub(lwc1(tgt + 0x0), lwc1(p0 + 0x0)), fpu_sub(lwc1(tgt + 0x4), lwc1(p0 + 0x4)));
    float dt;
    if (u2f(0x3f5f66f3u) < angle_dist(lwc1(P + 0x98), yaw) || vec2_distance(V(p0), V(tgt)) < 1.5f) {
        yaw = lwc1(P + 0x98);
        Vec4 near_pt{fpu_mul(cos_vu(yaw), 1.5f), fpu_mul(sin_vu(lwc1(P + 0x98)), 1.5f), 0.0f, 0.0f};
        vec3_add(near_pt, near_pt, V(p0));
        swc1(tgt + 0x0, near_pt.x);
        swc1(tgt + 0x4, near_pt.y);
        dt = lwc1(0x0015ed6cu);
    } else {
        dt = lwc1(0x0015ed6cu);
    }

    const float hspd = fpu_mul(dt, u2f(0x41080000u));
    swc1(p2 + 0x0, fpu_mul(cos_vu(yaw), hspd));
    swc1(p2 + 0x8, 0.0f);
    swc1(p2 + 0x4, fpu_mul(sin_vu(yaw), hspd));
    swc1(p2 + 0x8, sub_0025abf0(p0, tgt, nullptr, hspd, -pf12));

    const float range_cap = ((lwu(PAD + 0x1a0) & 5u) == 0) ? u2f(0x40200000u) : u2f(0x40600000u);
    const float max_vz = fpu_mul(fpu_mul(pf12, fpu_div(range_cap, hspd)), 0.5f);
    if (max_vz < lwc1(p2 + 0x8)) {
        swc1(p2 + 0x8, max_vz);
    }

    ee_stack_free(0x10);
}

// orig: 0x002d2158
void decoy_2d2158(uint32_t p0, uint32_t p1, uint32_t p2) {
    const uint32_t buf = ee_stack_alloc(0x20);
    const uint32_t from = buf;
    const uint32_t target = buf + 0x10;

    V(from) = V(p2);
    Mat4 cam_m{}, cam_t{};
    mat3_copy(cam_m, M(OV(0x00166c80u)));
    mat3_transpose(cam_t, cam_m);

    const uint32_t decoy = lwu(p1 + 0x50);
    if (decoy == 0) {
        ee_stack_free(0x20);
        return;
    }

    const uint32_t dpv = lwu(decoy + 0x78);
    Vec4 fwd{1.0f, 0.0f, 0.0f, 0.0f};
    mat3_transform(fwd, fwd, M(HERO_MAT));

    float side_ang = angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
    swc1(from + 0x0, fpu_mul(cos_vu(side_ang), u2f(0x3f5be9fbu)));
    side_ang = angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
    swc1(from + 0x8, 0.0f);
    swc1(from + 0x4, fpu_mul(sin_vu(side_ang), u2f(0x3f5be9fbu)));
    vec3_add(V(from), V(from), V(P + 0x80));
    swc1(from + 0x8, fpu_add(lwc1(from + 0x8), u2f(0x3ec8198fu)));
    (void)atan2_f(fwd.x, fwd.y);

    // enhancement: the modern aim takes the original's L1 / L2 camera aim, as the other gloves (advanced.h)
    const float aim_dist0 = advanced_camera_aim(lwu(PAD + 0x1a0)) ? u2f(0x40600000u) : u2f(0x40200000u);
    vec3_scale(V(target), fwd, aim_dist0);
    vec3_add(V(target), V(target), V(from));

    if (!advanced_camera_aim(lwu(PAD + 0x1a0)) || lbu(P + 0x20ac) != 0) {
        sw(0x0013fda0u, 0);
    } else {
        Vec4 dir{0.0f, 0.0f, 1.0f, 0.0f};
        mat4_transform(dir, dir, cam_t);
        int32_t hit_ok = 0;
        (void)angle_sub(lwc1(lwu(P + 0x2080) + 0x48), atan2_f(dir.x, dir.y));
        float cam_yaw = atan2_f(dir.x, dir.y);
        float pitch = atan2_f(vec2_length(dir), dir.z);
        if (u2f(0x3f17e9d8u) < pitch) pitch = u2f(0x3f17e9d8u);
        if (pitch < u2f(0xbfb2b8c2u)) pitch = u2f(0xbfb2b8c2u);

        const float sph_r = advanced_camera_aim(lwu(PAD + 0x1a0)) ? u2f(0x40600000u) : u2f(0x40200000u);
        vec_from_spherical_v(dir, sph_r, cam_yaw, pitch);

        const float hlen = vec2_length(dir);
        const float slope = (hlen == 0.0f) ? fpu_div(dir.z, u2f(0x3c23d70au)) : fpu_div(dir.z, hlen);

        const float g = fpu_mul(lwc1(0x0015ed70u), u2f(0x41100000u));
        const float r_aim = advanced_camera_aim(lwu(PAD + 0x1a0)) ? u2f(0x40600000u) : u2f(0x40200000u);
        const float s_half = sqrt_f(fpu_mul(fpu_mul(g, r_aim), 0.5f));
        const float num = fpu_mul(fpu_mul(fpu_add(s_half, s_half), s_half), fpu_sub(1.0f, slope));
        const float reach = fpu_div(num, fpu_mul(lwc1(0x0015ed70u), u2f(0x41100000u)));
        const float reach_z = fpu_mul(slope, reach);

        if (0.0f < reach) {
            dir.x = fpu_mul(cos_vu(cam_yaw), reach);
            dir.z = reach_z;
            dir.y = fpu_mul(sin_vu(cam_yaw), reach);

            Vec4 ray_end{};
            vec3_add(ray_end, V(OV(0x00166dc0u)), dir);
            if (segment_collide_v(V(OV(0x00166dc0u)), ray_end, 2u, p0, nullptr) != 0) {
                if (lwu(OV(0x00173e40u) + 0x18) != lwu(0x001413d0u)) {
                    V(target) = V(OV(0x00173e40u) + 0x20);
                    hit_ok = 1;
                    cam_yaw = atan2_f(fpu_sub(lwc1(target + 0x0), lwc1(from + 0x0)),
                                      fpu_sub(lwc1(target + 0x4), lwc1(from + 0x4)));
                }
            }
            if (hit_ok == 0) {
                swc1(target + 0x0, fpu_mul(fpu_mul(cos_vu(cam_yaw), reach), 5.0f));
                swc1(target + 0x8, fpu_mul(reach_z, 5.0f));
                swc1(target + 0x4, fpu_mul(fpu_mul(sin_vu(cam_yaw), reach), 5.0f));
                vec3_add(V(target), V(target), V(OV(0x00166dc0u)));
            }
        }
    }

    const float g_final = fpu_mul(lwc1(0x0015ed70u), u2f(0x41100000u));
    const float max_d = advanced_camera_aim(lwu(PAD + 0x1a0)) ? u2f(0x40600000u) : u2f(0x40200000u);
    decoy_2d1f30(from, target, dpv + 0x10, g_final, u2f(0x3727c5acu), max_d);

    ee_stack_free(0x20);
}

// orig: 0x002d2670
void decoy_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    const uint32_t tmp = ee_stack_alloc(0x10);

    Vec4 chain_pt{};
    moby_chain_point_v(m, 0, chain_pt);
    Mat4 chain_m{};
    moby_chain_world_one_v(m, 0, chain_m);
    const Vec4 local_off{0.0f, u2f(0xbe428f5cu), u2f(0xbde147aeu), 0.0f};
    Vec4 world_off{};
    mat4_transform(world_off, local_off, chain_m);
    vec3_add(V(pv + 0x40), chain_pt, world_off);

    uint32_t held = lwu(pv + 0x50);
    if (held != 0) {
        sw(held + 0x98, 1u);
        if (lbu(GOLD_DECOY) != 0 && lh(lwu(pv + 0x50) + 0xa6) != 0x76c) {
            vec_clear(V(tmp));
            moby_list_remove(lwu(pv + 0x50), OV(0x001b0830u));
            moby_retire(lwu(pv + 0x50));
            sw(pv + 0x50, static_cast<uint32_t>(decoy_2c35c0(m, pv + 0x40, tmp)));
        }
        held = lwu(pv + 0x50);
        if (held == 0) {
            sw(pv + 0x50, 0);
        } else {
            const uint32_t st = lbu(held + 0x20);
            if (st == 0xfeu || st == 0xfdu) {
                sw(pv + 0x50, 0);
            }
        }
    } else {
        sw(pv + 0x50, 0);
    }

    if (lw(P + 0x2084) == 1) {
        (void)hero_set_state(0x1e, 1);
    }

    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x54))) != 0 && lbu(P + 0x20ac) == 0) {
        bool fire = false;
        if (lbu(P + 0x20a8) != 0 && lw(P + 0x1bc) == frame_scale_i(0x11)) {
            fire = true;
        } else {
            if ((lwu(PAD + 0x1a4) & lwu(P + 0x10a0)) != 0 && lw(P + 0x2084) == 0x1e && sub_00233e98(-1) != 0) {
                fire = true;
            } else if (lw(P + 0x2084) == 0x23 && lw(P + 0x198) == frame_scale_i(0x10)) {
                fire = true;
            }
        }
        if (fire) {
            (void)hero_sound_start(0x1a, 0);
            (void)sub_00233db8(-1, 1);
            sb(m + 0x20, 3);
        }
    }

    V(tmp) = V(pv + 0x40);
    decoy_2d2158(m, pv, tmp);

    const uint32_t st = lbu(m + 0x20);
    if (st < 7u) {
        switch (ovl_code_to_veldin(lwu(OV(0x001e9ee0u) + (st << 2)))) {
        case 0x002d28c0u:
            sw(pv + 0x50, 0);
            sb(m + 0x20, (lbu(m + 0x70) & 2u) != 0 ? 2 : 1);
            [[fallthrough]];
        case 0x002d28dcu:
            if ((lbu(m + 0x70) & 2u) != 0) {
                sb(m + 0x20, 2);
            }
            break;
        case 0x002d2a08u:
            break;
        case 0x002d28f4u:
            if (lwu(pv + 0x50) != 0) {
                constexpr uint32_t STATS = 0x00141680u;
                const uint32_t cnt = lhu(STATS + 0xc8);
                if (cnt <= 0xfffeu) {
                    sh(STATS + 0xc8, cnt + 1u);
                }
                const int32_t mins = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
                if (static_cast<int32_t>(lhu(STATS + 0xca)) < mins) {
                    sh(STATS + 0xca, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258)));
                }
                const uint32_t lvl = lwu(0x0015ed84u);
                sw(STATS + 0xcc, lwu(STATS + 0xcc) | (1u << (lvl & 31u)) | 0x80000000u);

                const uint32_t dec = lwu(pv + 0x50);
                decoy_2c3888(pv + 0x40, dec, lwu(dec + 0x78) + 0x10);
                sw(pv + 0x50, 0);
            } else {
                (void)moby_sound_start(0, 0, m);
            }
            sb(m + 0x20, 4);
            sw(pv + 0x54, static_cast<uint32_t>(frame_scale_i(0x14)));
            break;
        case 0x002d29d8u:
            if (lw(P + 0x2084) != 0x23 && lbu(P + 0x20a8) == 0) {
                sb(m + 0x20, 2);
            }
            break;
        case 0x002d2a00u:
            sb(m + 0x20, 6);
            break;
        case 0x002d2a64u:
            ee_stack_free(0x10);
            return;
        default:
            break;
        }
    }

    const uint32_t held_now = lwu(pv + 0x50);
    if (held_now != 0) {
        V(held_now + 0x10) = V(pv + 0x40);
    } else if (sub_00233e98(-1) != 0 || lbu(P + 0x20a8) != 0) {
        vec_clear(V(tmp));
        sw(pv + 0x50, static_cast<uint32_t>(decoy_2c35c0(m, pv + 0x40, tmp)));
    }

    ee_stack_free(0x10);
}

// orig: (none)
void decoy_fns_register() {
    ovl_register_moby_fn(0x002d2670, decoy_update);
}

}  // namespace rac1
