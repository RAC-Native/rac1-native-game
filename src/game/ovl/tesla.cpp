// Ported overlay code: Tesla Claw weapon moby update, target selection, beam simulation, and hit recording.
#include <game/ovl/tesla.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

#include <cstring>

namespace rac1 {

// orig: 0x002b9310 — reset the Tesla Claw's beam arrays and side-arc timers when 0x161590 is set.
void tesla_2b9310(uint32_t p0) {
    const uint32_t pv = lwu(p0 + 0x78);
    if (lw(OV(0x00161590u)) == 0) return;

    sw(OV(0x00161590u), 0);
    sw(OV(0x0016167cu), 0);
    sw(OV(0x00161678u), static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161624u)))));
    sw(OV(0x001615f8u), 0x007f2020u);

    Vec4 diff{};
    vec3_sub(diff, V(pv + 0x10), V(pv));
    Vec4 step{};
    vec3_set_length(step, diff, fpu_div(lwc1(OV(0x00161594u)), 20.0f));

    const uint32_t pts = OV(0x001db5c0u);
    V(pts) = V(pv);

    uint32_t z0_ptr = OV(0x001db840u) + 4;
    uint32_t jv_ptr = OV(0x001db890u) + 0x10;
    uint32_t z1_ptr = OV(0x001db9d0u) + 4;
    uint32_t prev_pt = pts;
    uint32_t dst_pt = pts + 0x10;
    for (int32_t k = 18; k >= 0; --k) {
        vec3_add(V(dst_pt), V(prev_pt), step);
        prev_pt += 0x10;
        dst_pt += 0x10;
        sw(z0_ptr, 0);
        vec_clear(V(jv_ptr));
        jv_ptr += 0x10;
        sw(z1_ptr, 0);
        z0_ptr += 4;
        z1_ptr += 4;
    }

    for (int32_t k = 3; k >= 0; --k) {
        sh(OV(0x0016165eu) - static_cast<uint32_t>((3 - k) * 2), 0xffffu);
    }

    sh(OV(0x0016166cu), 8);
    sh(OV(0x0016166eu), 4);
    sh(OV(0x00161676u), 2);
    sh(OV(0x00161668u), 8);
    sh(OV(0x00161670u), 4);
    sh(OV(0x0016166au), 4);
    sh(OV(0x00161672u), 2);
    sh(OV(0x00161674u), 4);
    sw(OV(0x001615acu), 0);
    sw(OV(0x001615b4u), 0);
    sw(OV(0x001615c8u), 0);
}

// orig: 0x002b94d0 — pick primary/secondary targets, advance the two 20-point beams and 4 side arcs,
// collide segments, apply shock hits, and spawn muzzle/impact spark particles.
void tesla_2b94d0(uint32_t p0) {
    constexpr uint32_t P = g::PLAYER;
    const uint32_t gold = lbu(0x0013e533u);
    const float gold_f = static_cast<float>(static_cast<int32_t>(gold));
    const uint32_t pv = lwu(p0 + 0x78);

    float step_len = fpu_div(lwc1(OV(0x00161594u)), 20.0f);
    swc1(OV(0x001615acu), angle_add(lwc1(OV(0x001615acu)), lwc1(OV(0x001615b0u))));
    swc1(OV(0x001615b4u), angle_sub(lwc1(OV(0x001615b4u)), lwc1(OV(0x001615b8u))));
    swc1(OV(0x001615c8u), angle_add(lwc1(OV(0x001615c8u)), lwc1(OV(0x001615ccu))));

    uint32_t best1 = 0;
    uint32_t best2 = 0;
    float best1_score = 0.0f;
    float best2_score = 0.0f;

    bool need_retarget = (lwu(pv + 0x20) == 0);
    if (!need_retarget && gold != 0 && lwu(pv + 0x38) == 0) {
        need_retarget = true;
    }
    if (need_retarget) {
        const uint32_t t2 = lwu(pv + 0x38);
        if (t2 != 0) {
            const uint16_t h42 = lhu(pv + 0x42);
            sw(pv + 0x20, t2);
            sh(pv + 0x40, h42);
            sw(pv + 0x38, 0);
            sh(pv + 0x42, 0);
        } else {
            uint32_t list = OV(0x001aba00u);
            uint32_t cand = lwu(list);
            const uint32_t cam = OV(0x00166c80u);
            while (cand != 0) {
                if (cand != lwu(pv + 0x20) && cand != lwu(pv + 0x38)) {
                    const uint32_t cls_ptr = lwu(cand + 0x24);
                    if (cls_ptr != 0 && lh(cls_ptr + 0x46) == 5) {
                        const uint32_t st = lbu(cand + 0x20);
                        if (st != 0xfeu && st != 0xfdu && moby_pvar_word0(cand) != 0) {
                            Vec4 rel{};
                            vec3_sub(rel, V(cand + 0x10), V(pv));
                            const float dist2d = vec2_length(rel);
                            if (!(lwc1(OV(0x00161594u)) < dist2d)) {
                                const float cam_ang = atan2_f(fpu_sub(lwc1(cand + 0x10), lwc1(cam + 0x140)),
                                                              fpu_sub(lwc1(cand + 0x14), lwc1(cam + 0x144)));
                                const float cam_da = angle_dist(lwc1(cam + 0x158), cam_ang);
                                if (!(lwc1(OV(0x0016159cu)) < cam_da)) {
                                    const float hero_ang = atan2_f(fpu_sub(lwc1(cand + 0x10), lwc1(P + 0x80)),
                                                                   fpu_sub(lwc1(cand + 0x14), lwc1(P + 0x84)));
                                    const float hero_da = angle_dist(hero_ang, lwc1(P + 0x98));
                                    if (!(lwc1(OV(0x00161598u)) < hero_da)) {
                                        const float max_cam_ang = lwc1(OV(0x0016159cu));
                                        const float max_dist = lwc1(OV(0x00161594u));
                                        const float ang_w = lwc1(OV(0x0016162cu));
                                        const float score = fpu_add(
                                            fpu_mul(fpu_div(fpu_sub(max_cam_ang, cam_da), max_cam_ang), ang_w),
                                            fpu_div(fpu_sub(max_dist, dist2d), max_dist));
                                        if (best1_score < score) {
                                            best2_score = best1_score;
                                            best2 = best1;
                                            best1_score = score;
                                            best1 = cand;
                                        } else if (best2_score < score) {
                                            best2_score = score;
                                            best2 = cand;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                list += 4;
                cand = lwu(list);
            }
        }
    }

    if (best1 != 0) {
        if (lwu(pv + 0x20) == 0) {
            sw(pv + 0x20, best1);
            sh(pv + 0x40, 0);
            moby_sound_start(1, 0, p0);
        } else {
            best2 = best1;
        }
    }

    if (best2 != 0 && gold != 0) {
        const float d1 = vec3_distance(V(pv), V(best1 + 0x10));
        if (d1 < lwc1(OV(0x00161594u))) {
            const float d12 = fpu_add(d1, vec3_distance(V(best1 + 0x10), V(best2 + 0x10)));
            if (d12 < lwc1(OV(0x00161594u))) {
                sw(pv + 0x38, best2);
                sh(pv + 0x42, 0);
            }
        }
    }

    Vec4 tgt1_pos{};
    Vec4 tgt2_pos{};
    float dist_to_t1 = 0.0f;
    const uint32_t cur_t1 = lwu(pv + 0x20);
    if (cur_t1 != 0) {
        const uint32_t pw0 = moby_pvar_word0(cur_t1);
        tgt1_pos = V(lwu(pv + 0x20) + 0x10);
        tgt1_pos.z = fpu_add(tgt1_pos.z, lwc1(pw0 + 0x10));
        dist_to_t1 = vec3_distance(V(pv), tgt1_pos);
        step_len = fpu_mul(dist_to_t1, u2f(0x3d4ccccdu));
    }
    const uint32_t cur_t2 = lwu(pv + 0x38);
    if (cur_t2 != 0) {
        const uint32_t pw0 = moby_pvar_word0(cur_t2);
        tgt2_pos = V(lwu(pv + 0x38) + 0x10);
        tgt2_pos.z = fpu_add(tgt2_pos.z, lwc1(pw0 + 0x10));
        const float total_d = fpu_add(vec3_distance(tgt1_pos, tgt2_pos), dist_to_t1);
        step_len = fpu_mul(total_d, u2f(0x3d4ccccdu));
    }

    sw(OV(0x001615f8u), gold != 0 ? 0x00207f20u : 0x007f2020u);

    Vec4 hit_dir{};
    uint32_t rec[9] = {};
    sub_00259888_v(fpu_add(gold_f, 2.0f), rec, static_cast<int32_t>(p0), 0x210000, hit_dir);
    rec[6] = 5u | (1u << 8) | (static_cast<uint32_t>(lhu(p0 + 0xa6)) << 16);
    sh(pv + 0x3e, 20);
    mobys_in_sphere_v(V(pv), u2f(0x3e99999au), 0x10u, lwu(P + 0x2080), rec);

    Vec4 dir{};
    vec3_sub(dir, V(pv + 0x10), V(pv));
    vec3_set_length(dir, dir, 1.0f);
    Vec4 tmp_vec{};
    if (lwu(pv + 0x20) != 0 || lwu(pv + 0x38) != 0) {
        vec3_cross_ba(tmp_vec, dir, V(P + 0x290));
        vec_rotate_axis(dir, dir, tmp_vec, u2f(0x3e32b8c2u));
    }

    Vec4 step_vec{};
    vec3_scale(step_vec, dir, step_len);

    const uint32_t pts0 = OV(0x001db5c0u);
    const uint32_t pts1 = pts0 + 0x140;
    V(pts0) = V(pv);
    V(pts1) = V(pv);

    int32_t seg_hit = 0;
    int32_t saved_count = 0;
    uint32_t saved_mobys[20] = {};
    Vec4 blended{};
    Vec4 seek_vec{};
    Vec4 off_vec{};

    for (int32_t i = 1; i < 20; ++i) {
        const float fi = static_cast<float>(i);
        const float t_i = fpu_mul(int_to_float(i), u2f(0x3d4ccccdu));
        const float amp_lo = lwc1(OV(0x001615a4u));
        const float amp = fpu_add(amp_lo, fpu_mul(fpu_sub(lwc1(OV(0x001615a8u)), amp_lo), t_i));
        const float ph = angle_wrap_mod(fpu_add(lwc1(OV(0x001615acu)), fpu_mul(fi, lwc1(OV(0x001615a0u)))));
        const float z_wave = fpu_add(fpu_mul(amp, sin_vu(ph)),
                                     rand_range_signed_f(lwc1(OV(0x001615e4u)), lwc1(OV(0x001615e8u))));

        const uint32_t z0_slot = OV(0x001db840u) + static_cast<uint32_t>(i * 4);
        const uint32_t pt_cur = pts0 + static_cast<uint32_t>(i * 0x10);
        const uint32_t pt_prev = OV(0x001db5b0u) + static_cast<uint32_t>(i * 0x10);

        const float z_unnudged = fpu_sub(lwc1(pt_cur + 8), lwc1(z0_slot));
        swc1(z0_slot, z_wave);
        swc1(pt_cur + 8, z_unnudged);

        vec3_sub(tmp_vec, V(pt_cur), V(pt_prev));
        vec3_lerp(blended, tmp_vec, step_vec, u2f(0x3f59999au));
        const float bl_len = vec3_length(blended);
        if (bl_len == 0.0f) {
            V(pt_cur) = V(pt_prev);
        } else {
            vec3_scale(blended, blended, fpu_div(step_len, bl_len));
            vec3_add(V(pt_cur), V(pt_prev), blended);
            step_vec = blended;
        }
        swc1(pt_cur + 8, fpu_add(lwc1(pt_cur + 8), z_wave));

        vec3_sub(tmp_vec, V(pt_cur), V(pt_prev));
        bool first_half = (i < 10);

        if (lwu(pv + 0x20) != 0) {
            if (gold == 0 || lwu(pv + 0x38) == 0) {
                vec3_sub(seek_vec, tgt1_pos, V(pt_cur));
                const float rem_len = vec3_length(seek_vec);
                if (rem_len != 0.0f) {
                    float blend_w;
                    if (i == 19) {
                        blend_w = 0.5f;
                        step_len = rem_len;
                    } else if (i < 16) {
                        blend_w = u2f(0x3dcccccdu);
                    } else {
                        blend_w = 0.5f;
                        step_len = fpu_div(rem_len, int_to_float(19 - i));
                    }
                    first_half = (i < 10);
                    vec3_scale(seek_vec, seek_vec, fpu_div(step_len, rem_len));
                    vec3_lerp(step_vec, step_vec, seek_vec, blend_w);
                }
            } else {
                vec3_sub(seek_vec, first_half ? tgt1_pos : tgt2_pos, V(pt_cur));
                const float rem_len = vec3_length(seek_vec);
                if (rem_len != 0.0f) {
                    float blend_w;
                    if (i == 9 || i == 19) {
                        blend_w = u2f(0x3f333333u);
                        step_len = rem_len;
                    } else if (static_cast<uint32_t>(i - 5) < 5u) {
                        blend_w = u2f(0x3f333333u);
                        step_len = fpu_div(rem_len, int_to_float(9 - i));
                    } else if (i < 15) {
                        blend_w = u2f(0x3e4ccccdu);
                    } else {
                        blend_w = u2f(0x3f333333u);
                        step_len = fpu_div(rem_len, int_to_float(19 - i));
                    }
                    vec3_scale(seek_vec, seek_vec, fpu_div(step_len, rem_len));
                    vec3_lerp(step_vec, step_vec, seek_vec, blend_w);
                }
            }
        }

        const uint32_t pt1_cur = pts1 + static_cast<uint32_t>(i * 0x10);
        const uint32_t jv_slot = OV(0x001db890u) + static_cast<uint32_t>(i * 0x10);
        if (first_half) {
            const float ph_b = angle_wrap_mod(fpu_add(lwc1(OV(0x001615b4u)), fpu_mul(fi, lwc1(OV(0x001615bcu)))));
            const float ph_c = angle_wrap_mod(fpu_add(lwc1(OV(0x001615c8u)), fpu_mul(fi, lwc1(OV(0x001615d0u)))));
            const float env = fpu_mul(lwc1(OV(0x001615c0u)), sin_vu(ph_c));
            V(pt1_cur) = V(pt_cur);
            const float sin_b = sin_vu(ph_b);
            const float r_jit = rand_range_signed_f(0.0f, u2f(0x3dcccccdu));
            const float z_off = fpu_add(fpu_mul(env, sin_b), r_jit);
            swc1(OV(0x001db9d0u) + static_cast<uint32_t>(i * 4), z_off);
            swc1(pt1_cur + 8, fpu_add(lwc1(pt1_cur + 8), z_off));

            const float perp_ang = angle_add(atan2_f(tmp_vec.x, tmp_vec.y), u2f(0x3fc90fdbu));
            const float xy_rad = fpu_mul(lwc1(OV(0x001615c4u)), cos_vu(ph_b));
            off_vec.x = fpu_mul(cos_vu(perp_ang), xy_rad);
            off_vec.z = 0.0f;
            off_vec.y = fpu_mul(sin_vu(perp_ang), xy_rad);
            V(jv_slot) = off_vec;
            vec3_add(V(pt1_cur), V(pt1_cur), off_vec);
        } else {
            const int32_t mir = 21 - i;
            V(pt1_cur) = V(pt_cur);
            const float z_off = lwc1(OV(0x001db9d0u) + static_cast<uint32_t>(mir * 4));
            swc1(pt1_cur + 8, fpu_add(lwc1(pt1_cur + 8), z_off));
            vec3_add(V(pt1_cur), V(pt1_cur), V(OV(0x001db890u) + static_cast<uint32_t>(mir * 0x10)));
            off_vec = V(jv_slot);
        }

        if (i != 1 && i != 19 && (i & 1) != 0) {
            continue;
        }

        if (i == 1) {
            const uint32_t hero_moby = lwu(P + 0x2080);
            Vec4 half_vel{};
            vec3_scale(half_vel, V(hero_moby + 0xe0), 0.5f);
            vec3_add(seek_vec, V(P + 0x80), half_vel);
            seg_hit = segment_collide_v(seek_vec, V(pv), 0x10u, hero_moby, nullptr);
        } else {
            const uint32_t pt_prev2 = OV(0x001db5a0u) + static_cast<uint32_t>(i * 0x10);
            seg_hit = segment_collide_v(V(pt_prev2), V(pt_cur), 0x10u, p0, nullptr);
        }

        vec3_sub(hit_dir, V(pt_cur), V(OV(0x00166dc0u)));
        hit_dir.z = 0.0f;
        const float hd_len = vec3_length(hit_dir);
        if (hd_len == 0.0f) {
            hit_dir.z = 1.0f;
        } else {
            vec3_scale(hit_dir, hit_dir, fpu_div(1.0f, hd_len));
        }
        hit_dir.w = u2f(0x45afdf66u);
        std::memcpy(rec, &hit_dir, 16);

        if (seg_hit != 0) {
            const uint32_t hm = lwu(OV(0x00173e58u));
            if (hm != 0) {
                const uint32_t h_pw0 = moby_pvar_word0(hm);
                const uint32_t h_cls = (h_pw0 != 0) ? lwu(hm + 0x24) : 0u;
                if (h_cls != 0 && lh(h_cls + 0x46) == 5) {
                    saved_mobys[saved_count++] = hm;
                    seg_hit = 0;
                    sw(hm + 0x94, 0);
                } else {
                    moby_hit_record_v(hm, reinterpret_cast<const uint8_t*>(rec));
                }
            }
            if (seg_hit != 0) {
                const Vec4 hit_pt = V(OV(0x00173e60u));
                V(pt_cur) = hit_pt;
                sh(pv + 0x3e, static_cast<uint32_t>(i));
                V(pv + 0x10) = hit_pt;
                const uint32_t hm2 = lwu(OV(0x00173e58u));
                if (hm2 == 0) {
                    sw(pv + 0x20, 0);
                    sh(pv + 0x40, 0);
                    break;
                }
                if (moby_coll_flag8(hm2) != 0) {
                    sw(pv + 0x20, 0);
                    sh(pv + 0x40, 0);
                }
                break;
            }
        }
    }

    for (int32_t k = 0; k < saved_count; ++k) {
        const uint32_t sm = saved_mobys[k];
        sw(sm + 0x94, lwu(lwu(sm + 0x24) + 0x10));
    }

    if (seg_hit == 0) {
        V(pv + 0x10) = V(OV(0x001db6f0u));
    }

    const uint32_t hit_t1 = lwu(pv + 0x20);
    if (hit_t1 != 0 && (ld(pv + 0x40) & 0x0000ffff0000ffffull) == 0) {
        vec3_sub(seek_vec, V(hit_t1 + 0x10), V(OV(0x00166dc0u)));
        seek_vec.z = 0.0f;
        vec3_set_length(seek_vec, seek_vec, 1.0f);
        seek_vec.w = u2f(0x45afdf66u);
        uint32_t rec1[9] = {};
        sub_00259888_v(fpu_add(gold_f, 2.0f), rec1, static_cast<int32_t>(p0), 0x210000, seek_vec);
        rec1[6] = 5u | (3u << 8) | (static_cast<uint32_t>(lhu(p0 + 0xa6)) << 16);
        moby_hit_record_v(lwu(pv + 0x20), reinterpret_cast<const uint8_t*>(rec1));
        int32_t cd = 5;
        const uint32_t pw0 = moby_pvar_word0(lwu(pv + 0x20));
        if (pw0 != 0 && lh(pw0 + 4) == 1) cd = 1;
        sh(pv + 0x44, static_cast<uint32_t>(frame_scale_i(cd)));
    }

    const uint32_t hit_t2 = lwu(pv + 0x38);
    if (hit_t2 != 0 && (ld(pv + 0x40) & 0xffff0000ffff0000ull) == 0) {
        vec3_sub(seek_vec, V(hit_t2 + 0x10), V(OV(0x00166dc0u)));
        seek_vec.z = 0.0f;
        vec3_set_length(seek_vec, seek_vec, 1.0f);
        seek_vec.w = u2f(0x45afdf66u);
        uint32_t rec2[9] = {};
        sub_00259888_v(fpu_add(gold_f, 2.0f), rec2, static_cast<int32_t>(p0), 0x210000, seek_vec);
        rec2[6] = 5u | (3u << 8) | (static_cast<uint32_t>(lhu(p0 + 0xa6)) << 16);
        moby_hit_record_v(lwu(pv + 0x38), reinterpret_cast<const uint8_t*>(rec2));
        int32_t cd = 5;
        const uint32_t pw0 = moby_pvar_word0(lwu(pv + 0x38));
        if (pw0 != 0 && lh(pw0 + 4) == 1) cd = 1;
        sh(pv + 0x46, static_cast<uint32_t>(frame_scale_i(cd)));
    }

    for (int32_t arc = 0; arc < 4; ++arc) {
        Vec4 anchor{};
        float arc_scale;
        if (arc < 2) {
            anchor = V(pv);
            arc_scale = u2f(0x3ecccccdu);
        } else {
            anchor = V(pv + 0x10);
            arc_scale = 0.5f;
        }
        const uint32_t h_off = static_cast<uint32_t>(arc * 2);
        const uint32_t timer_addr = OV(0x00161658u) + h_off;
        const uint32_t arc_base = OV(0x001dba20u) + static_cast<uint32_t>(arc * 0x50);

        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(timer_addr))) != 0) {
            sh(timer_addr, static_cast<uint32_t>(frame_scale_i(lh(OV(0x00161668u) + h_off))));
            sh(OV(0x00161660u) + h_off, 0);

            const float rot_side = rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f860a92u));
            const float rot_fwd = rand_range_f(u2f(0xbf860a92u), u2f(0x3e32b8c2u));
            vec3_scale(seek_vec, dir, arc_scale);
            vec_rotate_axis(seek_vec, seek_vec, V(0x0013f9a0u), rot_fwd);
            vec_rotate_axis(seek_vec, seek_vec, V(0x0013f9b0u), rot_side);

            V(arc_base) = anchor;
            vec3_add(V(arc_base + 0x10), anchor, seek_vec);

            float sgn = 1.0f;
            for (int32_t k = 0; k < 3; ++k) {
                const uint32_t p_src = arc_base + static_cast<uint32_t>((k + 1) * 0x10);
                const uint32_t p_dst = arc_base + static_cast<uint32_t>((k + 2) * 0x10);
                Vec4 axis{};
                vec3_sub(axis, V(p_src), V(OV(0x00166dc0u)));
                const float bend = fpu_mul(rand_range_f(u2f(0x3e32b8c2u), u2f(0x3f75be0bu)), sgn);
                sgn = -sgn;
                vec_rotate_axis(seek_vec, seek_vec, axis, bend);
                vec3_add(V(p_dst), V(p_src), seek_vec);
            }
        } else {
            Vec4 diffs[4] = {};
            for (int32_t k = 0; k < 4; ++k) {
                const uint32_t p_prev = arc_base + static_cast<uint32_t>(k * 0x10);
                const uint32_t p_cur = arc_base + static_cast<uint32_t>((k + 1) * 0x10);
                vec3_sub(diffs[k], V(p_cur), V(p_prev));
                diffs[k].x = fpu_add(diffs[k].x, rand_range_signed_f(0.0f, u2f(0x3dcccccdu)));
                diffs[k].y = fpu_add(diffs[k].y, rand_range_signed_f(0.0f, u2f(0x3dcccccdu)));
                diffs[k].z = fpu_add(diffs[k].z, rand_range_signed_f(0.0f, u2f(0x3dcccccdu)));
            }
            V(arc_base) = anchor;
            for (int32_t k = 0; k < 4; ++k) {
                const uint32_t p_prev = arc_base + static_cast<uint32_t>(k * 0x10);
                const uint32_t p_cur = arc_base + static_cast<uint32_t>((k + 1) * 0x10);
                vec3_add(V(p_cur), V(p_prev), diffs[k]);
            }
            const int32_t rem_t = lh(timer_addr);
            const int32_t peak_t = lh(OV(0x00161670u) + h_off);
            const int32_t adiff = (peak_t < rem_t) ? (rem_t - peak_t) : (peak_t - rem_t);
            const float norm = fpu_div(int_to_float(adiff), int_to_float(frame_scale_i(15)));
            const int32_t alpha = float_to_int(fpu_mul(fpu_sub(1.0f, norm), 32.0f));
            sh(OV(0x00161660u) + h_off, static_cast<uint32_t>(alpha));
        }
    }

    const float sp_rot_side = rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f860a92u));
    const float sp_rot_fwd = rand_range_f(u2f(0xbf060a92u), u2f(0x3f860a92u));
    vec3_scale(seek_vec, dir, fpu_mul(lwc1(OV(0x00161628u)), lwc1(0x0015ed60u)));
    vec_rotate_axis(seek_vec, seek_vec, V(0x0013f9a0u), sp_rot_fwd);
    vec_rotate_axis(seek_vec, seek_vec, V(0x0013f9b0u), sp_rot_side);

    int32_t spin1 = rand_range(1, 8);
    if (rand_mod(2) != 0) spin1 = -spin1;
    const float sz1 = rand_range_f(u2f(0x3e4ccccdu), u2f(0x3f19999au));
    const uint32_t life1a = static_cast<uint32_t>(frame_scale_i(15));
    spawn_particle_35_v(V(pv), life1a, lwu(OV(0x001615f8u)) | 0x7f000000u, 0,
                        static_cast<uint32_t>(spin1), seek_vec,
                        fpu_mul(sz1, u2f(0x3e99999au)), sz1, 0.0f);

    spin1 = -spin1;
    const uint32_t life1b = static_cast<uint32_t>(frame_scale_i(15));
    spawn_particle_35_v(V(pv), life1b, 0x307f7f7fu, 0,
                        static_cast<uint32_t>(spin1), seek_vec,
                        fpu_mul(sz1, u2f(0x3e19999au)), fpu_mul(sz1, 0.5f), 0.0f);

    int32_t spin2 = rand_range(1, 8);
    if (rand_mod(2) != 0) spin2 = -spin2;
    const float sz2 = rand_range_f(u2f(0x3e19999au), 3.0f);
    const uint32_t life2 = static_cast<uint32_t>(frame_scale_i(12));
    spawn_particle_35_v(V(pv + 0x10), life2, lwu(OV(0x001615f8u)) | 0x7f000000u, 0,
                        static_cast<uint32_t>(spin2), V(OV(0x0015f580u)),
                        fpu_mul(sz2, u2f(0x3dcccccdu)), sz2, 0.0f);
}

// orig: 0x002b87e0 — Tesla Claw weapon moby update (class update registered at 0x002b87e0).
void tesla_update(uint32_t m) {
    constexpr uint32_t P = g::PLAYER;
    constexpr uint32_t PAD = 0x0013c940u;
    const uint32_t pv = lwu(m + 0x78);

    const bool unequipped = ((lwu(P + 0x10b4) ^ 3u) < 1u);
    if ((lwu(PAD + 0x1a4) & lwu(P + 0x10a0)) != 0 && lbu(P + 0x20ac) == 0) {
        swc1(pv + 0x24, u2f(0x3f4ccccdu));
    }

    swc1(OV(0x0017aebcu), u2f(0x3f666666u));
    const int32_t ammo = sub_00233e98(-1);
    bool firing = false;

    if (lhu(0x00141d70u) == 0 && lhu(0x00141918u) == 0 && lbu(P + 0x20b5) >= 3u) {
        const uint32_t hst = lwu(P + 0x208c);
        if (hst < 2u || hst == 9u) {
            sub_00203908(0x4e30, 0x81);
        }
    }

    if ((lwu(PAD + 0x1a0) & lwu(P + 0x10a0)) == 0) {
        sw(pv + 0x20, 0);
        sw(pv + 0x38, 0);
        sh(pv + 0x40, 0);
        sh(pv + 0x42, 0);
    } else {
        const uint8_t mst = lbu(m + 0x20);
        if (mst == 4u || mst == 0u || lbu(P + 0x20ac) != 0 || lw(P + 0x2084) == 0x72) {
            sw(pv + 0x20, 0);
            sw(pv + 0x38, 0);
            sh(pv + 0x40, 0);
            sh(pv + 0x42, 0);
        } else if (lw(pv + 0x2c) == 0) {
            if (ammo <= 0) {
                sw(pv + 0x20, 0);
                sw(pv + 0x38, 0);
                sh(pv + 0x40, 0);
                sh(pv + 0x42, 0);
                sh(pv + 0x44, 0);
                sh(pv + 0x46, 0);
            } else {
                if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x3c))) != 0) {
                    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x44)));
                    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x46)));
                    sub_00233db8(-1, 1);
                    sh(pv + 0x3c, static_cast<uint32_t>(frame_scale_i(10)));
                }

                const int32_t light_slot = lw(pv + 0x30);
                if (light_slot == -1) {
                    sw(pv + 0x30, static_cast<uint32_t>(sub_0023e5e0(m + 0x10, 0, 7.5f, 0.0f)));
                } else {
                    const int32_t ramp_max = frame_scale_i(10);
                    const int32_t ramp = lw(pv + 0x34);
                    if (ramp < ramp_max) sw(pv + 0x34, static_cast<uint32_t>(ramp + 1));
                }

                firing = true;

                uint32_t t1 = lwu(pv + 0x20);
                if (t1 != 0) {
                    const float ang = atan2_f(fpu_sub(lwc1(t1 + 0x10), lwc1(P + 0x80)),
                                              fpu_sub(lwc1(t1 + 0x14), lwc1(P + 0x84)));
                    if (u2f(0x40060903u) < angle_dist(lwc1(P + 0x98), ang)) {
                        sw(pv + 0x20, 0);
                        sh(pv + 0x40, 0);
                    } else {
                        t1 = lwu(pv + 0x20);
                        if (lbu(t1 + 0x31) == 0 || 20.0f < vec3_distance(V(t1 + 0x10), V(P + 0x80))) {
                            sw(pv + 0x20, 0);
                            sh(pv + 0x40, 0);
                        }
                    }
                }

                uint32_t t2 = lwu(pv + 0x38);
                if (t2 != 0) {
                    const float ang = atan2_f(fpu_sub(lwc1(t2 + 0x10), lwc1(P + 0x80)),
                                              fpu_sub(lwc1(t2 + 0x14), lwc1(P + 0x84)));
                    if (u2f(0x40060903u) < angle_dist(lwc1(P + 0x98), ang)) {
                        sw(pv + 0x38, 0);
                        sh(pv + 0x42, 0);
                    } else {
                        t2 = lwu(pv + 0x38);
                        if (lbu(t2 + 0x31) == 0 || 20.0f < vec3_distance(V(t2 + 0x10), V(P + 0x80))) {
                            sw(pv + 0x38, 0);
                            sh(pv + 0x42, 0);
                        }
                    }
                }

                V(pv) = V(m + 0x10);
                swc1(pv + 8, fpu_add(lwc1(pv + 8), u2f(0x3f333333u)));
                V(pv + 0x10) = V(pv);

                const uint32_t hold_t1 = lwu(pv + 0x20);
                if (hold_t1 != 0 && lh(pv + 0x40) != 0) {
                    sh(pv + 0x28, lhu(pv + 0x28) + 1u);
                    int32_t max_hold = 50;
                    const uint32_t pw0 = moby_pvar_word0(hold_t1);
                    if (pw0 != 0 && lh(pw0 + 4) == 1) max_hold = 5;
                    if (!(lh(pv + 0x28) < frame_scale_i(max_hold))) {
                        sw(pv + 0x20, 0);
                        sh(pv + 0x40, 0);
                    }
                } else {
                    sh(pv + 0x28, 0);
                }

                const uint32_t hold_t2 = lwu(pv + 0x38);
                if (hold_t2 != 0) {
                    if (lh(pv + 0x42) != 0) {
                        sh(pv + 0x2a, lhu(pv + 0x2a) + 1u);
                        int32_t max_hold = 50;
                        const uint32_t pw0 = moby_pvar_word0(hold_t2);
                        if (pw0 != 0 && lh(pw0 + 4) == 1) max_hold = 5;
                        if (!(lh(pv + 0x2a) < frame_scale_i(max_hold))) {
                            sw(pv + 0x38, 0);
                            sh(pv + 0x42, 0);
                        }
                    } else {
                        sh(pv + 0x2a, 0);
                    }
                } else {
                    sh(pv + 0x2a, 0);
                }

                const float target_reach = fpu_add(fpu_mul(int_to_float(lbu(0x0013e533u)), 6.0f), 12.0f);
                const float cur_reach = lwc1(pv + 0x24);
                const float reach_rate = fpu_mul(lwc1(0x0015ed60u), u2f(0x3d8f5c29u));
                swc1(pv + 0x24, fpu_add(cur_reach, fpu_mul(fpu_sub(target_reach, cur_reach), reach_rate)));

                if (lw(P + 0x2084) == 1) {
                    Vec4 off{u2f(0x3f99999au), 0.0f, 0.0f, 0.0f};
                    const uint32_t fp_mat = OV(0x00166fd0u);
                    mat3_transform(off, off, M(fp_mat));
                    const float reach = lwc1(pv + 0x24);
                    swc1(pv + 8, fpu_add(off.z, lwc1(m + 0x18)));
                    swc1(pv + 0, fpu_add(off.x, lwc1(P + 0x80)));
                    swc1(pv + 4, fpu_add(off.y, lwc1(P + 0x84)));
                    off.x = reach;
                    off.y = 0.0f;
                    off.z = 0.0f;
                    mat3_transform(off, off, M(fp_mat));
                    vec3_add(V(pv + 0x10), V(pv), off);
                } else if (lh(P + 0x22c8) == 1) {
                    moby_chain_point_v(m, 0, V(pv));
                    Vec4 off{0.0f, -lwc1(pv + 0x24), 0.0f, 0.0f};
                    mat3_transform(off, off, M(m + 0xc0));
                    vec3_add(V(pv + 0x10), V(pv), off);
                } else {
                    moby_chain_point_v(m, 0, V(pv));
                    vec3_set_length(V(pv + 0x10), V(P + 0x640), lwc1(pv + 0x24));
                    advanced_aim_shot(pv, pv + 0x10);  // enhancement: modern aim line (advanced.h)
                    vec3_add(V(pv + 0x10), V(pv), V(pv + 0x10));
                }

                tesla_2b9310(m);
                tesla_2b94d0(m);
                draw_callback_add(OVF(0x002ba970u), m);
            }
        }
    }

    if (firing) {
        const int32_t snd = lw(pv + 0x48);
        if (sub_0028d8c0(m, snd) == 0) {
            sw(pv + 0x48, static_cast<uint32_t>(moby_sound_start(4, 4, m)));
        }
    } else {
        const int32_t ramp = lw(pv + 0x34);
        sw(OV(0x00161590u), 1);
        if (ramp > 0) sw(pv + 0x34, static_cast<uint32_t>(ramp - 1));
        const int32_t snd = lw(pv + 0x48);
        if (snd != -1) {
            const uint32_t entry = 0x0013e550u + static_cast<uint32_t>(snd) * 0x70u;
            if (lwu(entry + 0x88) == m && lbu(entry + 0x74) != 0) {
                sub_0028d918(snd);
            }
        }
        sw(pv + 0x48, 0xffffffffu);
    }

    const uint32_t chk1 = lwu(pv + 0x20);
    if (chk1 == 0 || lbu(chk1 + 0x20) == 0xfeu || lbu(chk1 + 0x20) == 0xfdu || (lhu(chk1 + 0x34) & 0x1000u) == 0) {
        sh(pv + 0x40, 1);
    }
    const uint32_t chk2 = lwu(pv + 0x38);
    if (chk2 == 0 || lbu(chk2 + 0x20) == 0xfeu || lbu(chk2 + 0x20) == 0xfdu || (lhu(chk2 + 0x34) & 0x1000u) == 0) {
        sh(pv + 0x42, 1);
    }

    const uint8_t state = lbu(m + 0x20);
    if (state < 6u) {
        // (the state's case by its index: the original's switch over the moby's state, compiled to the jump table
        // 0x1e9a90; the disc's table is not read, so the states work on a build whose own code moved the case labels)
        switch (state) {
        case 0: {  // (0x002b8ed4)
            sw(pv + 0x2c, static_cast<uint32_t>(frame_scale_i(0x10)));
            sw(pv + 0x30, static_cast<uint32_t>(sub_0023e5e0(pv, 0, 0.0f, 0.0f)));
            sw(OV(0x00161590u), 1);
            sb(m + 0x20, 1);
            sh(pv + 0x3e, 20);
            sw(pv + 0x48, 0xffffffffu);
            sw(pv + 0x38, 0);
            sw(pv + 0x20, 0);
            sh(pv + 0x40, 0);
            sh(pv + 0x42, 0);
            sh(pv + 0x46, 0);
            sh(pv + 0x44, 0);
            break;
        }
        case 1: {  // (0x002b8f30)
            timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2c)));
            if (firing) {
                hero_hold_start();
                sb(m + 0x20, 3);
            }
            if ((lwu(PAD + 0x1a4) & lwu(P + 0x10a0)) == 0 || lbu(P + 0x20ac) != 0) {
                break;
            }
            if (firing) {
                const uint32_t stat = 0x00141680u;
                const uint16_t cnt = lhu(stat + 0x98);
                int32_t f_in = lw(0x0015eea4u);
                if (cnt <= 0xfffeu) {
                    sh(stat + 0x98, cnt + 1u);
                    f_in = lw(0x0015eea4u);
                }
                const int32_t mins = ee_div(frame_scale_i(f_in), 600);
                uint32_t bit = lwu(0x0015ed84u);
                if (static_cast<int32_t>(lhu(stat + 0x9a)) < mins) {
                    const int32_t mins2 = ee_div(frame_scale_i(lw(0x0015eea4u)), 600);
                    sh(stat + 0x9a, static_cast<uint32_t>(mins2));
                    bit = lwu(0x0015ed84u);
                }
                sw(stat + 0x9c, lwu(stat + 0x9c) | (1u << (bit & 31u)) | 0x80000000u);
                hero_hold_start();
                sb(m + 0x20, 3);
            } else {
                sw(pv + 0x20, 0);
                sw(pv + 0x38, 0);
                sh(pv + 0x40, 0);
                sh(pv + 0x42, 0);
                sb(m + 0x20, 1);
                moby_sound_start(0, 0, m);
                hero_hold_release();
            }
            break;
        }
        case 3: {  // (0x002b9058)
            if ((lwu(PAD + 0x1a8) & lwu(P + 0x10a0)) == 0 && firing && lbu(P + 0x20ac) == 0) {
                break;
            }
            int32_t f_in;
            if (frame_scale_i(0x46) < lw(P + 0x1bc)) {
                const uint32_t stat0 = 0x00141848u;
                const uint16_t cnt = lhu(stat0 + 0xd0);
                if (cnt > 0xfffeu) {
                    f_in = lw(0x0015eea4u);
                } else {
                    sh(stat0 + 0xd0, cnt + 1u);
                    f_in = lw(0x0015eea4u);
                }
            } else {
                f_in = lw(0x0015eea4u);
            }
            const uint32_t stat = 0x00141848u;
            const int32_t mins = ee_div(frame_scale_i(f_in), 600);
            uint32_t bit = lwu(0x0015ed84u);
            if (static_cast<int32_t>(lhu(stat + 0xd2)) < mins) {
                const int32_t mins2 = ee_div(frame_scale_i(lw(0x0015eea4u)), 600);
                sh(stat + 0xd2, static_cast<uint32_t>(mins2));
                bit = lwu(0x0015ed84u);
            }
            sw(stat + 0xd4, lwu(stat + 0xd4) | (1u << (bit & 31u)) | 0x80000000u);
            if (lw(P + 0x1bc) < frame_scale_i(0x1e)) {
                sb(P + 0x20b5, lbu(P + 0x20b5) + 1u);
            } else {
                sb(P + 0x20b5, 0);
            }
            sw(pv + 0x20, 0);
            sw(pv + 0x38, 0);
            sh(pv + 0x40, 0);
            sh(pv + 0x42, 0);
            sw(pv + 0x24, 0);
            hero_hold_release();
            sb(m + 0x20, 1);
            break;
        }
        case 2: case 4: case 5:  // (0x002b9180)
            break;
        }
    }

    if (lw(pv + 0x30) != -1) {
        Vec4 lpos{2.0f, 0.0f, 2.0f, 0.0f};
        mat3_transform(lpos, lpos, M(0x0013f990u));
        const uint32_t lights = OV(0x001802c0u);
        uint32_t slot = static_cast<uint32_t>(lw(pv + 0x30));
        swc1(lights + (slot << 5) + 0x10, fpu_add(lwc1(P + 0x80), lpos.x));
        const float py = fpu_add(lwc1(P + 0x84), lpos.y);
        const float pz = fpu_add(lwc1(P + 0x88), lpos.z);
        slot = static_cast<uint32_t>(lw(pv + 0x30));
        swc1(lights + (slot << 5) + 0x14, py);
        slot = static_cast<uint32_t>(lw(pv + 0x30));
        swc1(lights + (slot << 5) + 0x18, pz);
        slot = static_cast<uint32_t>(lw(pv + 0x30));
        swc1(lights + (slot << 5) + 0x1c, 7.5f);

        float intens = int_to_float(lw(pv + 0x34));
        const float max_ramp = static_cast<float>(frame_scale_i(10));
        slot = static_cast<uint32_t>(lw(pv + 0x30));
        intens = fpu_div(intens, max_ramp);
        swc1(lights + (slot << 5) + 8, intens);
        intens = fpu_mul(intens, u2f(0x3f333333u));
        slot = static_cast<uint32_t>(lw(pv + 0x30));
        swc1(lights + (slot << 5) + 4, intens);
        slot = static_cast<uint32_t>(lw(pv + 0x30));
        const uint32_t entry = lights + (slot << 5);
        swc1(entry + 0, lwc1(entry + 4));

        if (!unequipped) return;

        const uint32_t clear_slot = lwu(pv + 0x30);
        if (clear_slot != 0xffffffffu) {
            region_clear(clear_slot);
            sw(pv + 0x30, 0xffffffffu);
        }
    }

    if (unequipped && lw(pv + 0x30) == -1) {
        sb(m + 0x20, 4);
    }
}

// orig: (none) -- the ports for the engine's indirect calls (a moby class's update, a draw callback)
void tesla_fns_register() {
    ovl_register_moby_fn(0x002b87e0, tesla_update);
}

}  // namespace rac1
