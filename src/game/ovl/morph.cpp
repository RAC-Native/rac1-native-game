// Ported overlay code: the Morph-o-Ray (item 0x15, hand update 0x2bc7e8), target selection (0x2bc3b0),
// beam spine/helices/rings update (0x2bcf98 / 0x2bd0a0), particle 0x4e spawn (0x275910), and chicken
// transformation (0x2c92d8 / 0x2c9348), and class 804 (0x2d7f88, a piece on the hand moby).
#include <game/ovl/morph.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapon_classes.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t CAM_UP = 0x0013f5e0u;
constexpr uint32_t PAD_HELD = 0x0013cae0u;
constexpr uint32_t SND_CHANNELS = 0x0013e550u;
constexpr uint32_t GOLD_MORPH_FLAG = 0x0013e535u;

void morph_275910_v(const Vec4& pos, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, const Vec4& vel, uint32_t p6,
                    float pf12, float pf13) {
    const uint32_t p = alloc_particle(0x4e);
    if (p == 0) return;

    const uint32_t q = p + 0x20u;
    V(p + 0x10u) = pos;
    sw(p + 0x4u, p2);
    sb(p + 0x3u, 0x48u);
    const int32_t four = float_to_int(4.0f);
    sb(p + 0x1u, 0u);
    sb(p + 0x9u, static_cast<uint32_t>(four + 0x20));

    const float k_size = u2f(0x484d1400u);
    const float init_sz = fpu_mul(pf12, k_size);
    const uint32_t tex = lbu(lwu(OV(0x001b21b8u)));
    sh(p + 0xau, p1);
    sb(p + 0x2u, tex);
    swc1(p + 0xcu, init_sz);

    if (p3 == 0u) {
        sb(p + 0x8u, 0u);
    } else if (p3 == 1u) {
        sb(p + 0x8u, 0x20u);
    } else {
        sb(p + 0x8u, static_cast<uint32_t>(rand_mod(0xff)));
    }

    const uint32_t alpha_hi = static_cast<uint32_t>(static_cast<int32_t>(p2) >> 24);
    const float cur_sz = lwc1(p + 0xcu);
    const float end_sz = fpu_mul(pf13, k_size);
    sh(q + 0x8u, p1);
    swc1(q + 0x0u, cur_sz);
    sb(q + 0xau, p4);
    swc1(q + 0x4u, end_sz);
    sb(q + 0xbu, alpha_hi);
    sw(q + 0x18u, p6);
    sw(q + 0x1cu, 0u);
    if (p6 != 0u) {
        const uint32_t tpv = moby_pvar_word0(p6);
        if (tpv != 0u) {
            swc1(q + 0x1cu, lwc1(tpv + 0x10u));
        }
    }
    swc1(q + 0xcu, vel.x);
    swc1(q + 0x10u, vel.y);
    swc1(q + 0x14u, vel.z);
}

int32_t morph_2bc3b0_v(uint32_t p0, const Vec4& from, uint32_t p2, uint32_t p3) {
    float best_dist = lwc1(OV(0x001616dcu));
    const uint32_t hero_moby = lwu(P + 0x2080u);
    Vec4 hero_start = V(hero_moby + 0x10u);
    hero_start.z = from.z;
    if (segment_collide_v(hero_start, from, 6u, hero_moby, nullptr) != 0) {
        return 0;
    }

    uint32_t best_moby = 0u;
    uint32_t list = OV(0x001aba00u);
    uint32_t cand = lwu(list);
    if (cand == 0u) return 0;

    const float deg2rad = u2f(0x3c8efa35u);
    for (; cand != 0u; list += 4u, cand = lwu(list)) {
        if (static_cast<int8_t>(lbu(cand + 0x20u)) < 0) continue;
        const int16_t cls = lh(cand + 0xa6u);
        if (cls == 0x58e || cls == 0x452) continue;

        Vec4 cand_pos = V(cand + 0x10u);
        const uint32_t cpv0 = moby_pvar_word0(cand);
        if (cpv0 == 0u) continue;
        cand_pos.z = fpu_add(cand_pos.z, lwc1(cpv0 + 0x10u));

        const uint32_t ccls = lwu(cand + 0x24u);
        if (ccls == 0u || lh(ccls + 0x46u) != 5) continue;

        const float to_yaw = atan2_f(fpu_sub(cand_pos.x, from.x), fpu_sub(cand_pos.y, from.y));
        const float dist2d = vec2_distance(from, cand_pos);
        const float to_pitch = atan2_f(dist2d, fpu_sub(cand_pos.z, from.z));
        const float yaw_err = angle_dist(lwc1(p2), to_yaw);
        const float dist3d = vec3_distance(from, cand_pos);

        if (dist3d < 2.5f) {
            const float hero_to_cand = atan2_f(fpu_sub(lwc1(cand + 0x10u), lwc1(HERO_POS + 0x0u)),
                                               fpu_sub(lwc1(cand + 0x14u), lwc1(HERO_POS + 0x4u)));
            const uint32_t hm = lwu(P + 0x2080u);
            if (angle_dist(lwc1(hm + 0x48u), hero_to_cand) < u2f(0x3f860a92u) &&
                abs_f(fpu_sub(from.z, cand_pos.z)) < 2.0f &&
                segment_collide_v(from, cand_pos, 6u, lwu(P + 0x2080u), nullptr) == 0) {
                swc1(p3, -to_pitch);
                swc1(p2, to_yaw);
                return static_cast<int32_t>(cand);
            }
        }

        if (best_dist < dist3d) continue;

        const float adj_pitch = fpu_add(lwc1(p3), fpu_mul(lwc1(P + 0x2e4u), 0.5f));
        Vec4 aim_pt{};
        vec_from_spherical_v(aim_pt, dist3d, lwc1(p2), adj_pitch);
        vec3_add(aim_pt, aim_pt, from);

        const float chord = vec3_distance(aim_pt, cand_pos);
        const float half_ang = asin_f(fpu_div(chord, fpu_add(dist3d, dist3d)));
        float cone_ang = fpu_add(half_ang, half_ang);
        if (fpu_mul(lwc1(OV(0x001616d8u)), deg2rad) < cone_ang) {
            const uint32_t cpv = moby_pvar_word0(cand);
            if (cpv != 0u) {
                const float rad = fpu_mul(int_to_float(static_cast<int32_t>(lbu(cpv + 0xau))), 0.125f);
                const float sub_ang = asin_f(fpu_div(rad, dist3d));
                if (rad < dist3d) {
                    cone_ang = fpu_sub(cone_ang, sub_ang);
                    if (cone_ang < 0.0f) {
                        cone_ang = 0.0f;
                    }
                }
            }
        }
        if (!(cone_ang < fpu_mul(lwc1(OV(0x001616d8u)), deg2rad))) continue;

        bool clear = true;
        if (segment_collide_v(from, cand_pos, 6u, p0, nullptr) != 0) {
            const uint32_t hit_moby = lwu(OV(0x00173e40u) + 0x18u);
            if (hit_moby == 0u || hit_moby != cand) {
                clear = false;
            }
        }
        if (!clear) continue;

        best_dist = dist3d;
        swc1(p3, -to_pitch);
        if (yaw_err < u2f(0x3e32b8c2u)) {
            swc1(p2, to_yaw);
        }
        best_moby = cand;
    }

    return static_cast<int32_t>(best_moby);
}
}  // namespace

// orig: 0x002bcf98
void morph_2bcf98(uint32_t p0) {
    const uint32_t reset = lwu(OV(0x001617bcu));
    const uint32_t pv = lwu(p0 + 0x78u);
    if (reset == 0u) return;

    sw(OV(0x001617bcu), 0u);
    const float seg_len = fpu_div(lwc1(OV(0x001616dcu)), int_to_float(12));
    swc1(OV(0x001617ccu), seg_len);

    Vec4 step{};
    vec3_scale(step, V(p0 + 0xd0u), -seg_len);

    const uint32_t spine = OV(0x001dbf60u);
    V(spine) = V(pv);
    for (uint32_t k = 1; k <= 11; ++k) {
        vec3_add(V(spine + k * 0x10u), V(spine + (k - 1u) * 0x10u), step);
    }

    sw(OV(0x00161718u), 0u);
    sw(OV(0x00161720u), 0u);
    sw(OV(0x00161724u), 0u);
    sw(OV(0x001617d0u), 0u);
    sw(OV(0x00161824u), 0u);
    for (uint32_t k = 0; k < 3; ++k) {
        sw(OV(0x00161818u) + k * 4u, 0u);
        V(OV(0x001dd0d0u) + k * 0x10u) = step;
    }
}

// orig: 0x00275910
void morph_275910(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6,
                  float pf12, float pf13) {
    morph_275910_v(V(p0), p1, p2, p3, p4, V(p5), p6, pf12, pf13);
}

// orig: 0x002bd0a0
void morph_2bd0a0(uint32_t p0) {
    const uint32_t pv = lwu(p0 + 0x78u);
    morph_2bcf98(p0);

    const float next_phase = angle_wrap_mod(fpu_add(lwc1(OV(0x001617d0u)), lwc1(OV(0x001616f8u))));
    const float aim_yaw = lwc1(pv + 0x48u);
    const float aim_pitch = lwc1(pv + 0x4cu);
    const float prev_seg_len = lwc1(OV(0x001617ccu));
    swc1(OV(0x001617d0u), next_phase);

    Vec4 aim_step{};
    vec_from_spherical_v(aim_step, prev_seg_len, aim_yaw, aim_pitch);

    Vec4 end_pt{};
    const uint32_t tgt0 = lwu(pv + 0x10u);
    const uint32_t tpv0 = tgt0 != 0u ? moby_pvar_word0(tgt0) : 0u;
    if (tpv0 != 0u) {
        const uint32_t tgt_m = lwu(pv + 0x10u);
        end_pt = V(tgt_m + 0x10u);
        end_pt.z = fpu_add(end_pt.z, lwc1(tpv0 + 0x10u));
    } else {
        vec_from_spherical_v(end_pt, lwc1(OV(0x001616dcu)), lwc1(pv + 0x48u), lwc1(pv + 0x4cu));
        vec3_add(end_pt, end_pt, V(pv));
    }

    float beam_len = lwc1(OV(0x001616dcu));
    const uint32_t tgt1 = lwu(pv + 0x10u);
    if (tgt1 != 0u) {
        beam_len = vec2_distance(V(tgt1 + 0x10u), V(pv));
    } else if (segment_collide_v(V(pv), end_pt, 0x14u, lwu(P + 0x2080u), nullptr) != 0) {
        beam_len = vec2_distance(V(OV(0x00173e60u)), V(pv));
    }

    swc1(OV(0x001617ccu), fpu_mul(beam_len, lwc1(OV(0x001616f0u))));
    V(OV(0x001dbf60u)) = V(pv);
    V(OV(0x001dc020u)) = V(pv);
    V(OV(0x001dc0e0u)) = V(pv);

    Vec4 seg_dir{}, perp_vec{};
    for (int32_t s5 = 1; s5 < 12; ++s5) {
        const uint32_t s2 = static_cast<uint32_t>(s5) << 4;
        const uint32_t cur_node = OV(0x001dbf60u) + s2;
        const uint32_t prev_node = OV(0x001dbf50u) + s2;

        Vec4 prev_diff{};
        vec3_sub(prev_diff, V(cur_node), V(prev_node));

        const float f22 = fpu_mul(int_to_float(s5), lwc1(OV(0x001616f0u)));
        const float f20_lerp = fpu_add(fpu_mul(f22, u2f(0xbeffffffu)), u2f(0x3f666666u));
        if (lw(OV(0x001616f4u)) < s5) {
            const uint32_t tgt = lwu(pv + 0x10u);
            if (tgt != 0u) {
                Vec4 to_tgt{};
                vec3_sub(to_tgt, V(tgt + 0x10u), V(cur_node));
                vec3_set_length(to_tgt, to_tgt, lwc1(OV(0x001617ccu)));
                vec3_lerp(aim_step, aim_step, to_tgt, u2f(0x3e4ccccdu));
            }
        }

        Vec4 blended{};
        vec3_lerp(blended, prev_diff, aim_step, f20_lerp);
        const float blen = vec3_length(blended);
        if (blen == 0.0f) {
            V(cur_node) = V(prev_node);
        } else {
            vec3_scale(blended, blended, fpu_div(lwc1(OV(0x001617ccu)), blen));
            vec3_add(V(cur_node), V(prev_node), blended);
        }

        vec3_sub(seg_dir, V(cur_node), V(prev_node));
        vec3_cross_ba(perp_vec, seg_dir, V(CAM_UP));
        const float r0 = lwc1(OV(0x00161708u));
        const float r1 = lwc1(OV(0x0016170cu));
        const float tube_r = fpu_add(r0, fpu_mul(fpu_sub(r1, r0), f22));
        vec3_set_length(perp_vec, perp_vec, tube_r);

        uint32_t ring_pt = OV(0x001dc1a0u) + static_cast<uint32_t>(s5) * 0x140u;
        for (int32_t k = 0; k < 20; ++k, ring_pt += 0x10u) {
            const float ang = fpu_mul(static_cast<float>(k), lwc1(OV(0x00161704u)));
            Vec4 radial{};
            vec_rotate_axis(radial, perp_vec, seg_dir, ang);
            vec3_add(V(ring_pt), radial, V(cur_node));
        }

        Mat4 frame{};
        frame.r[3] = V(cur_node);
        vec3_set_length(frame.r[0], seg_dir, 1.0f);
        vec3_set_length(frame.r[1], perp_vec, 1.0f);
        vec3_cross_ba(frame.r[2], frame.r[1], frame.r[0]);

        const float h0 = lwc1(OV(0x0016172cu));
        const float h_rad = fpu_add(h0, fpu_mul(fpu_sub(lwc1(OV(0x00161730u)), h0), f22));
        const float h_ang = angle_wrap_mod(fpu_add(lwc1(OV(0x001617d0u)), fpu_mul(lwc1(OV(0x001616fcu)), int_to_float(s5))));
        const float hy = fpu_mul(h_rad, sin_vu(h_ang));
        const float hz = fpu_mul(h_rad, cos_vu(h_ang));

        Vec4 local_pt{0.0f, hy, hz, 1.0f};
        mat4_transform(V(OV(0x001dc020u) + s2), local_pt, frame);
        local_pt.z = -local_pt.z;
        mat4_transform(V(OV(0x001dc0e0u) + s2), local_pt, frame);
    }

    const float k01 = u2f(0x3dcccccdu);
    draw_callback_add(OVF(0x002bdc50u), p0);
    const float f22 = rand_range_f(k01, 2.0f);
    uint32_t spin_dir = rand_mod(2) != 0 ? 1u : 0xffffffffu;

    if (rand_mod(2) == 0) {
        const Vec4 pt = V(OV(0x001dc020u) + 0xb0u);
        const float c_fade = u2f(0x3b449ba6u);
        const uint32_t life1 = static_cast<uint32_t>(frame_scale_i(0x1e));
        (void)spawn_particle_35_v(pt, life1, 0x7f7f2020u, 0, spin_dir, V(OV(0x0015f580u)), fpu_mul(f22, k01), f22, c_fade);
        spin_dir = 0u - spin_dir;
        const uint32_t life2 = static_cast<uint32_t>(frame_scale_i(0x1e));
        (void)spawn_particle_35_v(pt, life2, 0x7f7f7f7fu, 1, spin_dir, V(OV(0x0015f580u)),
                                  fpu_mul(f22, u2f(0x3d8f5c29u)), fpu_mul(f22, u2f(0x3f333333u)), c_fade);
    }

    if (rand_mod(2) == 0) {
        const Vec4 pt = V(OV(0x001dc0e0u) + 0xb0u);
        const float c_fade = u2f(0x3b449ba6u);
        const uint32_t life1 = static_cast<uint32_t>(frame_scale_i(0x1e));
        (void)spawn_particle_35_v(pt, life1, 0x7f20207fu, 0, spin_dir, V(OV(0x0015f580u)), fpu_mul(f22, k01), f22, c_fade);
        spin_dir = 0u - spin_dir;
        const uint32_t life2 = static_cast<uint32_t>(frame_scale_i(0x1e));
        (void)spawn_particle_35_v(pt, life2, 0x7f7f7f7fu, 1, spin_dir, V(OV(0x0015f580u)),
                                  fpu_mul(f22, u2f(0x3d8f5c29u)), fpu_mul(f22, u2f(0x3f333333u)), c_fade);
    }

    vec3_sub(seg_dir, V(OV(0x001dbf70u)), V(OV(0x001dbf60u)));
    vec3_set_length(seg_dir, seg_dir, fpu_mul(lwc1(OV(0x00161734u)), lwc1(0x0015ed60u)));
    if (rand_mod(2) == 0) {
        vec3_set_length(perp_vec, V(p0 + 0xd0u), lwc1(OV(0x00161740u)));
        Vec4 spawn_pos{};
        vec3_add(spawn_pos, V(OV(0x001dbf60u)), perp_vec);
        const uint32_t col_pick = lwu(OV(0x001e9ab0u) + static_cast<uint32_t>(rand_mod(5)) * 4u);
        const float ang1 = rand_range_signed_f(0.0f, lwc1(OV(0x0016173cu)));
        const float ang2 = rand_range_signed_f(0.0f, lwc1(OV(0x0016173cu)));

        Vec4 vel_dir{}, side_axis{};
        vec_rotate_axis(vel_dir, seg_dir, V(CAM_UP), ang2);
        vec3_cross_ba(side_axis, seg_dir, V(CAM_UP));
        vec_rotate_axis(vel_dir, vel_dir, side_axis, ang1);

        const float denom = fpu_mul(lwc1(OV(0x00161734u)), lwc1(0x0015ed60u));
        uint32_t p_life = static_cast<uint32_t>(float_to_int(fpu_div(fpu_mul(beam_len, u2f(0x3f99999au)), denom)));
        const uint32_t tgt_m = lwu(pv + 0x10u);
        if (tgt_m != 0u) {
            p_life *= 3u;
        }
        morph_275910_v(spawn_pos, p_life, col_pick, 0u, spin_dir, vel_dir, tgt_m, fpu_mul(f22, k01), f22);
        morph_275910_v(spawn_pos, p_life, 0x7f7f7f7fu, 1u, 0u - spin_dir, vel_dir, lwu(pv + 0x10u),
                       fpu_mul(f22, u2f(0x3d8f5c29u)), fpu_mul(f22, u2f(0x3f333333u)));
    }

    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(OV(0x00161824u)))) != 0) {
        const int32_t next_wait = frame_scale_i(lw(OV(0x00161744u)));
        sw(OV(0x00161824u), static_cast<uint32_t>(next_wait));
        for (uint32_t idx = 0; idx < 3; ++idx) {
            if (lw(OV(0x00161818u) + idx * 4u) == 0) {
                sw(OV(0x00161818u) + idx * 4u, 1u);
                const int32_t ring_life = frame_scale_i(lw(OV(0x0016174cu)));
                const float init_size = lwc1(OV(0x0016175cu));
                const int32_t init_alpha = static_cast<int8_t>(lbu(OV(0x00161763u)));
                sw(OV(0x001617d8u) + idx * 4u, static_cast<uint32_t>(ring_life));
                swc1(OV(0x00161808u) + idx * 4u, init_size);
                sw(OV(0x001617f8u) + idx * 4u, 0u);
                swc1(OV(0x001617e8u) + idx * 4u, int_to_float(init_alpha));
                V(OV(0x001dd0a0u) + idx * 0x10u) = V(pv);
                vec3_sub(perp_vec, V(OV(0x001dbf70u)), V(OV(0x001dbf60u)));
                vec3_set_length(V(OV(0x001dd0d0u) + idx * 0x10u), perp_vec, 1.0f);
                break;
            }
        }
    }

    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t active_addr = OV(0x00161818u) + i * 4u;
        if (lw(active_addr) == 0) continue;

        const uint32_t timer_addr = OV(0x001617d8u) + i * 4u;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer_addr))) != 0) {
            sw(active_addr, 0u);
            continue;
        }

        if (lw(timer_addr) < lw(OV(0x00161750u))) {
            const float dec = lwc1(OV(0x00161768u));
            const uint32_t alpha_addr = OV(0x001617e8u) + i * 4u;
            swc1(alpha_addr, fpu_sub(lwc1(alpha_addr), dec));
        }

        const uint32_t dist_addr = OV(0x001617f8u) + i * 4u;
        const float step_dist = fpu_mul(lwc1(OV(0x00161748u)), lwc1(0x0015ed6cu));
        const float max_dist = lwc1(OV(0x001616dcu));
        const float new_dist = fpu_add(lwc1(dist_addr), step_dist);
        float frac = fpu_div(new_dist, max_dist);
        swc1(dist_addr, new_dist);
        if (1.0f < frac) {
            frac = 1.0f;
        }

        const float sz0 = lwc1(OV(0x0016175cu));
        const float sz1 = lwc1(OV(0x00161758u));
        swc1(OV(0x00161808u) + i * 4u, fpu_add(sz0, fpu_mul(fpu_sub(sz1, sz0), frac)));

        const uint32_t pos_addr = OV(0x001dd0a0u) + i * 0x10u;
        const uint32_t dir_addr = OV(0x001dd0d0u) + i * 0x10u;
        if (max_dist <= new_dist) {
            Vec4 delta{};
            vec3_scale(delta, V(dir_addr), step_dist);
            vec3_add(V(pos_addr), V(pos_addr), delta);
            continue;
        }

        const int32_t seg_idx = float_to_int(fpu_div(new_dist, lwc1(OV(0x001617ccu))));
        const float seg_start = fpu_mul(int_to_float(seg_idx), lwc1(OV(0x001617ccu)));
        const float rem_dist = fpu_sub(lwc1(dist_addr), seg_start);
        const uint32_t node_off = static_cast<uint32_t>(seg_idx) << 4;
        const uint32_t node_addr = OV(0x001dbf60u) + node_off;
        V(pos_addr) = V(node_addr);

        Vec4 seg_vec{};
        if (seg_idx == 11) {
            vec3_sub(seg_vec, V(OV(0x001dbf60u) + 0xb0u), V(OV(0x001dbf50u) + 0xb0u));
        } else {
            vec3_sub(seg_vec, V(OV(0x001dbf70u) + node_off), V(node_addr));
        }

        const float seg_vlen = vec3_length(seg_vec);
        if (seg_vlen == 0.0f) continue;

        const float inv_len = fpu_div(1.0f, seg_vlen);
        Vec4 along{};
        vec3_scale(along, seg_vec, fpu_mul(rem_dist, inv_len));
        vec3_add(V(pos_addr), V(pos_addr), along);
        vec3_set_length(along, seg_vec, inv_len);
        vec3_lerp(V(dir_addr), V(dir_addr), along, u2f(0x3dcccccdu));
        vec3_set_length(V(dir_addr), V(dir_addr), 1.0f);
    }
}

// orig: 0x002c92d8
void morph_2c92d8(uint32_t p0) {
    if (p0 == 0u) return;
    sw(p0 + 0xacu, 0xffffffffu);
    swc1(p0 + 0x7cu, u2f(0x3d4ccccdu));
    swc1(p0 + 0x80u, u2f(0x3f5ced91u));
    sw(p0 + 0x84u, 4u);
    sw(p0 + 0x88u, 3u);
    swc1(p0 + 0x8cu, u2f(0x3f200000u));
    sh(p0 + 0x9eu, 0xffffu);
    sw(p0 + 0x78u, 0u);
    sh(p0 + 0x9cu, 0xffffu);
    sw(p0 + 0xa0u, 0xffffffffu);
    sw(p0 + 0xa4u, 0xffffffffu);
    sw(p0 + 0xa8u, 0xffffffffu);
}

// orig: 0x002c9348
void morph_2c9348(uint32_t p0) {
    const uint32_t m = moby_spawn(0x10e);
    if (m == 0u) return;

    sb(m + 0x30u, 0x7fu);
    sh(m + 0x32u, 0x7fu);
    sb(m + 0x31u, 1u);
    const uint32_t pv = lwu(m + 0x78u);

    bool copied_hi = false;
    if ((lhu(p0 + 0x34u) & 0x20u) != 0) {
        const uint32_t src_c = lwu(lwu(p0 + 0x78u) + 0xcu);
        if (src_c != 0u) {
            moby_set_color_hi(m, lbu(src_c + 0x4u), lbu(src_c + 0x5u), lbu(src_c + 0x6u));
            copied_hi = true;
        }
    }
    if (!copied_hi) {
        sd(m + 0x38u, ld(p0 + 0x38u));
    }

    swc1(m + 0x58u, rand_range_f(u2f(0x3f59999au), u2f(0x3f933333u)));
    V(m + 0x10u) = V(p0 + 0x10u);
    vec_clear(V(m + 0x40u));

    const float yaw = atan2_f(fpu_sub(lwc1(m + 0x10u), lwc1(HERO_POS + 0x0u)),
                              fpu_sub(lwc1(m + 0x14u), lwc1(HERO_POS + 0x4u)));
    swc1(m + 0x48u, yaw);
    const float floor_z = floor_probe(0.5f, m + 0x10u, 0u);
    if (lwc1(m + 0x18u) <= floor_z) {
        swc1(m + 0x18u, floor_z);
        const uint32_t col_hit = OV(0x00173e40u);
        swc1(m + 0x18u, lwc1(col_hit + 0x28u));
        const Vec4 nrm = V(col_hit + 0x40u);

        const float c1 = cos_vu(lwc1(m + 0x48u));
        const float s1 = sin_vu(lwc1(m + 0x48u));
        const float nx = fpu_add(fpu_mul(nrm.x, c1), fpu_mul(nrm.y, s1));

        const float c2 = cos_vu(lwc1(m + 0x48u));
        const float s2 = sin_vu(lwc1(m + 0x48u));
        const float ny = fpu_sub(fpu_mul(nrm.y, c2), fpu_mul(nrm.x, s2));
        const float nz = nrm.z;

        const float len_xz = sqrt_f(fpu_add(fpu_mul(nx, nx), fpu_mul(nz, nz)));
        swc1(m + 0x40u, -atan2_f(len_xz, ny));
        swc1(m + 0x44u, atan2_f(nz, nx));
    }

    sw(m + 0x94u, lwu(lwu(m + 0x24u) + 0x10u));
    (void)moby_sound_start(1, 0u, m);
    sb(m + 0x20u, 1u);
    sh(pv + 0x66u, 0u);
    const int32_t wait_lo = frame_scale_i(0x78);
    const int32_t wait_hi = frame_scale_i(0x12c);
    sh(pv + 0x64u, static_cast<uint32_t>(rand_range(wait_lo, wait_hi)));

    const uint32_t gold = lbu(GOLD_MORPH_FLAG);
    sw(pv + 0x70u, 0xffffffffu);
    swc1(pv + 0x78u, fpu_add(fpu_mul(int_to_float(static_cast<int32_t>(gold)), 3.0f), 1.0f));
    sw(pv + 0x74u, lbu(GOLD_MORPH_FLAG));
    sb(m + 0xbcu, lbu(pv + 0x74u));
    moby_drop_bolts(p0, 0u, -1);

    const uint32_t src_pv0 = moby_pvar_word0(p0);
    if (src_pv0 != 0u && lbu(src_pv0 + 0xeu) == 1u) {
        sb(src_pv0 + 0xeu, 2u);
    } else {
        moby_retire(p0);
    }

    spawn_particle_05(m + 0x10u, 0x7fu, 0x7fu, 0x7fu, frame_scale_i(0xf), u2f(0x49742400u), 0.0f);
    mem_fill32(pv, 0u, 0x20);

    const uint32_t slot = lwu(OV(0x00161854u));
    const uint32_t old = lwu(OV(0x001dd100u) + (slot << 2));
    bool reused_active = false;
    if (old != 0u && lh(old + 0xa6u) == 0x10e) {
        const uint32_t old_st = lbu(old + 0x20u);
        if (old_st != 0xfeu && old_st != 0xfdu) {
            if (old_st == 5u) {
                swc1(pv + 0x78u, 0.0f);
                sh(m + 0x34u, lhu(m + 0x34u) & 0xffdfu);
            } else if (lw(pv + 0x74u) == 0) {
                sw(pv + 0x14u, OV(0x001dd150u) + slot * 0xb0u);
                sw(pv + 0x10u, OV(0x001ddf10u) + slot * 0x60u);
                sh(m + 0x34u, lhu(m + 0x34u) | 0x1020u);
                sw(lwu(pv + 0x14u) + 0x70u, OV(0x00161840u));
            }
            const uint32_t cur_slot = lwu(OV(0x00161854u));
            const uint32_t cur_old = lwu(OV(0x001dd100u) + (cur_slot << 2));
            sw(lwu(cur_old + 0x78u) + 0x70u, 1u);
            reused_active = true;
        }
    }
    if (!reused_active && lw(pv + 0x74u) == 0) {
        const uint32_t cur_slot = lwu(OV(0x00161854u));
        sw(pv + 0x10u, OV(0x001ddf10u) + cur_slot * 0x60u);
        sw(pv + 0x14u, OV(0x001dd150u) + cur_slot * 0xb0u);
        sh(m + 0x34u, lhu(m + 0x34u) | 0x1020u);
        sw(lwu(pv + 0x14u) + 0x70u, OV(0x00161840u));
    }

    morph_2c92d8(lwu(pv + 0x14u));
    {
        const uint32_t cur_slot = lwu(OV(0x00161854u));
        sw(OV(0x001dd100u) + (cur_slot << 2), m);
        const uint32_t next_slot = static_cast<uint32_t>(ee_rem(static_cast<int32_t>(cur_slot + 1u), 20));
        sw(OV(0x00161854u), next_slot);
        if (lw(pv + 0x74u) != 0) {
            sh(m + 0x34u, lhu(m + 0x34u) & 0xffdfu);
        }
    }
    moby_update_transform(m);
}

// orig: 0x002bc3b0
int32_t morph_2bc3b0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) {
    return morph_2bc3b0_v(p0, V(p1), p2, p3);
}

// orig: 0x002bc7e8
void morph_update(uint32_t m) {
    const bool mode_is_3 = lw(P + 0x10b4u) == 3;
    const uint32_t st = lbu(m + 0x20u);
    const uint32_t pv = lwu(m + 0x78u);
    if (st == 0u) {
        sb(m + 0x20u, 1u);
        sw(OV(0x001617c8u), 0xffffffffu);
        sw(OV(0x001617c0u), 0xffffffffu);
        sw(OV(0x001617c4u), 0u);
    }

    bool ready = true;
    if (lbu(m + 0x53u) == 0u && moby_anim_time(m) < 6.0f) {
        ready = false;
    }

    (void)timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1cu)));
    const uint32_t move_state = lwu(P + 0x208cu);
    const bool allowed_state = (move_state < 2u || move_state == 0xcu || move_state == 4u || move_state == 5u);
    const bool firing = allowed_state && ((lwu(PAD_HELD) & lwu(P + 0x10a0u)) != 0u) && !mode_is_3 && ready;

    if (firing) {
        if (lbu(P + 0x20a8u) == 0u) {
            hero_hold_start();
        }
        if (lh(pv + 0x1eu) == 0) {
            sh(pv + 0x1eu, 1u);
            const uint32_t stats = 0x00141680u;
            const uint32_t cnt = lhu(stats + 0xa8u);
            if (!(0xfffeu < cnt)) {
                sh(stats + 0xa8u, cnt + 1u);
            }
            const int32_t cur_sec = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
            if (static_cast<int32_t>(lhu(stats + 0xaau)) < cur_sec) {
                const int32_t new_sec = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
                sh(stats + 0xaau, static_cast<uint32_t>(new_sec));
            }
            const uint32_t item_id = lwu(0x0015ed84u);
            sw(stats + 0xacu, lwu(stats + 0xacu) | (1u << (item_id & 31u)) | 0x80000000u);
        }
        if (lh(pv + 0x1cu) == 0) {
            sh(pv + 0x1cu, static_cast<uint32_t>(frame_scale_i(0xf)));
        }

        moby_chain_point_v(m, 0u, V(pv));
        if (lw(P + 0x2084u) == 1) {
            (void)hero_set_state(0x1eu, 1);
        }

        Vec4 dir{0.0f, 0.0f, 0.0f, 0.0f};
        if (lw(P + 0x2084u) == 0x1e && (lhu(lwu(P + 0x2080u) + 0x34u) & 1u) != 0u) {
            Mat4 cam_m{}, cam_inv{};
            mat3_copy(cam_m, M(OV(0x00166c80u)));
            mat3_transpose(cam_inv, cam_m);
            dir.z = 1.0f;
            mat4_transform(dir, dir, cam_inv);
        } else if (lh(P + 0x22c8u) == 1) {
            vec3_scale(dir, V(m + 0xd0u), -1.0f);
        } else {
            dir.x = 1.0f;
            mat3_transform(dir, dir, M(P + 0x640u));
            if (advanced_aiming()) {  // enhancement: the modern aim's line (advanced.h), the original's facing otherwise
                const uint32_t tmp = ee_stack_alloc(0x10);
                V(tmp) = dir;
                advanced_aim_shot(pv, tmp);
                dir = V(tmp);
                ee_stack_free(0x10);
            }
        }

        swc1(pv + 0x48u, atan2_f(dir.x, dir.y));
        const float horiz = vec2_length(dir);
        swc1(pv + 0x4cu, atan2_f(horiz, dir.z));
        const Vec4 from = V(pv);
        const uint32_t found = static_cast<uint32_t>(morph_2bc3b0_v(m, from, pv + 0x48u, pv + 0x4cu));

        (void)timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x44u)));
        const uint32_t cur_tgt = lwu(pv + 0x10u);
        if (cur_tgt != 0u) {
            const uint32_t tst = lbu(cur_tgt + 0x20u);
            if (tst == 0xfeu || tst == 0xfdu) {
                sw(pv + 0x10u, 0u);
                sw(pv + 0x44u, 0u);
            }
        }

        bool update_meter = false;
        if (found == 0u) {
            if (lw(pv + 0x44u) != 0) {
                update_meter = true;
            }
        } else {
            const uint32_t fpv = moby_pvar_word0(found);
            if (found != lwu(pv + 0x10u) && lw(pv + 0x44u) == 0) {
                sw(pv + 0x10u, found);
                swc1(pv + 0x14u, int_to_float(lh(fpv + 0x4u)));
                swc1(pv + 0x18u, lwc1(fpv + 0x0u));
                const int32_t lock_frames = frame_scale_i(0x1e);
                const float max_hp = lwc1(pv + 0x14u);
                sw(pv + 0x44u, static_cast<uint32_t>(lock_frames));
                if (max_hp <= 1.0f) {
                    sw(pv + 0x18u, 0u);
                }
                const int32_t req = camera_request(4u, 0x7533u, OVF(0x00236b10u), OVF(0x00236d00u), OVF(0x00236e50u),
                                                   OV(0x001616ecu), 0x2710u);
                sw(pv + 0x34u, static_cast<uint32_t>(req));
            }
            update_meter = true;
        }

        if (update_meter) {
            const float gold_mult = fpu_add(fpu_mul(int_to_float(static_cast<int32_t>(lbu(GOLD_MORPH_FLAG))), u2f(0x3ea8f5c3u)), 1.0f);
            const float drain = fpu_mul(fpu_mul(lwc1(OV(0x001616e0u)), lwc1(0x0015ed6cu)), gold_mult);
            float hp = fpu_sub(lwc1(pv + 0x18u), drain);
            if (hp < 0.0f) {
                hp = 0.0f;
            }
            swc1(pv + 0x18u, hp);

            const int32_t pct = float_to_int(fpu_div(fpu_mul(lwc1(pv + 0x18u), 10000.0f), lwc1(pv + 0x14u)));
            sw(OV(0x001616ecu), 0x2710u - static_cast<uint32_t>(pct));
        }

        const uint32_t morphed_tgt = lwu(pv + 0x10u);
        if (morphed_tgt != 0u && lwc1(pv + 0x18u) <= 0.0f) {
            if (lw(0x0015ed84u) == 5 && lh(morphed_tgt + 0xa6u) == 0x271 && lbu(0x0013d410u) == 0u) {
                sb(0x0013d410u, 1u);
                (void)sound_play_indexed(1, 0u, 0u);
                sub_00263d40(TXT(0x53d6), -1);
            }
            morph_2c9348(lwu(pv + 0x10u));
            sw(pv + 0x10u, 0u);
        }

        morph_2bd0a0(m);
        if (lw(OV(0x001617c0u)) == -1) {
            const int32_t reg = sub_0023e5e0_v(V(m + 0x10u), 0x00207f7fu, 7.5f, 0.0f);
            sw(OV(0x001617c0u), static_cast<uint32_t>(reg));
        }
        if (lw(OV(0x001617c0u)) >= 0) {
            sw(OV(0x001617c4u), static_cast<uint32_t>(frame_scale_i(0x14)));
            Vec4 back_off{}, light_pos{};
            vec3_set_length(back_off, V(m + 0xd0u), -0.5f);
            vec3_add(light_pos, V(pv), back_off);
            const uint32_t light_rec = OV(0x001802c0u) + (lwu(OV(0x001617c0u)) << 5);
            swc1(light_rec + 0x10u, light_pos.x);
            swc1(light_rec + 0x1cu, 7.5f);
            swc1(light_rec + 0x14u, light_pos.y);
            swc1(light_rec + 0x18u, light_pos.z);
        }
        if (sub_0028d8c0(m, lw(OV(0x001617c8u))) == 0) {
            sw(OV(0x001617c8u), static_cast<uint32_t>(moby_sound_start(0, 4u, m)));
        }
    } else {
        const int32_t hud_req = lw(pv + 0x34u);
        sw(pv + 0x10u, 0u);
        sh(pv + 0x1eu, 0u);
        sw(pv + 0x44u, 0u);
        if (hud_req != -1) {
            (void)hud_request_clear(hud_req);
            sw(pv + 0x34u, 0xffffffffu);
        }
        sw(OV(0x001617bcu), 1u);
        if (lw(OV(0x001617c0u)) >= 0) {
            Vec4 back_off{}, light_pos{};
            vec3_set_length(back_off, V(m + 0xd0u), -0.5f);
            vec3_add(light_pos, V(pv), back_off);
            const uint32_t data_base = OV(0x001802c0u);
            const uint32_t light_rec = data_base + (lwu(OV(0x001617c0u)) << 5);
            const int32_t fade_t = lw(OV(0x001617c4u));
            swc1(light_rec + 0x14u, light_pos.y);
            swc1(light_rec + 0x18u, light_pos.z);
            swc1(light_rec + 0x1cu, 7.5f);
            swc1(light_rec + 0x10u, light_pos.x);

            const float f0 = fpu_mul(int_to_float(fade_t), u2f(0x3d4ccccdu));
            const float f1 = fpu_mul(f0, 0.25f);
            const uint32_t light_rec2 = data_base + (lwu(OV(0x001617c0u)) << 5);
            swc1(light_rec2 + 0x0u, f0);
            swc1(light_rec2 + 0x4u, f0);
            swc1(light_rec2 + 0x8u, f1);

            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(OV(0x001617c4u)))) != 0) {
                region_clear(lwu(OV(0x001617c0u)));
                sw(OV(0x001617c0u), 0xffffffffu);
            }
            const int32_t cur_reg = lw(OV(0x001617c0u));
            if (cur_reg >= 0 && lw(OV(0x001617c4u)) == 0) {
                region_clear(static_cast<uint32_t>(cur_reg));
                sw(OV(0x001617c0u), 0xffffffffu);
            }
        }

        const int32_t snd_slot = lw(OV(0x001617c8u));
        if (snd_slot >= 0) {
            if (snd_slot != -1) {
                const uint32_t ch = SND_CHANNELS + static_cast<uint32_t>(snd_slot) * 0x70u;
                if (lwu(ch + 0x88u) == m && lbu(ch + 0x74u) != 0u) {
                    sub_0028d918(snd_slot);
                }
            }
            sw(OV(0x001617c8u), 0xffffffffu);
        }
        hero_hold_release();
    }

    if (mode_is_3) {
        const int32_t reg = lw(OV(0x001617c0u));
        if (reg >= 0) {
            region_clear(static_cast<uint32_t>(reg));
            sw(OV(0x001617c0u), 0xffffffffu);
        }
    }
}

// Class 804, a piece riding on the Morph-o-Ray's hand moby (pvar +0, class 0xb9): it slides out along the hand's axis
// (+4, by twice the ray's range per second), fading past the range and gone at 1.1 times it, when its timer (+8) runs
// out or the hand goes; its scale +0xc grows with the distance.
// orig: 0x002d7f88
void class804_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float range = lwc1(OV(0x001616dcu));
    swc1(pv + 4, fpu_add(lwc1(pv + 4), fpu_mul(fpu_add(range, range), lwc1(0x0015ed6cu))));
    const bool expired = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) != 0;
    const uint32_t hand = lwu(pv);
    if (expired || fpu_mul(lwc1(OV(0x001616dcu)), u2f(0x3f8ccccdu)) <= lwc1(pv + 4) || hand == 0 || lh(hand + 0xa6) != 0xb9 ||
        lbu(hand + 0x20) == 0xfe || lbu(hand + 0x20) == 0xfd) {
        moby_retire(m);
        return;
    }
    const uint32_t hand_pv = lwu(hand + 0x78);
    vec_clear(V(m + 0x10));
    swc1(m + 0x14, -lwc1(pv + 4));
    mat3_transform(V(m + 0x10), V(m + 0x10), M(hand + 0xc0));
    vec3_add(V(m + 0x10), V(m + 0x10), V(hand_pv));
    V(m + 0x40) = V(hand + 0x40);
    const float out = lwc1(pv + 4);
    if (lwc1(OV(0x001616dcu)) < out) {
        const float past = fpu_mul(fpu_sub(out, lwc1(OV(0x001616dcu))), 10.0f);
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(1.0f, past), 127.0f))));
    }
    const float grow = lwc1(pv + 0xc);
    swc1(m + 0x2c, fpu_add(fpu_mul(grow, u2f(0x3dcccccdu)), fpu_div(fpu_mul(grow, lwc1(pv + 4)), lwc1(OV(0x001616dcu)))));
}

// orig: (none)
void morph_fns_register() {
    ovl_register_moby_fn(0x002bc7e8, morph_update);
}

}  // namespace rac1
