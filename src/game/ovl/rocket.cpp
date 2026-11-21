// Ported overlay code: Devastator rocket projectile (class 0x99, update 0x002b03a0),
// Gold Devastator ricochet target search (0x002affa8), and VU0 matrix/vector helpers.
#include <game/ovl/rocket.h>

#include <bit>
#include <game/math/vecmath.h>
#include <game/ovl/devastator.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {

struct EeStackFrame {
    uint32_t addr;
    uint32_t size;
    explicit EeStackFrame(uint32_t sz) : addr(ee_stack_alloc(sz)), size(sz) {}
    ~EeStackFrame() { ee_stack_free(size); }
    EeStackFrame(const EeStackFrame&) = delete;
    EeStackFrame& operator=(const EeStackFrame&) = delete;
    operator uint32_t() const { return addr; }
};

constexpr uint32_t HERO = 0x0013f350u;

// orig: (helper) VU0 operand/result clamp: denormals -> signed zero, Inf/NaN -> +-FLT_MAX
inline float vuc(float f) {
    uint32_t u = std::bit_cast<uint32_t>(f);
    const uint32_t e = u & 0x7f800000u;
    if (e == 0) u &= 0x80000000u;
    else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
    return std::bit_cast<float>(u);
}

// orig: (helper) host-Mat4 form of 0x001ffe20
void mat4_add_vu(Mat4& d, const Mat4& a, const Mat4& b) {
    const Mat4 A = a, B = b;
    for (int i = 0; i < 4; ++i) {
        d.r[i] = Vec4{vuc(vuc(A.r[i].x) + vuc(B.r[i].x)),
                      vuc(vuc(A.r[i].y) + vuc(B.r[i].y)),
                      vuc(vuc(A.r[i].z) + vuc(B.r[i].z)),
                      vuc(vuc(A.r[i].w) + vuc(B.r[i].w))};
    }
}

// orig: (helper) host-Mat4 form of 0x001ffd48
void mat4_scale_vu(Mat4& d, const Mat4& a, float s) {
    const Mat4 A = a;
    const float sv = vuc(s);
    for (int i = 0; i < 4; ++i) {
        d.r[i] = Vec4{vuc(vuc(A.r[i].x) * sv),
                      vuc(vuc(A.r[i].y) * sv),
                      vuc(vuc(A.r[i].z) * sv),
                      vuc(vuc(A.r[i].w) * sv)};
    }
}

}  // namespace

// Gold Devastator ricochet target search from target list 0x1aba00 (skipping `p2`).
// orig: 0x002affa8
void rocket_2affa8(uint32_t p0, uint32_t p1, uint32_t p2) {
    float best_dist = u2f(0x461c4000u);
    float aim_yaw = lwc1(p0 + 0x48);
    float aim_pitch = lwc1(p0 + 0x44);

    for (uint32_t idx = 0;; ++idx) {
        const uint32_t cand = lwu(OV(0x001aba00u) + (idx << 2));
        if (cand == 0) break;
        if (static_cast<int8_t>(lbu(cand + 0x20)) < 0) continue;
        if (cand == p2) continue;

        const uint32_t cand_pos_addr = cand + 0x10;
        Vec4 tpos = V(cand_pos_addr);
        if (40.0f < vec3_distance(tpos, V(p0 + 0x10))) continue;

        uint32_t w0 = moby_pvar_word0(cand);
        if (w0 != 0 && static_cast<int32_t>(0x00100000u) < static_cast<int32_t>(w0) &&
            !(static_cast<int32_t>(0x003fffffu) < static_cast<int32_t>(w0))) {
            tpos.z = fpu_add(tpos.z, lwc1(w0 + 0x10));
        } else {
            w0 = 0;
            tpos.z = fpu_add(tpos.z, 0.5f);
        }

        const uint32_t cls_info = lwu(cand + 0x24);
        if (cls_info == 0 || lh(cls_info + 0x46) != 5) continue;

        const float yaw_to = atan2_f(fpu_sub(tpos.x, lwc1(p0 + 0x10)), fpu_sub(tpos.y, lwc1(p0 + 0x14)));
        const float pitch_to = atan2_f(vec2_distance(V(p0 + 0x10), tpos), fpu_sub(tpos.z, lwc1(p0 + 0x18)));
        const float dist3d = vec3_distance(V(p0 + 0x10), tpos);

        if (dist3d < 2.5f) {
            const float hero_to =
                atan2_f(fpu_sub(lwc1(cand + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(cand + 0x14), lwc1(HERO + 0x84)));
            const uint32_t hero_moby = lwu(HERO + 0x2080);
            if (angle_dist(lwc1(hero_moby + 0x48), hero_to) < u2f(0x3f860a92u) &&
                abs_f(pitch_to) < u2f(0x3f490fdbu)) {
                return;
            }
        }

        if (best_dist < dist3d) continue;

        const float gold_cone = fpu_mul(int_to_float(static_cast<int32_t>(lbu(0x0013e52bu))), u2f(0x3f32b8c2u));
        const float pitch_adj = fpu_add(aim_pitch, fpu_mul(lwc1(HERO + 0x2e4), 0.5f));
        Vec4 cone_pt{};
        vec_from_spherical_v(cone_pt, dist3d, aim_yaw, pitch_adj);
        vec3_add(cone_pt, cone_pt, V(p0 + 0x10));

        const float chord = vec3_distance(cone_pt, tpos);
        const float half_ang = asin_f(fpu_div(chord, fpu_add(dist3d, dist3d)));
        const float cone_ang = fpu_add(half_ang, half_ang);
        const float max_cone = fpu_add(gold_cone, u2f(0x3fc90fdbu));
        if (max_cone < cone_ang && w0 != 0) {
            const float rad = fpu_mul(int_to_float(static_cast<int32_t>(lbu(w0 + 0xa) << 3)), 0.125f);
            (void)asin_f(fpu_div(rad, dist3d));
        }

        if (segment_collide_v(V(OV(0x00166dc0u)), tpos, 6u, p0, nullptr) != 0) continue;

        best_dist = dist3d;
        aim_pitch = -pitch_to;
        aim_yaw = yaw_to;
        if (dev_25f878(cand) != 0) continue;

        const float z_off = (w0 != 0) ? lwc1(w0 + 0x10) : 0.5f;
        sw(p1 + 0x18, cand);
        swc1(p1 + 0x2c, yaw_to);
        sw(p1 + 0x38, 1u);
        swc1(p1 + 0x30, aim_pitch);
        sw(p1 + 0x24, 0);
        sw(p1 + 0x28, 0);
        sw(p1 + 0x20, 0);
        V(p1) = V(cand_pos_addr);
        swc1(p1 + 0x1c, z_off);
    }
}

// VU0 4x4 matrix addition: M(p0) = M(p1) + M(p2).
// orig: 0x001ffe20
void rocket_1ffe20(uint32_t p0, uint32_t p1, uint32_t p2) {
    mat4_add_vu(M(p0 & ~15u), M(p1 & ~15u), M(p2 & ~15u));
}

// VU0 in-place xyz subtraction: V(p1).xyz -= V(p2).xyz, preserving V(p1).w (p0 is unused).
// orig: 0x001ff2c0
void rocket_1ff2c0(uint32_t p0, uint32_t p1, uint32_t p2) {
    (void)p0;
    Vec4& d = V(p1 & ~15u);
    vec3_sub(d, d, V(p2 & ~15u));
}

// VU0 4x4 matrix scalar multiply: M(p0) = M(p1) * pf12.
// orig: 0x001ffd48
void rocket_1ffd48(uint32_t p0, uint32_t p1, float pf12) {
    mat4_scale_vu(M(p0 & ~15u), M(p1 & ~15u), pf12);
}

// Devastator rocket projectile update (class 0x99).
// orig: 0x002b03a0
void devastator_rocket_update(uint32_t m) {
    uint32_t direct_hit_moby = 0;
    uint32_t overloaded = 0;

    Vec4 refl{0.0f, 0.0f, fpu_mul(lwc1(0x0015ed6cu), 8.0f), 0.0f};
    Vec4 norm{0.0f, 0.0f, 1.0f, 0.0f};
    Vec4 tang{0.0f, 0.0f, 0.0f, 0.0f};

    const uint32_t pv = lwu(m + 0x78);
    const uint32_t target = lwu(pv + 0x18);
    if (u2f(0x3f666666u) < lwc1(OV(0x0015f5d0u)) || u2f(0x3f666666u) < lwc1(OV(0x0015f5d4u))) {
        overloaded = 1u;
    }

    // Accelerate toward target speed pv+0x34 and tick hero-velocity blend timer pv+0x3c
    const float cur_spd = lwc1(pv + 0x20);
    const float spd_step = fpu_mul(fpu_div(fpu_sub(lwc1(pv + 0x34), cur_spd), 10.0f), lwc1(0x0015ed64u));
    swc1(pv + 0x20, fpu_add(cur_spd, spd_step));

    (void)timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x3c)));
    const float carry_ratio =
        fpu_div(int_to_float(static_cast<int32_t>(lh(pv + 0x3c))), int_to_float(static_cast<int32_t>(lh(pv + 0x3e))));

    Vec4 hero_vel_sum{};
    vec3_add(hero_vel_sum, V(0x0013f490u), V(0x0013f450u));
    Vec4 hero_vel_scaled = hero_vel_sum;
    vec3_scale(hero_vel_scaled, hero_vel_scaled, carry_ratio);

    const float gold_scale = fpu_add(int_to_float(static_cast<int32_t>(lbu(0x0013e52bu))), 1.0f);

    // Homing & quadratic lead guidance
    float yaw_goal, pitch_goal;
    const int32_t arm_threshold = frame_scale_i(0x12c) - frame_scale_i(5);
    if (target != 0 && lw(pv + 0x14) < arm_threshold && static_cast<int8_t>(lbu(target + 0x20)) >= 0) {
        float t_hit = 0.0f;
        Vec4 tgt_pos = V(target + 0x10);
        tgt_pos.z = fpu_add(tgt_pos.z, lwc1(pv + 0x1c));

        Vec4 to_tgt{};
        vec3_sub(to_tgt, tgt_pos, V(m + 0x10));

        Vec4 tgt_vel{0.0f, 0.0f, 0.0f, 0.0f};
        if (lwu(pv + 0x38) != 0) {
            vec3_sub(tgt_vel, tgt_pos, V(pv));
            vec3_sub(tgt_vel, tgt_vel, hero_vel_scaled);
            V(pv) = tgt_pos;

            const float v_sq = vec3_length_sq_vu(tgt_vel);
            float b_term = fpu_mul(
                fpu_add(fpu_add(fpu_mul(tgt_vel.x, to_tgt.x), fpu_mul(tgt_vel.y, to_tgt.y)), fpu_mul(tgt_vel.z, to_tgt.z)),
                -2.0f);
            const float r_spd = lwc1(pv + 0x20);
            float a_term = fpu_sub(fpu_mul(r_spd, r_spd), v_sq);
            const float neg_d_sq = -vec3_length_sq_vu(to_tgt);
            const float disc = fpu_sub(fpu_mul(b_term, b_term), fpu_mul(fpu_mul(a_term, 4.0f), neg_d_sq));
            const float root = sqrt_f(disc);
            b_term = -b_term;
            a_term = fpu_add(a_term, a_term);

            const float t1 = fpu_div(fpu_add(b_term, root), a_term);
            const float t2 = fpu_div(fpu_sub(b_term, root), a_term);
            if (0.0f < t1 && 0.0f < t2) {
                t_hit = !(t2 < t1) ? t2 : t1;  // matches original bc1f polarity at 0x2b06d4
            } else if (0.0f < t1) {
                t_hit = t1;
            } else if (0.0f < t2) {
                t_hit = t2;
            } else {
                t_hit = -1.0f;
            }
        }

        if (0.0f < t_hit && t_hit < static_cast<float>(frame_scale_i(0x12c))) {
            Vec4 lead{};
            vec3_scale(lead, tgt_vel, t_hit);
            vec3_add(lead, lead, to_tgt);
            yaw_goal = atan2_f(lead.x, lead.y);
            pitch_goal = -atan2_f(vec2_length(lead), lead.z);
        } else {
            yaw_goal = atan2_f(to_tgt.x, to_tgt.y);
            pitch_goal = -atan2_f(vec2_length(to_tgt), to_tgt.z);
        }
    } else {
        const int32_t arm_threshold2 = frame_scale_i(0x12c) - frame_scale_i(5);
        if (lw(pv + 0x14) < arm_threshold2) {
            yaw_goal = lwc1(pv + 0x2c);
            pitch_goal = lwc1(pv + 0x30);
        } else {
            yaw_goal = lwc1(m + 0x48);
            pitch_goal = lwc1(m + 0x44);
        }
    }

    const float TWO_PI = u2f(0x40c90fdbu);
    const float PI = u2f(0x40490fdbu);
    if (target != 0) {
        const float dt70 = lwc1(0x0015ed70u);
        const float vmax_y = fpu_mul(lwc1(0x0015ed6cu), TWO_PI);
        const float next_yaw =
            angle_seek_rate(pv + 0x24, lwc1(m + 0x48), yaw_goal, fpu_mul(dt70, TWO_PI), fpu_mul(dt70, PI), vmax_y);
        const float dt70_p = lwc1(0x0015ed70u);
        const float vmax_p = fpu_mul(lwc1(0x0015ed6cu), TWO_PI);
        swc1(m + 0x48, next_yaw);
        const float next_pitch =
            angle_seek_rate(pv + 0x28, lwc1(m + 0x44), pitch_goal, fpu_mul(dt70_p, TWO_PI), fpu_mul(dt70_p, PI), vmax_p);
        swc1(m + 0x44, next_pitch);
    } else {
        const float dt70 = lwc1(0x0015ed70u);
        const float vmax_y = fpu_mul(lwc1(0x0015ed6cu), u2f(0x4096cbe4u));
        const float next_yaw =
            angle_seek_rate(pv + 0x24, lwc1(m + 0x48), yaw_goal, fpu_mul(dt70, TWO_PI), fpu_mul(dt70, PI), vmax_y);
        const float dt70_p = lwc1(0x0015ed70u);
        const float vmax_p = fpu_mul(lwc1(0x0015ed6cu), u2f(0x4096cbe4u));
        swc1(m + 0x48, next_yaw);
        const float next_pitch =
            angle_seek_rate(pv + 0x28, lwc1(m + 0x44), pitch_goal, fpu_mul(dt70_p, TWO_PI), fpu_mul(dt70_p, PI), vmax_p);
        swc1(m + 0x44, next_pitch);
    }

    Vec4 vel{};
    vec_from_spherical_v(vel, lwc1(pv + 0x20), lwc1(m + 0x48), -lwc1(m + 0x44));
    vec3_set_length(V(OV(0x00173e80u)), vel, 1.0f);
    vec3_scale(V(OV(0x00173e80u)), V(OV(0x00173e80u)), -1.0f);

    // Exhaust smoke & spark trail (throttled to even frames under particle load)
    if (overloaded == 0 || (lwu(OV(0x0015f5ccu)) & 1u) == 0) {
        Vec4 rnd{0.0f, 0.0f, 0.0f, 0.0f};
        rnd.x = rand_range_f(-1.0f, 1.0f);
        rnd.y = rand_range_f(-1.0f, 1.0f);
        rnd.z = rand_range_f(-1.0f, 1.0f);
        Vec4 pvel = rnd;
        const float rlen = fpu_mul(rand_range_f(u2f(0x3dcccccd), u2f(0x3e4ccccd)), lwc1(0x0015ed6cu));
        vec3_set_length(pvel, pvel, rlen);

        const float back_hi = lwc1(0x0015ed6cu);
        const float back_lo = fpu_mul(back_hi, u2f(0x3dcccccd));
        Vec4 trail_pos{};
        vec3_set_length(trail_pos, vel, -rand_range_f(back_lo, back_hi));
        vec3_add(pvel, pvel, trail_pos);

        const float along = fpu_mul(rand_range_f(0.0f, 1.0f), lwc1(pv + 0x20));
        vec3_set_length(trail_pos, vel, along);
        vec3_add(trail_pos, trail_pos, V(m + 0x10));

        {
            EeStackFrame sf(0x20);
            V(sf) = trail_pos;
            V(sf + 0x10) = pvel;
            const uint32_t life1 = static_cast<uint32_t>(frame_scale_i(0x28));
            const uint32_t p1 = static_cast<uint32_t>(dev_2712b8(
                sf, sf + 0x10, life1, 0x32u, 0x00505050u, 3u, u2f(0x471c4000u), 1000.0f, 1.0f, u2f(0xb951b717u), 0.0f));
            if (p1 != 0) {
                const uint32_t tex = lbu(lwu(OV(0x001b20dcu)));
                sb(p1 + 3, 0x44);
                sb(p1 + 2, tex);
            }

            const uint32_t life2 = static_cast<uint32_t>(frame_scale_i(7));
            (void)dev_2712b8(m + 0x10, sf + 0x10, life2, 0x7fu, 0x00b0b0b0u, 3u, u2f(0x471c4000u), 1000.0f, 1.0f,
                             u2f(0xb9d1b717u), 0.0f);
        }

        Vec4 spark_vel{0.0f, 0.0f, u2f(0x3ca3d70au), 0.0f};
        mat3_transform(spark_vel, spark_vel, M(m + 0xc0));
        vec_rotate_axis(spark_vel, spark_vel, V(m + 0xc0), rand_angle());
        const uint32_t scol = color_swap_channels(0x4f007fffu, lbu(0x0013e52bu));
        const uint32_t slife = static_cast<uint32_t>(frame_scale_i(0x0f));
        (void)spawn_particle_15_v(trail_pos, spark_vel, scol, 0x1fffffffu, slife, 1u, u2f(0x46c35000u));
    }

    // Advance position & check world bounds
    const Vec4 prev_pos = V(m + 0x10);
    vec3_add_inplace(vel, hero_vel_scaled);
    vec3_add(V(m + 0x10), V(m + 0x10), vel);

    if (lwc1(m + 0x10) < 0.0f || lwc1(m + 0x14) < 0.0f || lwc1(m + 0x18) < 0.0f) {
        moby_list_remove(m, OV(0x001b0770u));
        moby_retire(m);
        return;
    }

    // Segment collision & lifetime/range check
    Vec4 hit_dir = vel;
    vec2_set_length(hit_dir, hit_dir, 1.0f);
    const uint32_t rec[9] = {
        f2u(hit_dir.x),
        f2u(hit_dir.y),
        f2u(1.0f),
        0x45afdf66u,
        m,
        0x00830000u,
        0x03u | (0x01u << 8) | (static_cast<uint32_t>(lhu(m + 0xa6)) << 16),
        f2u(3.0f),
        1u,
    };

    if (segment_collide_v(prev_pos, V(m + 0x10), 0u, lwu(HERO + 0x2080), rec) != 0) {
        const uint32_t outb = OV(0x00173e40u);
        const uint32_t hit = lwu(outb + 0x18);
        if (hit == 0) {
            if (lw(outb + 0x1c) > 0) {
                sb(m + 0xbc, 1);
                V(m + 0x10) = V(outb + 0x20);
                vec3_sub(tang, V(outb + 0x50), V(outb + 0x60));
                vec3_set_length(tang, tang, 1.0f);
                vec3_reflect(refl, vel, V(outb + 0x40));
                const float g2 = fpu_add(lwc1(0x0015ed6cu), lwc1(0x0015ed6cu));
                vec3_set_length(refl, refl, g2);
                vec3_set_length(norm, V(outb + 0x40), 1.0f);
            }
        } else if (hit != lwu(HERO + 0x2080) && hit != lwu(pv + 0x10)) {
            sb(m + 0xbc, 2);
            V(m + 0x10) = V(outb + 0x20);
            vec3_sub(tang, V(outb + 0x50), V(outb + 0x60));
            direct_hit_moby = m;
            vec3_set_length(tang, tang, 1.0f);
            vec3_reflect(refl, vel, V(outb + 0x40));
            const float g2 = fpu_add(lwc1(0x0015ed6cu), lwc1(0x0015ed6cu));
            vec3_set_length(refl, refl, g2);
            vec3_set_length(norm, V(outb + 0x40), 1.0f);
        }
    } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0 ||
               lwc1(pv + 0x48) < vec2_distance(V(m + 0x10), V(HERO + 0x80))) {
        norm = V(OV(0x00160680u));
        tang = V(OV(0x00160660u));
        mat3_transform(tang, tang, M(m + 0xc0));
        mat3_transform(norm, norm, M(m + 0xc0));
        if (lbu(0x0013e52bu) == 0) {
            moby_list_remove(m, OV(0x001b0770u));
            moby_retire(m);
            return;
        }
        sb(m + 0xbc, 1);
        vec3_add(refl, refl, vel);
    }

    // Detonation & Gold Devastator bounce
    if (lbu(m + 0xbc) != 0) {
        EeStackFrame sf(0x50);

        const float blast_rad = fpu_add(gold_scale, gold_scale);
        const int32_t n_hit = mobys_in_sphere_v(V(m + 0x10), blast_rad, 0x10u, m, nullptr);
        moby_list_hit_v(m, V(m + 0x10), OV(0x00177f00u), n_hit, direct_hit_moby, 0x00830000u, 3u, 1u, 3.0f, 1.0f, 1.0f);

        Mat4 K{}, K2{};
        K.r[0] = Vec4{0.0f, norm.z, -norm.y, 0.0f};
        K.r[1] = Vec4{-norm.z, 0.0f, norm.x, 0.0f};
        K.r[2] = Vec4{norm.y, -norm.x, 0.0f, 0.0f};
        K.r[3] = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        mat4_mul(K2, K, K);

        uint32_t p_count = 10u;
        uint32_t low_lod = 0u;
        if (overloaded != 0) {
            low_lod = 1u;
            p_count = static_cast<uint32_t>(ee_div(static_cast<int32_t>(p_count), 3));
        }

        if (p_count != 0) {
            const uint32_t life_trim = low_lod * 0x23u;
            uint32_t rem = p_count;
            do {
                Vec4 fvel{};
                if (lbu(m + 0xbc) == 1u) {
                    const float ang = rand_angle();
                    const float one_minus_c = fpu_sub(1.0f, cos_vu(ang));
                    Mat4 m_k2{}, m_k{}, rot_m{};
                    mat4_scale_vu(m_k2, K2, one_minus_c);
                    mat4_scale_vu(m_k, K, sin_vu(ang));
                    mat4_add_vu(m_k, m_k, m_k2);
                    mat4_identity(m_k2);
                    mat4_add_vu(rot_m, m_k, m_k2);
                    mat4_transform(fvel, tang, rot_m);
                    const float rspd =
                        fpu_mul(fpu_mul(rand_range_f(8.5f, 16.5f), lwc1(0x0015ed6cu)), gold_scale);
                    vec3_set_length(fvel, fvel, rspd);
                } else {
                    const float rspd =
                        fpu_mul(fpu_mul(rand_range_f(8.5f, 16.5f), lwc1(0x0015ed6cu)), gold_scale);
                    const float ryaw = rand_angle();
                    const float rpitch = rand_angle();
                    vec_from_spherical_v(fvel, rspd, ryaw, rpitch);
                }

                fvel.z = fpu_add(fvel.z, fpu_mul(lwc1(0x0015ed6cu), 5.0f));
                vec3_add(fvel, fvel, refl);

                const uint32_t c0 = color_swap_channels(0x4f007fffu, lbu(0x0013e52bu));
                const uint32_t c1 = color_swap_channels(0x1f00007fu, lbu(0x0013e52bu));
                const float flen = vec3_length(fvel);
                const float gs = lwc1(0x0015ed6cu);
                if (1.0f < flen) {
                    vec3_set_length(fvel, fvel, fpu_mul(gs, 8.0f));
                }

                --rem;
                const float fsz = fpu_mul(gold_scale, u2f(0x471c4000u));
                const int32_t lo_l = frame_scale_i(0x3c);
                const int32_t hi_l = frame_scale_i(0x78);
                const uint32_t flife = static_cast<uint32_t>(rand_range(lo_l, hi_l)) - life_trim;
                V(sf) = fvel;
                (void)spawn_particle_0f(m + 0x10, sf, c0, c1, flife, 1u, -1, -1, fsz);
            } while (rem != 0);
        }

        // Debris 0x7a biased toward camera
        Vec4 debris_vel{0.0f, 0.0f, 0.0f, 0.0f};
        debris_vel.x = rand_range_f(-1.0f, 1.0f);
        debris_vel.y = rand_range_f(-1.0f, 1.0f);
        debris_vel.z = rand_range_f(-1.0f, 1.0f);

        Vec4 to_cam{};
        vec3_sub(to_cam, V(OV(0x00166dc0u)), V(m + 0x10));
        const float cam_dist = vec3_length(to_cam);
        to_cam.z = fpu_add(to_cam.z, fpu_mul(cam_dist, 0.5f));
        vec3_set_length(debris_vel, debris_vel,
                        fpu_mul(fpu_mul(fpu_div(cam_dist, 5.0f), lwc1(0x0015ed6cu)), gold_scale));
        vec3_set_length(to_cam, to_cam, fpu_mul(fpu_add(cam_dist, cam_dist), lwc1(0x0015ed6cu)));
        vec3_add(debris_vel, debris_vel, to_cam);
        (void)vec3_clamp_length_vu(debris_vel, debris_vel, fpu_mul(lwc1(0x0015ed6cu), 10.0f));
        const uint32_t dlife = static_cast<uint32_t>(rand_range(frame_scale_i(0x3c), frame_scale_i(0x5a)));
        (void)debris_7a_spawn_v(V(m + 0x10), debris_vel, dlife, 0u, 0u);

        int32_t burst_n = (cam_dist < 6.0f) ? (float_to_int(cam_dist) / 2) : 3;
        const float close_bias = (cam_dist < 7.0f) ? fpu_sub(7.0f, cam_dist) : 0.0f;
        if (overloaded != 0) {
            burst_n /= 2;
        }

        if (burst_n > 0) {
            const uint32_t trim10 = low_lod * 10u;
            const uint32_t trim7 = low_lod * 7u;
            const uint32_t trim5 = low_lod * 5u;
            for (int32_t i = burst_n; i != 0; --i) {
                const float r_spd = fpu_mul(rand_range_f(8.0f, 10.0f), gold_scale);
                const float gs = lwc1(0x0015ed6cu);
                const float p_spd = fpu_sub(fpu_mul(r_spd, gs), fpu_mul(close_bias, gs));

                const uint32_t i0 = static_cast<uint32_t>(rand_mod(6));
                const uint32_t c0 = color_swap_channels(lwu(OV(0x001e99f0u) + (i0 << 2)), lbu(0x0013e52bu));
                const uint32_t i1 = static_cast<uint32_t>(rand_mod(6));
                const uint32_t c1 = color_swap_channels(lwu(OV(0x001e9a08u) + (i1 << 2)), lbu(0x0013e52bu));

                const uint32_t cnt1 =
                    static_cast<uint32_t>(rand_range(frame_scale_i(0x0f), frame_scale_i(0x14))) - trim7;
                const uint32_t b38_1 =
                    static_cast<uint32_t>(rand_range(frame_scale_i(0x19), frame_scale_i(0x1e))) - trim10;
                spawn_particle_0b_v(V(m + 0x10), refl, c0, c1, cnt1, b38_1, 0u, 0u, u2f(0x48c35000u), p_spd);

                const uint32_t cnt2 = static_cast<uint32_t>(rand_range(frame_scale_i(5), frame_scale_i(0x0a))) - trim5;
                const uint32_t b38_2 =
                    static_cast<uint32_t>(rand_range(frame_scale_i(0x0f), frame_scale_i(0x14))) - trim7;
                spawn_particle_0b_v(V(m + 0x10), refl, 0x7fffffffu, 0x00ffffffu, cnt2, b38_2, 0u, 0u, u2f(0x48c35000u),
                                    fpu_mul(p_spd, 0.5f));
            }
        }

        if (p_count != 0) {
            const uint32_t trim10 = low_lod * 10u;
            for (uint32_t i = p_count; i != 0; --i) {
                Vec4 svel{0.0f, 0.0f, 0.0f, 0.0f};
                svel.x = rand_range_f(-1.0f, 1.0f);
                svel.y = rand_range_f(-1.0f, 1.0f);
                svel.z = rand_range_f(-1.0f, 1.0f);
                const float slen = fpu_mul(fpu_mul(rand_range_f(0.0f, 3.0f), lwc1(0x0015ed6cu)), gold_scale);
                vec3_set_length(svel, svel, slen);

                const uint32_t i0 = static_cast<uint32_t>(rand_mod(6));
                const uint32_t c0 = color_swap_channels(lwu(OV(0x001e99f0u) + (i0 << 2)), lbu(0x0013e52bu));
                const uint32_t i1 = static_cast<uint32_t>(rand_mod(6));
                const uint32_t c1 = color_swap_channels(lwu(OV(0x001e9a08u) + (i1 << 2)), lbu(0x0013e52bu));
                const float ssz = fpu_mul(gold_scale, u2f(0x48435000u));
                const int32_t slife =
                    rand_range(frame_scale_i(0x14), frame_scale_i(0x23)) - static_cast<int32_t>(trim10);
                V(sf) = svel;
                spawn_particle_08(m + 0x10, sf, c0, c1, slife, ssz);
            }
        }

        if (lwc1(OV(0x0015f5d0u)) < u2f(0x3f733333u) && 9.0f < cam_dist) {
            const uint32_t ch = lbu(0x0013e52bu) != 0 ? 0x3cu : 0x7fu;
            const float sh_scale = fpu_mul(gold_scale, 4.0f);
            (void)shard_spawn_v(m, V(m + 0x10), refl, static_cast<uint32_t>(frame_scale_i(0x0f)), ch, 0x7fu, ch, 0x20u,
                                sh_scale);
            (void)shard_spawn_v(m, V(m + 0x10), refl, static_cast<uint32_t>(frame_scale_i(0x18)), ch, 0x7fu, ch, 0x20u,
                                sh_scale);
        }

        const float sh_scale2 = fpu_mul(gold_scale, 4.0f);
        const uint32_t ch2 = lbu(0x0013e52bu) != 0 ? 0x20u : 0x7fu;
        (void)shard_spawn_v(m, V(m + 0x10), refl, static_cast<uint32_t>(frame_scale_i(0x14)), ch2, 0x7fu, 0u, 0x30u,
                            sh_scale2);
        if (overloaded == 0) {
            (void)shard_spawn_v(m, V(m + 0x10), refl, static_cast<uint32_t>(frame_scale_i(0x13)), 0xffu, 0xffu, 0xffu,
                                0x20u, blast_rad);
        }

        // Note: original binary stores 0 to sp+0xf0 (vel.x) here before camera shake and Gold bounce reflect.
        vel.x = 0.0f;
        const uint32_t cam_base = OV(0x00166c80u);
        sb(m + 0xbc, 0);
        if (cam_dist < 20.0f) {
            swc1(cam_base + 0x160, fpu_sub(u2f(0x3ecccccdu), fpu_mul(cam_dist, u2f(0x3c8f5c29u))));
        } else {
            swc1(cam_base + 0x160, u2f(0x3d4cccd0u));
        }
        sw(cam_base + 0x168, static_cast<uint32_t>(frame_scale_i(0x19)));
        (void)moby_sound_start(0, 0, m);

        if (overloaded == 0) {
            const uint32_t src_def = (lbu(0x0013e52bu) != 0) ? OV(0x001e99a0u) : OV(0x001e9950u);
            for (uint32_t i = 0; i < 0x50u; i += 4u) {
                sw(sf + i, lwu(src_def + i));
            }
            (void)effect_spawn(sf, m + 0x10, 0u, 0u);
        }

        const uint32_t bounces = lwu(pv + 0x40) + 1u;
        sw(pv + 0x40, bounces);
        if (lbu(0x0013e52bu) == 0 || bounces == 3u) {
            moby_list_remove(m, OV(0x001b0770u));
            moby_retire(m);
            return;
        }
        // the target let go before the ricochet's search (0x2b1990: the delay slot of the bounce count's branch-likely, run only
        // when it bounces); the search below sets a new one or leaves none
        sw(pv + 0x18, 0);

        // Gold Devastator ricochet
        const uint32_t out_norm = OV(0x00173e80u);
        const uint32_t outb = out_norm - 0x40u;
        sb(m + 0xbc, 0);
        vec3_set_length(V(out_norm), V(out_norm), 1.0f);

        Vec4 bounce_dir{};
        vec3_reflect(bounce_dir, vel, V(out_norm));
        swc1(m + 0x48, atan2_f(bounce_dir.x, bounce_dir.y));
        swc1(m + 0x44, -atan2_f(vec2_length(bounce_dir), bounce_dir.z));
        sw(pv + 0x24, 0);
        sw(pv + 0x28, 0);

        Vec4 push_out{};
        vec3_set_length(push_out, V(out_norm), u2f(0x3d4ccccdu));
        vec3_add(V(m + 0x10), V(m + 0x10), push_out);

        const uint32_t hit_moby = lwu(outb + 0x18);
        if (hit_moby != 0) {
            sw(pv + 0x10, hit_moby);
        }
        if (lwu(pv + 0x44) == 0) {
            sw(pv + 0x44, static_cast<uint32_t>(frame_scale_i(0x5a)));
        }
        rocket_2affa8(m, pv, lwu(outb + 0x18));
        (void)moby_sound_start(1, 0, m);
    }

    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x44))) != 0) {
        sw(pv + 0x40, 0);
    }
}

// orig: (none)
void rocket_fns_register() {
    ovl_register_moby_fn(0x002b03a0, devastator_rocket_update);
}

}  // namespace rac1
