// Ported overlay code: the Glove of Doom's homing agents ("doomlets", moby class 0xba, update 0x002bffa8)
// and shared walker-approach / jump helpers (0x00259028, 0x0025b440).
#include <game/ovl/doomlet.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/blarg_call.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {

constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t HERO_MOBY_PTR = 0x001413d0u;
constexpr uint32_t DOOM_GOLD_ADDR = 0x0013e534u;
constexpr uint32_t DOOMLET_WALK_REC = 0x00141210u;
constexpr uint32_t DOOMLET_JUMP_REC = 0x00141260u;

// orig: (helper) world-coordinate bounds check [2.0f, 1021.0f] along x, y, z in doomlet_update
inline bool in_world_bounds(uint32_t m) {
    const float lo = 2.0f;
    const float hi = u2f(0x447f4000u);
    const float x = lwc1(m + 0x10);
    if (x < lo || hi < x) return false;
    const float y = lwc1(m + 0x14);
    if (y < lo || hi < y) return false;
    const float z = lwc1(m + 0x18);
    if (z < lo || hi < z) return false;
    return true;
}

// orig: (helper) Vec4 form of doomlet_2bf8b0 (0x002bf8b0)
void doomlet_2bf8b0_impl(uint32_t p0, uint32_t p1) {
    const float d2 = vec2_distance(V(p0 + 0x10), V(p1));
    float dist_term = fpu_mul(d2, d2);
    float rise = abs_f(fpu_sub(lwc1(p1 + 0x8), lwc1(p0 + 0x18)));
    float speed = fpu_mul(lwc1(p1 + 0x1c), u2f(0x420c0000u));
    const float max_speed = lwc1(p1 + 0x20);
    rise = fpu_add(rise, 1.0f);
    speed = fpu_mul(speed, max_speed);
    dist_term = fpu_add(dist_term, 0.25f);
    speed = fpu_div(speed, rise);
    speed = fpu_mul(speed, dist_term);
    swc1(p1 + 0x14, speed);
    if (max_speed < speed) {
        swc1(p1 + 0x14, max_speed);
    }

    const float yaw = atan2_f(fpu_sub(lwc1(p1 + 0x0), lwc1(p0 + 0x10)), fpu_sub(lwc1(p1 + 0x4), lwc1(p0 + 0x14)));
    const float neg_g = -lwc1(p1 + 0x1c);
    const float spd = lwc1(p1 + 0x14);
    swc1(p1 + 0x18, yaw);
    const float vz = sub_0025abf0(p0 + 0x10, p1, nullptr, spd, neg_g);
    const float cap = fpu_mul(lwc1(0x0015ed6cu), 7.0f);
    swc1(p1 + 0x10, vz);
    if (cap < vz) {
        swc1(p1 + 0x10, cap);
    }

    const float ratio = fpu_div(vz, lwc1(p1 + 0x10));
    const float cur_spd = lwc1(p1 + 0x14);
    sb(p1 + 0x2f, 0);
    swc1(p1 + 0x14, fpu_mul(cur_spd, ratio));
}

// orig: (helper) Vec4 form of doomlet_2bf8b0 (0x002bf8b0)
void doomlet_2bf8b0_v(uint32_t p0, uint32_t p1, const Vec4& target) {
    sw(p1 + 0x30, 0);
    V(p1) = target;
    doomlet_2bf8b0_impl(p0, p1);
}

// orig: (helper) Vec4 form of doomlet_2bec80 (0x002bec80)
float doomlet_2bec80_v(const Vec4& pos, uint32_t p1) {
    Vec4 end = pos;
    Vec4 start = pos;
    const float z0 = fpu_sub(end.z, 5.0f);
    const float min_z = u2f(0x3dcccccdu);
    const float z1 = (z0 < min_z) ? min_z : z0;
    end.z = z1;
    start.z = fpu_add(z1, 6.0f);
    if (segment_collide_v(start, end, 6, p1, nullptr) == 0) {
        return 0.0f;
    }

    const uint32_t rec = OV(0x00173e40u);
    const uint32_t hit = lwu(rec + 0x18);
    if (hit != 0) {
        const uint32_t ci = lwu(hit + 0x24);
        if (ci != 0 && lh(ci + 0x46) == 5) {
            return lwc1(rec + 0x28);
        }
    }

    // the surface kind of the hit (the original's jump table 0x1e9ad0): kinds 0, 1, 3, 8, 11, 12 and 13 are no floor
    switch (static_cast<uint32_t>(sub_001f35d8())) {
    case 0: case 1: case 3: case 8: case 11: case 12: case 13:  // (0x002bed78)
        return 0.0f;
    default:  // 2, 4..7, 9, 10 (0x002bed6c), and kinds past the table
        break;
    }
    return lwc1(OV(0x00173e40u) + 0x28u);
}

// orig: (helper) Vec4 form of doomlet_2beab0 (0x002beab0)
int32_t doomlet_2beab0_v(uint32_t p0, uint32_t p1, const Vec4* p2) {
    const float upg = int_to_float(static_cast<int32_t>(lbu(DOOM_GOLD_ADDR)));
    const float scale = fpu_add(fpu_mul(upg, 0.5f), 1.0f);
    Vec4 pos = (p2 != nullptr) ? *p2 : V(p0 + 0x10);
    const float dz = fpu_mul(scale, u2f(0x3e4ccccdu));
    const float radius = fpu_mul(scale, u2f(0x3e4cb296u));
    pos.z = fpu_add(pos.z, dz);
    const int32_t hit = sphere_collide_v(pos, radius, p1, p0);
    if (hit != 0) {
        const uint32_t rec = OV(0x00173e40u);
        swc1(rec + 0x38, fpu_sub(lwc1(rec + 0x38), dz));
    }
    return hit;
}

// orig: (helper) Vec4 form of doomlet_2bf810 (0x002bf810)
void doomlet_2bf810_v(const Vec4& a, const Vec4& b, uint32_t n) {
    Vec4 step{};
    vec3_sub(step, b, a);
    const float inv = fpu_div(1.0f, int_to_float(static_cast<int32_t>(n)));
    vec3_scale(step, step, inv);
    Vec4 cur = a;
    for (int32_t i = static_cast<int32_t>(n); i > 0; --i) {
        vec3_add(cur, cur, step);
    }
}

// orig: (helper) Vec4 form of doomlet_2bf9f0 (0x002bf9f0)
int32_t doomlet_2bf9f0_v(uint32_t p0, uint32_t p1, const Vec4& target, uint32_t p3) {
    Vec4 step{};
    vec3_sub(step, target, V(p0 + 0x10));
    const float half = 0.5f;
    const float max_d = fpu_div(fpu_mul(lwc1(p0 + 0x2c), u2f(0x40a66666u)), lwc1(lwu(p0 + 0x24) + 0x24));
    (void)vec2_clamp_length(step, step, max_d);

    Vec4 landing{};
    vec3_add(landing, V(p0 + 0x10), step);

    Vec4 midpoint{};
    vec3_scale(midpoint, step, half);
    const float mid_len = vec2_length(midpoint);
    vec3_add(midpoint, V(p0 + 0x10), midpoint);
    const bool can_halve = half < mid_len;

    const auto retry = [&]() -> int32_t {
        return can_halve ? doomlet_2bf9f0_v(p0, p1, midpoint, 1) : 0;
    };

    const float floor_z = doomlet_2bec80_v(landing, p0);
    const float dz = fpu_sub(floor_z, lwc1(p0 + 0x18));
    landing.z = floor_z;
    step.z = dz;
    if (floor_z == 0.0f) return retry();

    const float max_rise = fpu_div(fpu_mul(lwc1(p0 + 0x2c), 3.0f), lwc1(lwu(p0 + 0x24) + 0x24));
    if (max_rise < dz) return retry();

    Vec4 probe = V(OV(0x00173e80u));
    vec3_set_length(probe, probe, 1.0f);
    const float n_xy = fpu_add(abs_f(probe.x), abs_f(probe.y));
    if (abs_f(probe.z) < n_xy) return retry();

    const float k025 = u2f(0x3ccccccdu);
    const float lift = fpu_div(fpu_mul(lwc1(p0 + 0x2c), k025), lwc1(lwu(p0 + 0x24) + 0x24));
    landing.z = fpu_add(landing.z, lift);
    if (doomlet_2beab0_v(p0, 6, &landing) != 0) return retry();

    const float sc = lwc1(p0 + 0x2c);
    const uint32_t ci = lwu(p0 + 0x24);
    const float drop = fpu_div(fpu_mul(sc, k025), lwc1(ci + 0x24));
    landing.z = fpu_sub(landing.z, drop);

    Vec4 from_top = V(p0 + 0x10);
    const float h_scaled = fpu_mul(sc, u2f(0x3e4ccccdu));
    from_top.z = fpu_add(from_top.z, fpu_div(h_scaled, lwc1(ci + 0x24)));

    Vec4 land_top = landing;
    land_top.z = fpu_add(land_top.z, fpu_div(h_scaled, lwc1(ci + 0x24)));
    if (p3 != 0 && segment_collide_v(from_top, land_top, 6, p0, nullptr) == 0) {
        return retry();
    }

    doomlet_2bec00(p1);
    doomlet_2bf8b0_v(p0, DOOMLET_JUMP_REC, landing);
    doomlet_2bec48(p1);

    vec3_scale(probe, step, 0.5f);
    vec3_add(probe, probe, V(p0 + 0x10));

    Vec4 arc{};
    arc.z = lwc1(p1 + 0x50);
    probe.z = sub_00259468_v(V(p0 + 0x10), arc, nullptr, lwc1(0x0014127cu));

    doomlet_2bf810_v(from_top, probe, 10);
    doomlet_2bf810_v(land_top, probe, 10);

    if (segment_collide_v(from_top, probe, 6, p0, nullptr) != 0) {
        if (lwu(OV(0x00173e58u)) != lwu(p1 + 0x38)) return retry();
    }
    if (segment_collide_v(probe, land_top, 6, p0, nullptr) != 0) {
        if (lwu(OV(0x00173e58u)) != lwu(p1 + 0x38)) return retry();
    }

    sb(p0 + 0xbc, 0xd);
    return 1;
}

// orig: (helper) Vec4 form of doomlet_2bed90 (0x002bed90)
int32_t doomlet_2bed90_v(uint32_t p0, const Vec4& origin, uint32_t p2, uint32_t /*p3*/, uint32_t p4) {
    uint32_t close_flag = 1;
    uint32_t best_moby = 0;
    float best_score = u2f(0x4e6e6b28u);

    uint32_t cur = lwu(OV(0x001aba00u));
    if (cur != 0) {
        uint32_t list_ptr = OV(0x001aba00u);
        do {
            const uint32_t ci = lwu(cur + 0x24);
            const int32_t grp = (ci != 0) ? lh(ci + 0x46) : 0;
            if (grp != 5) {
                list_ptr += 4;
            } else {
                const float dist = vec3_distance(origin, V(cur + 0x10));
                if (u2f(0x42200000u) < dist) {
                    list_ptr += 4;
                } else {
                    if (u2f(0x41f00000u) < dist) {
                        close_flag = 0;
                    }
                    Vec4 pt = V(cur + 0x10);
                    pt.z = fpu_add(pt.z, u2f(0x3ecccccdu));

                    const float yaw_to = atan2_f(fpu_sub(pt.x, origin.x), fpu_sub(pt.y, origin.y));
                    const float dyaw = angle_dist(lwc1(p2 + 0x8), yaw_to);
                    const float yaw_pen = fpu_mul(dyaw, dyaw);

                    const float pitch_to = atan2_f(dist, fpu_sub(pt.z, origin.z));
                    const float dpitch = angle_dist(lwc1(p2 + 0x4), pitch_to);
                    const float pitch_pen = fpu_mul(dpitch, dpitch);

                    uint32_t occluded;
                    if (close_flag == 0) {
                        occluded = (lbu(cur + 0x31) < 1u) ? 1u : 0u;
                    } else if (8.0f < abs_f(fpu_sub(lwc1(cur + 0x18), lwc1(p0 + 0x18)))) {
                        occluded = 1u;
                    } else {
                        occluded = (segment_collide_v(origin, pt, 6, cur, nullptr) != 0) ? 1u : 0u;
                    }

                    if (p4 == 0 && occluded != 0) {
                        list_ptr += 4;
                    } else {
                        float d3 = vec3_distance(origin, pt);
                        if (u2f(0x40500000u) < fpu_sub(lwc1(cur + 0x18), lwc1(p0 + 0x18))) {
                            d3 = fpu_add(d3, 15.0f);
                        }
                        float score = fpu_add(d3, (occluded == 0) ? 0.0f : 5.0f);
                        score = fpu_add(score, pitch_pen);
                        score = fpu_add(score, yaw_pen);
                        list_ptr += 4;
                        if (score < best_score) {
                            best_score = score;
                            best_moby = cur;
                        }
                    }
                }
            }
            cur = lwu(list_ptr);
        } while (cur != 0);
    }

    if (best_moby == 0) {
        if (u2f(0x404ccccdu) < vec2_distance(V(p0 + 0x10), V(HERO_POS))) {
            best_moby = lwu(HERO_MOBY_PTR);
        }
    }
    return static_cast<int32_t>(best_moby);
}

// orig: (helper) host-reference form of doomlet_2bf050 (0x002bf050)
float doomlet_2bf050_v(uint32_t p0, const Vec4& origin, uint32_t p2, float& out_dist, float pf12, float pf13, float pf14, float pf15, float pf16, float pf17) {
    const float step_ang = fpu_div(pf13, pf14);
    float best_ang = pf12;
    const int32_t count = float_to_int(pf14);
    float best_dist = 0.0f;
    const uint32_t pv = lwu(p0 + 0x78);
    float best_adist = u2f(0x40c90fdbu);
    out_dist = 0.0f;

    float ref_ang;
    if (p2 != 0) {
        ref_ang = atan2_f(fpu_sub(lwc1(p2 + 0x0), lwc1(p0 + 0x10)), fpu_sub(lwc1(p2 + 0x4), lwc1(p0 + 0x14)));
    } else {
        ref_ang = best_ang;
    }

    float cur_ang = fpu_mul(pf13, 0.5f);
    (void)angle_add(pf12, cur_ang);
    cur_ang = angle_sub(pf12, cur_ang);

    Vec4 ray_from = origin;
    ray_from.y = fpu_add(ray_from.y, fpu_mul(sin_vu(ref_ang), pf17));
    ray_from.x = fpu_add(ray_from.x, fpu_mul(cos_vu(ref_ang), pf17));
    ray_from.z = fpu_add(ray_from.z, pf15);

    Vec4 ray_to = origin;
    const float z_far = fpu_add(pf15, pf15);
    ray_to.y = fpu_add(ray_to.y, fpu_mul(sin_vu(ref_ang), pf16));
    ray_to.x = fpu_add(ray_to.x, fpu_mul(cos_vu(ref_ang), pf16));
    ray_to.z = fpu_add(ray_to.z, z_far);

    const auto hit_is_enemy = [&](uint32_t hit) -> bool {
        if (hit == lwu(pv + 0x38)) return true;
        if (hit == 0) return false;
        const uint32_t ci = lwu(hit + 0x24);
        return ci != 0 && lh(ci + 0x46) == 5;
    };

    if (segment_collide_v(ray_from, ray_to, 4, p0, nullptr) == 0) {
        if (doomlet_2bec80_v(ray_to, p0) != 0.0f) {
            best_ang = ref_ang;
            best_dist = pf16;
            out_dist = pf16;
        }
    } else {
        if (hit_is_enemy(lwu(OV(0x00173e58u)))) {
            out_dist = 100.0f;
            return ref_ang;
        }
        best_ang = ref_ang;
        const float d = vec3_distance(ray_from, V(OV(0x00173e60u)));
        best_adist = 0.0f;
        const float prev_out = out_dist;
        best_dist = d;
        if (prev_out < best_dist) {
            out_dist = best_dist;
        }
    }

    for (int32_t i = 0; i < count; ++i) {
        ray_from = origin;
        ray_from.y = fpu_add(ray_from.y, fpu_mul(sin_vu(cur_ang), pf17));
        ray_from.x = fpu_add(ray_from.x, fpu_mul(cos_vu(cur_ang), pf17));
        ray_from.z = fpu_add(ray_from.z, pf15);

        ray_to = origin;
        ray_to.y = fpu_add(ray_to.y, fpu_mul(sin_vu(cur_ang), pf16));
        ray_to.x = fpu_add(ray_to.x, fpu_mul(cos_vu(cur_ang), pf16));
        ray_to.z = fpu_add(ray_to.z, z_far);

        if (segment_collide_v(ray_from, ray_to, 4, p0, nullptr) == 0) {
            if (doomlet_2bec80_v(ray_to, p0) != 0.0f) {
                const float ad = angle_dist(cur_ang, ref_ang);
                if (ad < best_adist || best_dist < pf16) {
                    const float prev_out = out_dist;
                    best_adist = ad;
                    best_ang = cur_ang;
                    best_dist = pf16;
                    if (prev_out < pf16) {
                        out_dist = pf16;
                    }
                }
            }
        } else {
            if (hit_is_enemy(lwu(OV(0x00173e40u) + 0x18u))) {
                out_dist = 100.0f;
                return cur_ang;
            }
            const float d = vec3_distance(ray_from, V(OV(0x00173e60u)));
            if (best_dist < d) {
                best_ang = cur_ang;
                best_dist = d;
                const float ad = angle_dist(best_ang, ref_ang);
                const float prev_out = out_dist;
                best_adist = ad;
                if (prev_out < d) {
                    out_dist = d;
                }
            } else if (d == best_dist) {
                const float ad = angle_dist(cur_ang, ref_ang);
                if (ad < best_adist) {
                    const float prev_out = out_dist;
                    best_adist = ad;
                    best_ang = cur_ang;
                    best_dist = d;
                    if (prev_out < d) {
                        out_dist = d;
                    }
                }
            }
        }
        cur_ang = angle_add(cur_ang, step_ang);
    }

    float side_ang = angle_sub(best_ang, u2f(0x3fc90fdbu));
    ray_from = origin;
    ray_from.y = fpu_add(ray_from.y, fpu_mul(sin_vu(side_ang), pf17));
    ray_from.x = fpu_add(ray_from.x, fpu_mul(cos_vu(side_ang), pf17));
    ray_from.z = fpu_add(ray_from.z, fpu_mul(pf15, 0.125f));
    ray_to = ray_from;
    ray_to.y = fpu_add(ray_to.y, fpu_mul(sin_vu(best_ang), pf16));
    ray_to.x = fpu_add(ray_to.x, fpu_mul(cos_vu(best_ang), pf16));
    ray_to.z = fpu_add(ray_to.z, u2f(0x3df5c28fu));
    if (segment_collide_v(ray_from, ray_to, 4, p0, nullptr) != 0 && !hit_is_enemy(lwu(OV(0x00173e58u)))) {
        const float d = vec3_distance(ray_from, V(OV(0x00173e60u)));
        const float num = fpu_mul(fpu_sub(pf16, d), u2f(0x40490fdbu));
        const float den = fpu_mul(pf16, 3.0f);
        best_ang = angle_add(best_ang, fpu_div(num, den));
    }

    side_ang = angle_add(side_ang, u2f(0x40490fdbu));
    ray_from = origin;
    ray_from.y = fpu_add(ray_from.y, fpu_mul(sin_vu(side_ang), pf17));
    ray_from.x = fpu_add(ray_from.x, fpu_mul(cos_vu(side_ang), pf17));
    ray_from.z = fpu_add(ray_from.z, fpu_mul(pf15, 0.125f));
    ray_to = ray_from;
    ray_to.y = fpu_add(ray_to.y, fpu_mul(sin_vu(best_ang), pf16));
    ray_to.x = fpu_add(ray_to.x, fpu_mul(cos_vu(best_ang), pf16));
    ray_to.z = fpu_add(ray_to.z, u2f(0x3df5c28fu));
    if (segment_collide_v(ray_from, ray_to, 4, p0, nullptr) != 0 && !hit_is_enemy(lwu(OV(0x00173e58u)))) {
        const float d = vec3_distance(ray_from, V(OV(0x00173e60u)));
        const float num = fpu_mul(fpu_sub(pf16, d), u2f(0x40490fdbu));
        const float den = fpu_mul(pf16, 3.0f);
        best_ang = angle_sub(best_ang, fpu_div(num, den));
    }

    if (u2f(0x4020d97cu) < angle_dist(best_ang, lwc1(p0 + 0x48))) {
        const float diff = angle_sub(best_ang, lwc1(p0 + 0x48));
        best_ang = angle_add(lwc1(p0 + 0x48), fpu_mul(diff, 0.5f));
    }
    return best_ang;
}

}  // namespace

// orig: 0x002bfec8
int32_t doomlet_2bfec8(uint32_t p0) {
    int32_t s0 = lbu(p0 + 0x20);
    Vec4 end = V(p0 + 0x10);
    Vec4 start = V(p0 + 0x10);
    end.z = fpu_sub(end.z, u2f(0x3e99999au));
    start.z = fpu_add(start.z, 1.0f);
    if (segment_collide_v(start, end, 6, p0, nullptr) == 0) {
        return s0;
    }

    const uint32_t hit = lwu(OV(0x00173e58u));
    if (hit != 0) {
        const uint32_t ci = lwu(hit + 0x24);
        if (ci != 0 && lh(ci + 0x46) == 5) {
            return 0xb;
        }
    }

    // the surface kind under it (the original's jump table 0x1e9b10): kinds 0, 1, 3, 8, 11, 12 and 13 blow it up
    switch (static_cast<uint32_t>(sub_001f35d8())) {
    case 0: case 1: case 3: case 8: case 11: case 12: case 13:  // (0x002bff90)
        s0 = 0xb;
        break;
    default:  // 2, 4..7, 9, 10 (0x002bff94), and kinds past the table
        break;
    }
    return s0;
}

// orig: 0x002beb90
void doomlet_2beb90(uint32_t p0) {
    sw(P + 0x1ed0u, lwu(p0 + 0x40));
    sw(P + 0x1ed4u, lwu(p0 + 0x44));
    sw(P + 0x1ed8u, lwu(p0 + 0x48));
    sw(P + 0x1f08u, lwu(p0 + 0x4c));
    sw(P + 0x1ee4u, lwu(p0 + 0x5c));
}

// orig: 0x0025b440
int32_t doomlet_25b440(uint32_t p0, uint32_t p1) {
    const uint32_t st = lbu(p1 + 0x2f);
    switch (st) {
    case 0:
        if (lbu(p0 + 0x52) != lbu(p1 + 0x2c)) {
            return static_cast<int32_t>(lbu(p1 + 0x2f));
        }
        if (moby_anim_time(p0) <= lwc1(p1 + 0x24)) {
            return static_cast<int32_t>(lbu(p1 + 0x2f));
        }
        if ((lbu(p0 + 0x70) & 2u) != 0) {
            moby_anim_set(p0, lbu(p1 + 0x2d), 0, 0xa);
            sb(p1 + 0x2f, 1);
        }
        [[fallthrough]];
    case 1: {
        yaw_turn_damped(p0, p1 + 0x30, lwc1(p1 + 0x18), lwc1(p1 + 0x34), lwc1(p1 + 0x38), lwc1(p1 + 0x3c));
        const float dx = fpu_mul(cos_vu(lwc1(p1 + 0x18)), lwc1(p1 + 0x14));
        const float dy = fpu_mul(sin_vu(lwc1(p1 + 0x18)), lwc1(p1 + 0x14));
        const Vec4 delta{dx, dy, lwc1(p1 + 0x10), 0.0f};
        vec3_add(V(p0 + 0x10), V(p0 + 0x10), delta);

        const float vz = fpu_sub(lwc1(p1 + 0x10), lwc1(p1 + 0x1c));
        swc1(p1 + 0x10, vz);
        if (vz < 0.0f) {
            const float frames = frame_scale_f(fpu_add(fpu_add(lwc1(p1 + 0x28), lwc1(p1 + 0x28)), 10.0f));
            float half_g = fpu_mul(lwc1(p1 + 0x1c), 0.5f);
            float v_term = fpu_mul(lwc1(p1 + 0x10), frames);
            const float target_z = lwc1(p1 + 0x8);
            const float cur_z = lwc1(p0 + 0x18);
            half_g = fpu_mul(fpu_mul(half_g, frames), frames);
            v_term = fpu_sub(v_term, half_g);
            if (cur_z <= fpu_sub(target_z, v_term)) {
                moby_anim_set(p0, lbu(p1 + 0x2e), 0, 0xa);
                sb(p1 + 0x2f, 2);
            }
        }
        return static_cast<int32_t>(lbu(p1 + 0x2f));
    }
    case 2: {
        swc1(p0 + 0x18, fpu_add(lwc1(p0 + 0x18), lwc1(p1 + 0x10)));
        swc1(p1 + 0x10, fpu_sub(lwc1(p1 + 0x10), lwc1(p1 + 0x1c)));
        const float fl = floor_probe(0.5f, p0 + 0x10, 0);
        if (lwc1(p0 + 0x18) <= fl) {
            swc1(p0 + 0x18, fl);
            sb(p1 + 0x2f, 3);
        } else {
            const float dx = fpu_mul(cos_vu(lwc1(p1 + 0x18)), lwc1(p1 + 0x14));
            const float dy = fpu_mul(sin_vu(lwc1(p1 + 0x18)), lwc1(p1 + 0x14));
            const Vec4 delta{dx, dy, 0.0f, 0.0f};
            vec3_add(V(p0 + 0x10), V(p0 + 0x10), delta);
        }
        [[fallthrough]];
    }
    case 3:
        if ((lbu(p0 + 0x70) & 2u) != 0) {
            sb(p1 + 0x2f, 4);
        }
        return static_cast<int32_t>(lbu(p1 + 0x2f));
    default:
        return static_cast<int32_t>(lbu(p1 + 0x2f));
    }
}

// orig: 0x002bebc8
void doomlet_2bebc8(uint32_t p0) {
    sw(p0 + 0x40, lwu(P + 0x1ed0u));
    sw(p0 + 0x44, lwu(P + 0x1ed4u));
    sw(p0 + 0x48, lwu(P + 0x1ed8u));
    sw(p0 + 0x4c, lwu(P + 0x1f08u));
    sw(p0 + 0x5c, lwu(P + 0x1ee4u));
}

// orig: 0x002bec00
void doomlet_2bec00(uint32_t p0) {
    sw(P + 0x1f20u, lwu(p0 + 0x50));
    sw(P + 0x1f24u, lwu(p0 + 0x54));
    sw(P + 0x1f28u, lwu(p0 + 0x58));
    sb(P + 0x1f3fu, lbu(p0 + 0x68));
    sw(P + 0x1f40u, lwu(p0 + 0x44));
    lq_sq(P + 0x1f10u, p0);
}

// orig: 0x002bf8b0
void doomlet_2bf8b0(uint32_t p0, uint32_t p1, uint32_t p2) {
    sw(p1 + 0x30, 0);
    lq_sq(p1, p2);
    doomlet_2bf8b0_impl(p0, p1);
}

// orig: 0x002bec80
float doomlet_2bec80(uint32_t p0, uint32_t p1) {
    return doomlet_2bec80_v(V(p0), p1);
}

// orig: 0x002beab0
int32_t doomlet_2beab0(uint32_t p0, uint32_t p1, uint32_t p2) {
    return doomlet_2beab0_v(p0, p1, p2 != 0 ? &V(p2) : nullptr);
}

// orig: 0x002bec48
void doomlet_2bec48(uint32_t p0) {
    sw(p0 + 0x50, lwu(P + 0x1f20u));
    sw(p0 + 0x54, lwu(P + 0x1f24u));
    sw(p0 + 0x58, lwu(P + 0x1f28u));
    sb(p0 + 0x68, lbu(P + 0x1f3fu));
    sw(p0 + 0x44, lwu(P + 0x1f40u));
}

// orig: 0x002bf810
void doomlet_2bf810(uint32_t p0, uint32_t p1, uint32_t p2) {
    doomlet_2bf810_v(V(p0), V(p1), p2);
}

// orig: 0x002bf9f0
int32_t doomlet_2bf9f0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) {
    return doomlet_2bf9f0_v(p0, p1, V(p2), p3);
}

// orig: 0x002bed90
int32_t doomlet_2bed90(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    return doomlet_2bed90_v(p0, V(p1), p2, p3, p4);
}

// orig: 0x002bf050
float doomlet_2bf050(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, float pf12, float pf13, float pf14, float pf15, float pf16, float pf17) {
    float out = 0.0f;
    const float r = doomlet_2bf050_v(p0, V(p1), p2, out, pf12, pf13, pf14, pf15, pf16, pf17);
    swc1(p3, out);
    return r;
}

// orig: 0x00259028
int32_t doomlet_259028(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) {
    if (on_blarg()) return static_cast<int32_t>(blg_call(blg_doomlet_259028_u, p0, p1, p2, p3).v0);
    uint32_t flags = 0;
    vec_clear(V(p3));
    if (vec3_distance(V(p0 + 0x10), V(p2)) < lwc1(p1 + 0x2c)) {
        flags = 4;
    }

    const float yaw = atan2_f(fpu_sub(lwc1(p2 + 0x0), lwc1(p0 + 0x10)), fpu_sub(lwc1(p2 + 0x4), lwc1(p0 + 0x14)));
    yaw_turn_damped(p0, p1 + 0x14, yaw, lwc1(p1 + 0x3c), lwc1(p1 + 0x40), lwc1(p1 + 0x44));
    const float adist = angle_dist(yaw, lwc1(p0 + 0x48));
    if (lwc1(p1 + 0x28) <= adist) {
        flags |= 0x10u;
    }

    (void)timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(p1 + 0x48)));
    if (adist < lwc1(p1 + 0x28)) {
        if (lw(p1 + 0x48) < frame_scale_i(4)) {
            sw(p1 + 0x48, static_cast<uint32_t>(frame_scale_i(4)));
        }
    }

    if (lw(p1 + 0x48) != 0) {
        const float max_ang = lwc1(p1 + 0x28);
        const float scale = fpu_div(fpu_sub(max_ang, adist), max_ang);
        flags |= walker_record_step(scale, p0, p1, p2, p3);
        if (u2f(0x3dcccccdu) < adist) {
            flags &= ~2u;
        }
    }
    return static_cast<int32_t>(flags);
}

// orig: 0x002bffa8
void doomlet_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    if (lw(OV(0x0015f5c4u)) == 2) {
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        return;
    }

    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    if (!in_world_bounds(m)) {
        moby_retire(m);
        return;
    }

    if (sub_0025a420(m, 0x00800001u, 0) != 0) {
        sb(m + 0x20, 0xb);
    }

    const float grow = fpu_add(int_to_float(static_cast<int32_t>(lbu(DOOM_GOLD_ADDR))), 1.0f);
    const float cur_scale = lwc1(m + 0x2c);
    const float base_scale = lwc1(lwu(m + 0x24) + 0x24);
    const float dscale = fpu_mul(fpu_sub(fpu_mul(base_scale, grow), cur_scale), u2f(0x3d4ccccdu));
    swc1(m + 0x2c, fpu_add(cur_scale, dscale));

    const float grow_half = fpu_add(fpu_mul(int_to_float(static_cast<int32_t>(lbu(DOOM_GOLD_ADDR))), 0.5f), 1.0f);
    const uint32_t anim_id = lbu(m + 0x52);
    if (anim_id == 0 || anim_id == 6 || anim_id == 7) {
        swc1(m + 0x58, fpu_div(grow_half, grow));
    } else {
        swc1(m + 0x58, 1.0f);
    }

    if (lbu(m + 0x20) != 0xb) {
        const float k02 = u2f(0x3ca3d70au);
        const int32_t rad = float_to_int(fpu_mul(grow, u2f(0x43374bc7u))) + 1;
        const float up = fpu_add(fpu_mul(grow, u2f(0x3e4ac083u)), k02);
        sw(P + 0x1ec0u, static_cast<uint32_t>(rad));
        sw(P + 0x1ef0u, 0);
        swc1(P + 0x1ec4u, up);

        if (lbu(m + 0x20) == 0) {  // first frame: the walk and jump records' parameters, scaled by the frame time
            (void)libc_memset(P + 0x1f10u, 0, 0x40);
            const float dt = lwc1(0x0015ed60u);
            const float k_001 = u2f(0x3c23d70au);
            const float walk_turn_acc = fpu_mul(fpu_mul(grow, k_001), dt);
            const float quarter = fpu_mul(grow, 0.25f);
            const float jump_gravity = fpu_mul(lwc1(0x0015ed70u), u2f(0x41ed999au));
            const float run_turn_acc = fpu_mul(dt, u2f(0x3e4ccccdu));
            const float chase_speed = fpu_mul(fpu_mul(dt, u2f(0x3d872b02u)), grow_half);
            const float turn_vmax = fpu_mul(dt, u2f(0x3d3851ecu));
            const float turn_acc = fpu_mul(dt, k02);
            const float turn_damp = fpu_mul(dt, k_001);
            const float tenth = u2f(0x3dcccccdu);
            swc1(P + 0x1eccu, quarter);
            swc1(P + 0x1ec8u, quarter);
            swc1(pv + 0x44, run_turn_acc);
            swc1(pv + 0x40, walk_turn_acc);
            swc1(P + 0x1ee0u, u2f(0x3e99999au));
            swc1(P + 0x1ee4u, u2f(0x402ccccdu));
            swc1(P + 0x1edcu, walk_turn_acc);
            sw(P + 0x1ed8u, 0);
            swc1(pv + 0x5c, chase_speed);
            sb(P + 0x1f3eu, 4);
            sb(P + 0x1f3cu, 2);
            sb(P + 0x1f3du, 3);
            swc1(P + 0x1f4cu, turn_vmax);
            swc1(P + 0x1ee8u, u2f(0x40490fdbu));
            swc1(P + 0x1f2cu, jump_gravity);
            swc1(P + 0x1f30u, fpu_mul(grow, tenth));
            swc1(P + 0x1f34u, 7.5f);
            swc1(P + 0x1f38u, 11.5f);
            swc1(P + 0x1f44u, turn_acc);
            swc1(P + 0x1f48u, turn_damp);
            swc1(P + 0x1f00u, turn_damp);
            swc1(P + 0x1efcu, turn_acc);
            swc1(P + 0x1f04u, turn_vmax);
            sw(pv + 0x38, 0);
            sb(m + 0x20, 0xe);
            sw(pv + 0x30, 0);
            swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), tenth), grow_half));
            doomlet_2beb90(pv);
            doomlet_2bec00(pv);
            if (lbu(m + 0x53) != 5) {
                moby_anim_set(m, 5, 0, frame_scale_i(5));
            }
        }

        if (lbu(m + 0x20) == 0xd) {
            const uint32_t owner = lwu(pv + 0x60);
            if (owner == 0) {
                moby_retire(m);
                return;
            }
            const uint32_t ost = lbu(owner + 0x20);
            if (ost == 0xfeu || ost == 0xfdu || !in_world_bounds(m)) {
                moby_retire(m);
            }
            return;
        }

        sw(pv + 0x34, lwu(pv + 0x34) + 1u);
        if (frame_scale_i(0xe10) < lw(pv + 0x34)) {
            sb(m + 0x20, 0xb);
        }
        (void)timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x30)));
        V(pv + 0x20) = V(m + 0x10);

        if (doomlet_2beab0_v(m, 5, nullptr) != 0) {
            const uint32_t rec = OV(0x00173e40u);
            const uint32_t hit = lwu(rec + 0x18);
            bool do_pushout = false;
            if (hit == 0) {
                do_pushout = true;
            } else if (lw(rec + 0x1c) < 0) {
                if ((lhu(hit + 0x34) & 0x1000u) != 0 && lwu(hit + 0x24) != 0 && lh(lwu(hit + 0x24) + 0x46) == 5) {
                    sb(m + 0x20, 0xb);
                } else {
                    do_pushout = true;
                }
            }

            if (do_pushout) {
                const float old_z = lwc1(m + 0x18);
                V(m + 0x10) = V(OV(0x00173e70u));
                if (u2f(0x3f19999au) < abs_f(fpu_sub(old_z, old_z))) {
                    swc1(m + 0x18, fpu_add(old_z, u2f(0x3c23d70au)));
                }
                V(pv + 0x10) = V(OV(0x00173e70u) + 0x10u);
                const float k05 = u2f(0x3d4ccccdu);
                (void)vec3_clamp_length_vu(V(pv + 0x10), V(pv + 0x10), fpu_mul(lwc1(0x0015ed60u), k05));
                (void)vec2_clamp_length(V(pv + 0x10), V(pv + 0x10), fpu_mul(lwc1(0x0015ed60u), k05));
                sb(m + 0x20, 8);
                const uint32_t cur_tgt = lwu(pv + 0x38);
                if (cur_tgt != 0) {
                    const uint32_t t30 = lwu(pv + 0x30);
                    sw(pv + 0x3c, cur_tgt);
                    sw(pv + 0x30, t30 + 0x1f4u);
                }
                if (lbu(m + 0x53) != 5) {
                    moby_anim_set(m, 5, 0, frame_scale_i(5));
                }
            }
        }

        if (lwc1(m + 0x18) < u2f(0x3dcccccdu)) {
            moby_retire(m);
            return;
        }
        if (!(lw(pv + 0x30) < 4001)) {
            sb(m + 0x20, 0xb);
        }
    }

    if (floor_probe(0.5f, m + 0x10, 0) != 0.0f) {
        const uint32_t ground_moby = lwu(OV(0x00173e58u));
        if (ground_moby != 0) {
            (void)sub_00260400(m, ground_moby, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
        }
    }

    // the state machine (the original's jump table 0x1e9ba0, 16 states)
    bool check_fall = false;
    const uint32_t st = lbu(m + 0x20);
    switch (st) {
    case 0:  // (0x002c0664)
        debug_printf_stub();
        sb(m + 0x20, 1);
        break;

    case 8: {  // (0x002c067c)
        const uint32_t vel_addr = pv + 0x10;
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(lwc1(0x0015ed70u), 9.0f)));
        doomlet_2beb90(pv);
        const uint32_t moved = walker_move_fx(m, vel_addr, lw(P + 0x1ec0u), 0, fpu_mul(grow, u2f(0x3f333333u)), lwc1(P + 0x1ef0u));
        const uint32_t hit_floor = moved & 2u;
        if (hit_floor != 0) {
            sw(pv + 0x18, 0);
            vec3_scale(V(vel_addr), V(vel_addr), 0.5f);
        }
        (void)vec2_clamp_length(V(vel_addr), V(vel_addr), fpu_mul(lwc1(0x0015ed60u), u2f(0x3d4ccccdu)));
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (hit_floor != 0) {
                sb(m + 0x20, 2);
            } else if (lbu(m + 0x53) != 5) {
                moby_anim_set(m, 5, 0, frame_scale_i(5));
            }
        }
        const float yaw = atan2_f(lwc1(pv + 0x10), lwc1(pv + 0x14));
        yaw_turn_damped(m, pv + 0x40, yaw, lwc1(P + 0x1edcu), lwc1(P + 0x1ee0u), lwc1(P + 0x1ee4u));
        doomlet_2bebc8(pv);
        check_fall = true;
        break;
    }

    case 1: case 9: case 10:  // (0x002c0750)
        if ((lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 2);
        }
        check_fall = true;
        break;

    case 2: case 3: case 4: case 5: {  // (0x002c0768)
        float clearance = 0.0f;
        const uint32_t tgt0 = lwu(pv + 0x38);
        if (tgt0 != 0) {
            const uint32_t tst = lbu(tgt0 + 0x20);
            if (tst != 0xfeu && tst != 0xfdu) {
                float target_z = lwc1(tgt0 + 0x18);
                const uint32_t tpv0 = moby_pvar_word0(tgt0);
                float reach = grow_half;
                if (tpv0 != 0) {
                    const float add_r = fpu_mul(int_to_float(static_cast<int32_t>(lbu(tpv0 + 0xa))), 0.125f);
                    target_z = fpu_add(target_z, lwc1(tpv0 + 0x10));
                    reach = fpu_add(grow_half, add_r);
                }
                if (vec3_distance(V(m + 0x10), V(lwu(pv + 0x38) + 0x10)) < reach) {
                    if (sphere_collide_v(V(m + 0x10), grow_half, 1, m) != 0) {
                        sb(m + 0x20, 0xb);
                        break;
                    }
                }
                const float d2 = vec2_distance(V(m + 0x10), V(lwu(pv + 0x38) + 0x10));
                reach = fpu_mul(reach, 0.5f);
                if (d2 < reach) {
                    const float z_mid = fpu_add(lwc1(m + 0x18), fpu_mul(grow_half, u2f(0x3dcccccdu)));
                    if (abs_f(fpu_sub(target_z, z_mid)) < reach) {
                        doomlet_2bec00(pv);
                        doomlet_2bf8b0(m, DOOMLET_JUMP_REC, lwu(pv + 0x38) + 0x10u);
                        doomlet_2bec48(pv);
                        V(pv + 0x0) = V(lwu(pv + 0x38) + 0x10u);
                        swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3d872b02u)), grow_half));
                        if (lbu(m + 0x53) != 0) {
                            moby_anim_set(m, 0, 0, frame_scale_i(2));
                        }
                        sb(m + 0x20, 6);
                    }
                }
            }
        }

        Vec4 probe_pos = V(m + 0x10);
        probe_pos.z = fpu_add(probe_pos.z, fpu_mul(grow, u2f(0x3e99999au)));
        float dt_spd;
        if (rand_mod(4) != 0) {
            dt_spd = lwc1(0x0015ed60u);
        } else {
            sw(pv + 0x38, static_cast<uint32_t>(doomlet_2bed90_v(m, probe_pos, m + 0x40, lwu(pv + 0x3c), 1)));
            dt_spd = lwc1(0x0015ed60u);
        }
        swc1(pv + 0x5c, fpu_mul(fpu_mul(dt_spd, u2f(0x3dcccccdu)), grow_half));
        sb(m + 0x20, 4);

        if (lwu(pv + 0x38) == 0) {
            const float wander_yaw = rand_range_f(u2f(0xc0490fdbu), u2f(0x40490fdbu));
            if (rand_mod(4) == 0) {
                const float step_z = fpu_mul(grow, u2f(0x3eb33333u));
                const float r_near = fpu_mul(int_to_float(lw(DOOMLET_WALK_REC)), u2f(0x3a800000u));
                swc1(pv + 0x64, doomlet_2bf050_v(m, V(m + 0x10), 0, clearance, wander_yaw, u2f(0x3fec8b1fu), 4.0f, step_z, 4.0f, r_near));
            } else {
                clearance = 5.0f;
            }
        }

        uint32_t tgt = lwu(pv + 0x38);
        if (tgt != 0) {
            bool valid = (tgt == lwu(HERO_MOBY_PTR));
            if (!valid) {
                const uint32_t tst = lbu(tgt + 0x20);
                if (tst != 0xfeu && tst != 0xfdu) {
                    const uint32_t ci = lwu(tgt + 0x24);
                    if (ci != 0 && lh(ci + 0x46) == 5 && (lhu(tgt + 0x34) & 0x1000u) != 0) {
                        valid = true;
                    }
                }
            }
            if (!valid) {
                sw(pv + 0x38, 0);
                sb(m + 0x20, 2);
                swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3cdfa440u)), grow_half));
                break;
            }
        }

        const uint32_t prev_st = lbu(m + 0x20);
        if (tgt == 0) {
            const uint32_t cur_anim = lbu(m + 0x53);
            sb(m + 0x20, 5);
            if (cur_anim != 6) {
                moby_anim_set(m, 6, 0, frame_scale_i(0xa));
            }
            swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3cdfa440u)), grow_half));
        } else {
            if (rand_mod(4) == 0 && doomlet_2bf9f0(m, pv, lwu(pv + 0x38) + 0x10u, 1) != 0) {
                V(pv + 0x0) = V(lwu(pv + 0x38) + 0x10u);
                swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3d872b02u)), grow_half));
                if (lbu(m + 0x53) != 0) {
                    moby_anim_set(m, 0, 0, frame_scale_i(2));
                }
                sb(m + 0x20, 6);
                break;
            }

            const float dist2d = vec2_distance(V(m + 0x10), V(lwu(pv + 0x38) + 0x10u));
            if (lwu(pv + 0x38) == lwu(P + 0x2080u)) {
                const uint32_t m70 = lbu(m + 0x70);
                sb(m + 0x20, 5);
                if ((m70 & 2u) != 0 && lbu(m + 0x53) != 6) {
                    moby_anim_set(m, 6, 0, frame_scale_i(0));
                }
                swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3cdfa440u)), grow_half));
            }

            float k_spd;
            if (!(dist2d < 8.0f) && 20.0f < dist2d) {
                const uint32_t cur_anim = lbu(m + 0x53);
                sb(m + 0x20, 5);
                if (cur_anim != 0) {
                    moby_anim_set(m, 0, 0, frame_scale_i(0xa));
                }
                k_spd = u2f(0x3cdfa440u);
            } else {
                const uint32_t cur_anim = lbu(m + 0x53);
                sb(m + 0x20, 4);
                if (cur_anim != 7) {
                    moby_anim_set(m, 7, 0, frame_scale_i(0xa));
                }
                k_spd = u2f(0x3dcccccdu);
            }
            swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), k_spd), grow_half));

            if (rand_mod(4) == 0) {
                const uint32_t tgt_pos = lwu(pv + 0x38) + 0x10u;
                const float r_near = fpu_mul(int_to_float(lw(P + 0x1ec0u)), u2f(0x3a800000u));
                swc1(pv + 0x64, doomlet_2bf050_v(m, V(m + 0x10), tgt_pos, clearance, lwc1(m + 0x48), u2f(0x3fec8b1fu), 4.0f, u2f(0x3eb33333u), 4.0f, r_near));
            } else {
                clearance = 5.0f;
            }
        }

        if (lbu(m + 0x20) == 4 && prev_st == 5) {
            const uint32_t cur_anim = lbu(m + 0x53);
            sb(m + 0x20, 0xc);
            if (cur_anim != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(0xa));
            }
            check_fall = true;
            break;
        }

        const float ang = lwc1(pv + 0x64);
        swc1(pv + 0x4, fpu_add(lwc1(m + 0x14), fpu_mul(sin_vu(ang), 5.0f)));
        swc1(pv + 0x0, fpu_add(lwc1(m + 0x10), fpu_mul(cos_vu(lwc1(pv + 0x64)), 5.0f)));
        swc1(pv + 0x8, fpu_add(lwc1(m + 0x18), 1.0f));
        doomlet_2beb90(pv);
        yaw_turn_damped(m, pv + 0x44, lwc1(pv + 0x64), lwc1(P + 0x1efcu), lwc1(P + 0x1f00u), lwc1(P + 0x1f04u));

        Vec4 ahead{};
        vec3_scale(ahead, V(pv + 0x10), 5.0f);
        ahead.z = 0.0f;
        Vec4 feet = V(m + 0x10);
        feet.z = fpu_add(feet.z, fpu_mul(grow, u2f(0x3e19999au)));
        Vec4 feet_ahead{};
        vec3_add(feet_ahead, feet, ahead);

        bool can_walk = true;
        if (segment_collide_v(feet, feet_ahead, 6, m, nullptr) != 0) {
            const uint32_t hit = lwu(OV(0x00173e58u));
            if (hit != lwu(pv + 0x38)) {
                if (hit == 0 || lwu(hit + 0x24) == 0 || lh(lwu(hit + 0x24) + 0x46) != 5) {
                    clearance = 0.0f;
                    can_walk = false;
                }
            }
        }

        uint32_t walk_flags;
        if (can_walk) {
            const float saved_z = lwc1(m + 0x18);
            walk_flags = static_cast<uint32_t>(doomlet_259028(m, DOOMLET_WALK_REC, pv, pv + 0x10));
            if (1.0f < abs_f(fpu_sub(lwc1(m + 0x18), saved_z))) {
                swc1(m + 0x18, saved_z);
                if (doomlet_2beab0_v(m, 5, nullptr) != 0) {
                    V(m + 0x10) = V(OV(0x00173e70u));
                    if (1.0f < abs_f(fpu_sub(lwc1(m + 0x18), saved_z))) {
                        swc1(m + 0x18, saved_z);
                    }
                }
            }
        } else {
            walk_flags = 2;
            swc1(pv + 0x10, fpu_sub(lwc1(m + 0x10), lwc1(pv + 0x0)));
            swc1(pv + 0x18, u2f(0x3c23d70au));
            swc1(pv + 0x14, fpu_sub(lwc1(m + 0x14), lwc1(pv + 0x4)));
            vec3_set_length(V(pv + 0x10), V(pv + 0x10), u2f(0x3dcccccdu));
        }

        doomlet_2bebc8(pv);
        if ((walk_flags & 2u) != 0 || clearance < 4.0f) {
            if (doomlet_2bf9f0(m, pv, pv, 0) != 0) {
                swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3d872b02u)), grow_half));
                if (lbu(m + 0x53) != 0) {
                    moby_anim_set(m, 0, 0, frame_scale_i(2));
                }
                sb(m + 0x20, 6);
                check_fall = true;
            } else {
                const uint32_t cur_tgt = lwu(pv + 0x38);
                if (cur_tgt != 0) {
                    const uint32_t t30 = lwu(pv + 0x30);
                    sw(pv + 0x3c, cur_tgt);
                    sw(pv + 0x30, t30 + 0x1f4u);
                }
                swc1(pv + 0x10, fpu_sub(lwc1(m + 0x10), lwc1(pv + 0x0)));
                swc1(pv + 0x18, u2f(0x3c23d70au));
                swc1(pv + 0x14, fpu_sub(lwc1(m + 0x14), lwc1(pv + 0x4)));
                vec3_set_length(V(pv + 0x10), V(pv + 0x10), u2f(0x3dcccccdu));
                const uint32_t cur_anim = lbu(m + 0x53);
                sb(m + 0x20, 8);
                if (cur_anim != 5) {
                    moby_anim_set(m, 5, 0, frame_scale_i(5));
                }
            }
        } else {
            check_fall = true;
        }
        break;
    }

    case 6: {  // (0x002c10f4)
        const float yaw_to_goal = atan2_f(fpu_sub(lwc1(pv + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x4), lwc1(m + 0x14)));
        doomlet_2beb90(pv);
        const float acc = fpu_add(lwc1(P + 0x1efcu), lwc1(P + 0x1efcu));
        const float damp = fpu_mul(lwc1(P + 0x1f00u), 0.5f);
        const float vmax = fpu_add(lwc1(P + 0x1f04u), lwc1(P + 0x1f04u));
        yaw_turn_damped(m, 0x00141224u, yaw_to_goal, acc, damp, vmax);
        doomlet_2bebc8(pv);
        if (angle_dist(lwc1(m + 0x48), yaw_to_goal) < u2f(0x3db2b8c2u)) {
            const uint32_t anim = lbu(P + 0x1f3cu);
            moby_anim_set(m, anim, 0, frame_scale_i(3));
            sb(m + 0x20, 7);
        }
        check_fall = true;
        break;
    }

    case 7: {  // (0x002c11b4)
        const float old_z = lwc1(m + 0x18);
        doomlet_2bec00(pv);
        const int32_t jst = doomlet_25b440(m, DOOMLET_JUMP_REC);
        if (jst == 4) {
            const uint32_t cur_anim = lbu(m + 0x53);
            sb(m + 0x20, 4);
            if (cur_anim != 7) {
                moby_anim_set(m, 7, 0, frame_scale_i(3));
            }
            swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3dcccccdu)), grow_half));
        }
        doomlet_2bec48(pv);
        (void)abs_f(fpu_sub(lwc1(m + 0x18), old_z));
        check_fall = true;
        break;
    }

    case 11: {  // (0x002c1248)
        if (!in_world_bounds(m)) {
            moby_retire(m);
            return;
        }
        const uint32_t gold = lbu(DOOM_GOLD_ADDR);
        explosion_effect(m, 0, m + 0x10, 5, 2, 4, -1, 1, 5, -1, gold, 0.0f, 0.0f, fpu_add(grow_half, grow_half), grow_half, 9.0f, 1.0f, 15.0f);
        const int32_t count = mobys_in_sphere(m + 0x10, grow_half, 0x15, m, 0);
        const Vec4 hit_pos = V(m + 0x10);
        moby_list_hit_v(m, hit_pos, OV(0x00177f00u), count, 0, 0x10000u, 2, 3, 3.0f, 1.0f, 1.0f);
        if (lbu(DOOM_GOLD_ADDR) != 0) {
            (void)moby_sound_start(4, 0, m);
        } else {
            (void)moby_sound_start(0, 0, m);
        }
        moby_retire(m);
        return;
    }

    case 12:  // (0x002c1400)
        if ((lbu(m + 0x70) & 2u) != 0) {
            const uint32_t cur_anim = lbu(m + 0x53);
            sb(m + 0x20, 4);
            if (cur_anim != 7) {
                moby_anim_set(m, 7, 0, frame_scale_i(0xa));
            }
            swc1(pv + 0x5c, fpu_mul(fpu_mul(lwc1(0x0015ed60u), u2f(0x3dcccccdu)), grow_half));
        }
        break;

    case 13:  // (0x002c1464)
        debug_printf_stub();
        break;

    case 14: {  // (0x002c1478)
        const uint32_t vel_addr = pv + 0x10;
        Vec4 push{};
        vec3_add(push, V(m + 0x10), V(vel_addr));
        if (sphere_collide_v(V(m + 0x10), fpu_mul(grow, 0.5f), 1, m) != 0) {
            const uint32_t rec = OV(0x00173e40u);
            const uint32_t hit = lwu(rec + 0x18);
            if (hit != 0 && lh(hit + 0xa6) == 0xba) {
                vec3_sub(push, V(rec + 0x30), V(m + 0x10));
                const float len = vec3_length(push);
                push.z = 0.0f;
                vec3_set_length(push, push, fpu_mul(len, u2f(0x3f8ccccdu)));
                vec3_add(V(m + 0x10), V(m + 0x10), push);
            }
        }
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(lwc1(0x0015ed70u), 9.0f)));
        doomlet_2beb90(pv);
        const uint32_t moved = walker_move_fx(m, vel_addr, lw(P + 0x1ec0u), 0, fpu_mul(grow, u2f(0x3f333333u)), lwc1(P + 0x1ef0u));
        const uint32_t hit_floor = moved & 2u;
        if (hit_floor != 0) {
            sw(pv + 0x18, 0);
            vec3_scale(V(vel_addr), V(vel_addr), 0.5f);
        }
        (void)vec3_clamp_length_vu(V(vel_addr), V(vel_addr), fpu_mul(lwc1(0x0015ed60u), u2f(0x3d4ccccdu)));
        if (hit_floor != 0) {
            sb(m + 0x20, 2);
        } else if (lbu(m + 0x53) != 5) {
            moby_anim_set(m, 5, 0, frame_scale_i(5));
        }
        const float yaw = atan2_f(lwc1(pv + 0x10), lwc1(pv + 0x14));
        yaw_turn_damped(m, pv + 0x40, yaw, lwc1(P + 0x1edcu), lwc1(P + 0x1ee0u), lwc1(P + 0x1ee4u));
        doomlet_2bebc8(pv);
        check_fall = true;
        break;
    }

    default:  // 15 (0x002c1644), and the states past the table
        sb(m + 0x20, 0);
        break;
    }

    if (check_fall) {
        sb(m + 0x20, static_cast<uint32_t>(doomlet_2bfec8(m)));
    }

    if (!in_world_bounds(m)) {
        moby_retire(m);
    }
}

// orig: (none) -- the ports for the engine's indirect calls (a moby class's update, a draw callback)
void doomlet_fns_register() {
    ovl_register_moby_fn(0x002bffa8, doomlet_update);
}

}  // namespace rac1
