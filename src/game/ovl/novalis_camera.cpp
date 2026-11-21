// Camera type 3, the path camera for slides and rails: Novalis's (level 1) and the same code on Rilgar, Batalia, Kalebo III
// and Drek's fleet (levels 5, 8, 16, 17; their camera types table's row 3, its accept a bare `return 0`). What it reads is
// the engine's (the camera records 0x15ef50, the paths 0x1b04b0, the rails 0x15f70c, the previous camera 0x166e04, the up
// axis 0x166e30: OV) and the base ELF's (the hero, his rail record 0x13f8b0..): one port serves every level's copy.
#include <game/ovl/novalis_camera.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {

constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t RAIL_PATH = 0x0013f8b0u;
constexpr uint32_t RAIL_SEG = 0x0013f8b4u;
constexpr uint32_t RAIL_OFF = 0x0013f8b8u;
constexpr uint32_t RAIL_REV = 0x0013f8c0u;
constexpr uint32_t HERO_CAM_REQ = 0x001415d4u;

uint32_t entry_of(uint32_t cam) {
    return lwu(OV(0x0015ef50u)) + static_cast<uint32_t>(lh(cam + 0x84) * 0x20);
}

uint32_t path_at(int32_t idx) {
    return lwu(OV(0x001b04b0u) + static_cast<uint32_t>(idx) * 4u);
}

void approach_step(uint32_t addr, float target, float step) {
    const float cur = lwc1(addr);
    if (abs_f(fpu_sub(cur, target)) < step) {
        swc1(addr, target);
    } else {
        swc1(addr, cur < target ? fpu_add(cur, step) : fpu_sub(cur, step));
    }
}

void map_waypoints_to_path(uint32_t wp, uint32_t target, int32_t& seg_idx, float& dist) {
    int32_t n = lw(wp);
    if (n > 0) {
        for (int32_t i = 0; i < n; ++i) {
            const uint32_t meta = wp + static_cast<uint32_t>(i) * 0x10u;
            const uint32_t pt = meta + 0x10u;
            Vec4 closest{};
            path_closest_point_v(target, V(pt), closest, &seg_idx, &dist, 0, 20.0f, 5.0f, 0.0f);
            swc1(meta + 0x10u, lwc1(meta + 0x1cu));
            swc1(meta + 0x1cu, static_cast<float>(seg_idx));
            n = lw(wp);
        }
    }
    if (n > 0) {
        int32_t prev_i = n - 1;
        for (int32_t i = 0; i < n; ++i) {
            const uint32_t prev_meta = wp + static_cast<uint32_t>(prev_i) * 0x10u;
            const uint32_t cur_meta = wp + static_cast<uint32_t>(i) * 0x10u;
            const int32_t start_seg = float_to_int(lwc1(prev_meta + 0x1cu));
            const int32_t end_seg = float_to_int(lwc1(cur_meta + 0x1cu));
            int32_t count = end_seg - start_seg;
            if (count < 0) count += lw(target);
            float sum = 0.0f;
            if (count > 0) {
                // Preserves original L1:0x315184 loop indexing: uses (start_seg + i) % target_count
                const int32_t seg_idx_sum = start_seg + i;
                for (int32_t k = 0; k < count; ++k) {
                    const int32_t rem = ee_rem(seg_idx_sum, lw(target));
                    sum = fpu_add(sum, lwc1(target + static_cast<uint32_t>(rem) * 0x10u + 0x1cu));
                }
            }
            swc1(cur_meta + 0x14u, sum);
            dist = sum;
            prev_i = i;
            n = lw(wp);
        }
    }
}

}  // namespace

// The camera's work block (+0x70) reset to its start values; for a hand-back kind 2 (data +0x30) seen the first time
// (data +0x34) the two waypoint lists (paths data +0x28 / +0x2c) are mapped onto the camera path (data +0x20) and the rail
// (the rail table's entry data +0x24): each waypoint's segment and the lengths between them.
// orig: L1:0x00314e98 L5:0x00325340 L8:0x00311890 L16:0x002efe48 L17:0x002fc2c8
void cam_type3_reset(uint32_t cam) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    const uint32_t w1 = lwu(cam + 0x70u) + 0x80u;
    swc1(w1 + 0x4cu, lw(data + 0x30u) == 3 ? 0.5f : 1.5f);
    sw(w1 + 0x40u, 0);
    swc1(w1 + 0x48u, u2f(0x3e567750u));

    const uint32_t w2 = lwu(cam + 0x70u);
    swc1(w2 + 0xe0u, 6.0f);
    swc1(w2 + 0xe4u, 0.5f);
    swc1(w2 + 0xf0u, 2.0f);

    const float k01 = u2f(0x3c23d70au);
    const uint32_t w3 = lwu(cam + 0x70u);
    swc1(w3 + 0x20u, u2f(0x3a03126fu));
    swc1(w3 + 0x14u, u2f(0x3e333333u));
    swc1(w3 + 0x10u, k01);
    sw(w3 + 0x18u, 0);
    sh(w3 + 0x1cu, 0);
    sw(w3 + 0x24u, 0);
    vec_clear(V(w3));

    const float d2 = u2f(0x3e4ccccdu);
    const uint32_t w4 = lwu(cam + 0x70u) + 0x30u;
    swc1(w4 + 0x24u, u2f(0x3e99999au));
    swc1(w4 + 0x20u, k01);
    swc1(w4 + 0x28u, d2);
    sh(w4 + 0x2cu, 0);
    vec_clear(V(w4));

    const uint32_t w5 = lwu(cam + 0x70u);
    const uint32_t s0 = w5 + 0x100u;
    vec_clear(V(s0));
    vec_clear(V(w5 + 0x120u));
    swc1(s0 + 0x40u, 3.0f);
    swc1(s0 + 0x30u, u2f(0x3b449ba6u));
    swc1(s0 + 0x34u, u2f(0x3dcccccdu));
    swc1(s0 + 0x38u, d2);
    sw(s0 + 0x3cu, 0);

    const uint32_t w6 = lwu(cam + 0x70u) + 0x150u;
    sw(w6, 0);
    sw(w6 + 0x10u, 1u);
    sw(w6 + 0x4u, 0);
    sw(w6 + 0x8u, 0);
    sw(w6 + 0xcu, 0);

    if (lw(data + 0x30u) == 2 && lh(data + 0x34u) == 0) {
        const uint32_t wp0 = path_at(lw(data + 0x28u));
        const uint32_t cam_path = path_at(lw(data + 0x20u));
        const uint32_t rail_entry = lwu(OV(0x0015f70cu)) + static_cast<uint32_t>(lw(data + 0x24u)) * 0x20u;
        const uint32_t rail_path = lwu(rail_entry + 0x10u);
        const uint32_t wp1 = path_at(lw(data + 0x2cu));
        sh(data + 0x34u, 1u);
        int32_t seg = 0;
        float dist = 0.0f;
        map_waypoints_to_path(wp0, cam_path, seg, dist);
        map_waypoints_to_path(wp1, rail_path, seg, dist);
    }

    sh(cam + 0x7eu, 0);
}

// The camera started: the previous camera's matrix and position taken over, the camera path's segment lengths measured,
// the direction along the path chosen (data +0x36: the camera ahead of the hero) for kind 3, then the reset.
// orig: L1:0x00315de0 L5:0x00326288 L8:0x003127d8 L16:0x002f0d90 L17:0x002fd210
void cam_type3_init(uint32_t cam) {
    const uint32_t prev = lwu(OV(0x00166e04u));
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    lq_sq(cam + 0x30u, prev + 0x30u);
    lq_sq(cam + 0x00u, prev + 0x00u);
    lq_sq(cam + 0x10u, prev + 0x10u);
    lq_sq(cam + 0x20u, prev + 0x20u);
    lq_sq(cam + 0x40u, cam + 0x00u);

    const int32_t path_idx = lw(data + 0x20u);
    if (path_idx >= 0) {
        const uint32_t path = path_at(path_idx);
        int32_t n = lw(path);
        for (int32_t i = 0; i < n; ++i) {
            const int32_t next = ee_rem(i + 1, n);
            const float d = vec3_distance(V(path + 0x10u + static_cast<uint32_t>(i) * 0x10u),
                                          V(path + 0x10u + static_cast<uint32_t>(next) * 0x10u));
            swc1(path + static_cast<uint32_t>(i) * 0x10u + 0x1cu, d);
            n = lw(path);
        }
    }

    const int32_t mode = lw(data + 0x30u);
    sh(data + 0x36u, 0);
    if (mode == 3) {
        const uint32_t path = path_at(lw(data + 0x20u));
        int32_t seg = 0;
        float dist = 0.0f;
        Vec4 closest{};
        if (path_closest_point_v(path, V(HERO_POS), closest, &seg, &dist, 0, 20.0f, 1.0f, 0.0f) != 0) {
            const int32_t n = lw(path);
            const int32_t next = (seg + 1 < n) ? (seg + 1) : (n - 1);
            Vec4 tangent{};
            vec3_sub(tangent,
                     V(path + 0x10u + static_cast<uint32_t>(next) * 0x10u),
                     V(path + 0x10u + static_cast<uint32_t>(seg) * 0x10u));
            if (vec3_length(tangent) != 0.0f && next != seg) {
                vec3_set_length(tangent, tangent, 1.0f);
                Vec4 cam_off{};
                vec3_sub(cam_off, V(cam + 0x30u), V(HERO_POS));
                if (0.0f < vec3_dot(cam_off, tangent)) {
                    sh(data + 0x36u, 1u);
                }
            }
        }
    }

    cam_type3_reset(cam);
}

// The camera's frame: its position along the camera path (by kind data +0x30: 0 the rail's segment, 2 the waypoints'
// share of the rail, else the point nearest the hero or the rail point ahead), sprung toward; then turned toward a point
// ahead of the hero on the path or the rail.
// orig: L1:0x00315358 L5:0x00325800 L8:0x00311d50 L16:0x002f0308 L17:0x002fc788
void cam_type3_update(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70u);
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    const int32_t mode = lw(data + 0x30u);
    const int32_t path_idx = lw(data + 0x20u);
    float k_aim = u2f(0x3c23d70au);
    float look_ahead = lwc1(data + 0x00u);
    const float damp = u2f(0x3e333333u);

    int32_t sp78 = 0;
    float sp7c = 0.0f;
    // The point the camera looks past the hero from: one vector of the original's frame (sp+0x40), which holds the segment
    // offset (kind 0) or the target (kind 2) before the look writes it. The look's nearest point on the camera path leaves it
    // as it is when the hero is beyond the search's reach (more than 21 from the path): the camera then looks from there
    // (from 0, a slot the frame never wrote, for the other kinds).
    Vec4 look_base{};

    if (mode == 0) {
        const uint32_t cam_path_slot = OV(0x001b04b0u) + static_cast<uint32_t>(path_idx) * 4u;
        const int32_t rail_seg = lw(RAIL_SEG);
        const float rail_off = lwc1(RAIL_OFF);
        sp78 = rail_seg;
        sp7c = rail_off;
        sw(w + 0x150u, static_cast<uint32_t>(rail_seg));
        swc1(w + 0x154u, rail_off);

        int32_t adv_seg = rail_seg;
        float adv_off = rail_off;
        Vec4 adv_out{};
        path_advance_v(lwu(cam_path_slot), adv_out, &adv_seg, &adv_off, 0, 1.0f);
        sw(w + 0x150u, static_cast<uint32_t>(adv_seg));
        swc1(w + 0x154u, adv_off);
        V(w + 0x80u) = adv_out;

        const uint32_t rail_path = lwu(RAIL_PATH);
        const uint32_t cam_path = lwu(cam_path_slot);
        const float frac = fpu_div(sp7c, lwc1(rail_path + static_cast<uint32_t>(sp78) * 0x10u + 0x1cu));
        const float cam_len = fpu_mul(lwc1(cam_path + static_cast<uint32_t>(sp78) * 0x10u + 0x1cu), frac);

        Vec4 seg_dir{}, seg_off{}, target{};
        vec3_sub(seg_dir,
                 V(cam_path + 0x20u + static_cast<uint32_t>(sp78) * 0x10u),
                 V(cam_path + 0x10u + static_cast<uint32_t>(sp78) * 0x10u));
        vec3_set_length(seg_off, seg_dir, cam_len);
        vec3_add(target, V(cam_path + 0x10u + static_cast<uint32_t>(sp78) * 0x10u), seg_off);
        look_base = seg_off;

        swc1(cam + 0x30u, camera_spring(lwc1(cam + 0x30u), target.x, k_aim, damp, 0.0f, w + 0x0u));
        swc1(cam + 0x34u, camera_spring(lwc1(cam + 0x34u), target.y, k_aim, damp, 0.0f, w + 0x4u));
        swc1(cam + 0x38u, camera_spring(lwc1(cam + 0x38u), target.z, k_aim, damp, 0.0f, w + 0x8u));
    } else if (mode == 2) {
        const uint32_t cam_path = path_at(path_idx);
        const uint32_t wp1 = path_at(lw(data + 0x2cu));
        const uint32_t wp0 = path_at(lw(data + 0x28u));
        const uint32_t rail_path = lwu(lwu(OV(0x0015f70cu)) + static_cast<uint32_t>(lw(data + 0x24u)) * 0x20u + 0x10u);

        int32_t span = 0;
        int32_t prev = lw(wp1) - 1;
        while (span < lw(wp1)) {
            const int32_t start_seg = float_to_int(lwc1(wp1 + static_cast<uint32_t>(prev) * 0x10u + 0x1cu));
            const int32_t end_seg = float_to_int(lwc1(wp1 + static_cast<uint32_t>(span) * 0x10u + 0x1cu));
            const int32_t cur_seg = lw(RAIL_SEG);
            if (!(end_seg < start_seg)) {
                if (!(cur_seg < start_seg) && cur_seg < end_seg) break;
            } else {
                if (!(cur_seg < start_seg) || cur_seg < end_seg) break;
            }
            prev = span;
            ++span;
        }

        sw(w + 0x160u, static_cast<uint32_t>(span));
        const float rail_span_len = lwc1(wp1 + static_cast<uint32_t>(span) * 0x10u + 0x14u);
        const int32_t n_wp = lw(wp1);
        const int32_t prev_wp = ee_rem(span + n_wp - 1, n_wp);
        const float wp_look = lwc1(wp1 + static_cast<uint32_t>(prev_wp) * 0x10u + 0x10u);
        const float cam_span_len = lwc1(wp0 + static_cast<uint32_t>(span) * 0x10u + 0x14u);
        if (wp_look != 0.0f) look_ahead = wp_look;

        const int32_t start_seg = float_to_int(lwc1(wp1 + static_cast<uint32_t>(prev_wp) * 0x10u + 0x1cu));
        int32_t seg_diff = lw(RAIL_SEG) - start_seg;
        if (seg_diff < 0) seg_diff += lw(rail_path);
        float dist_along = 0.0f;
        for (int32_t k = 0; k < seg_diff; ++k) {
            const int32_t r = ee_rem(start_seg + k, lw(rail_path));
            dist_along = fpu_add(dist_along, lwc1(rail_path + static_cast<uint32_t>(r) * 0x10u + 0x1cu));
        }
        dist_along = fpu_add(dist_along, lwc1(RAIL_OFF));

        const float cam_step = fpu_mul(cam_span_len, fpu_div(dist_along, rail_span_len));
        int32_t cam_seg = float_to_int(lwc1(wp0 + static_cast<uint32_t>(prev_wp) * 0x10u + 0x1cu));
        float cam_off = 0.0f;
        Vec4 target{};
        path_advance_v(cam_path, target, &cam_seg, &cam_off, 1, cam_step);
        look_base = target;

        const float k_mult = lwc1(wp0 + static_cast<uint32_t>(prev_wp) * 0x10u + 0x10u);
        float k_pos = u2f(0x3c23d70au);
        if (!(k_mult <= 0.0f)) {
            k_aim = fpu_mul(k_aim, k_mult);
            k_pos = fpu_mul(k_mult, u2f(0x3c23d70au));
        }
        approach_step(w + 0x10u, k_pos, u2f(0x391d4952u));

        swc1(cam + 0x30u, camera_spring(lwc1(cam + 0x30u), target.x, lwc1(w + 0x10u), damp, 0.0f, w + 0x0u));
        swc1(cam + 0x34u, camera_spring(lwc1(cam + 0x34u), target.y, lwc1(w + 0x10u), damp, 0.0f, w + 0x4u));
        swc1(cam + 0x38u, camera_spring(lwc1(cam + 0x38u), target.z, lwc1(w + 0x10u), damp, 0.0f, w + 0x8u));
    } else {
        const uint32_t cam_path = path_at(path_idx);
        Vec4 target{};
        if (lw(data + 0x24u) < 0) {
            path_closest_point_v(cam_path, V(HERO_POS), target, &sp78, &sp7c, 0, 20.0f, 1.0f, 0.0f);
            const float step = (lh(data + 0x36u) == 1) ? -fpu_mul(lwc1(data + 0x38u), 4.0f) : lwc1(data + 0x38u);
            path_advance_v(cam_path, target, &sp78, &sp7c, 1, step);
        } else {
            int32_t r_seg = lw(RAIL_SEG);
            float r_off = lwc1(RAIL_OFF);
            Vec4 rail_pt{};
            path_advance_v(lwu(RAIL_PATH), rail_pt, &r_seg, &r_off, 1, lwc1(data + 0x38u));
            path_closest_point_v(cam_path, rail_pt, target, &sp78, &sp7c, 0, 20.0f, 1.0f, 0.0f);
        }

        const float k_base = u2f(0x3c23d70au);
        swc1(cam + 0x30u, camera_spring(lwc1(cam + 0x30u), target.x, k_base, damp, 0.0f, w + 0x0u));
        swc1(cam + 0x34u, camera_spring(lwc1(cam + 0x34u), target.y, k_base, damp, 0.0f, w + 0x4u));
        swc1(cam + 0x38u, camera_spring(lwc1(cam + 0x38u), target.z, k_base, damp, 0.0f, w + 0x8u));
    }

    if (lw(data + 0x24u) < 0) {
        const uint32_t cam_path = path_at(lw(data + 0x20u));
        path_closest_point_v(cam_path, V(HERO_POS), look_base, &sp78, &sp7c, 0, 20.0f, 1.0f, 0.0f);
        const int32_t loop = (lw(0x0015ed84u) == 0xe && lw(data + 0x30u) == 0) ? 0 : 1;
        Vec4 ahead{};
        path_advance_v(cam_path, ahead, &sp78, &sp7c, loop, look_ahead);
        vec3_sub(ahead, ahead, look_base);
        vec3_add(look_base, V(HERO_POS), ahead);
    } else {
        int32_t r_seg = lw(RAIL_SEG);
        float r_off = lwc1(RAIL_OFF);
        path_advance_v(lwu(RAIL_PATH), look_base, &r_seg, &r_off, 1, look_ahead);
    }

    Vec4 up_off{}, aim_pt{}, dir{};
    vec3_set_length(up_off, V(OV(0x00166e30u)), lwc1(w + 0x80u + 0x4cu));
    vec3_add(aim_pt, up_off, look_base);
    vec3_sub(dir, aim_pt, V(cam + 0x30u));

    const float len = vec3_length(dir);
    float off_ang = 0.0f;
    if (len != 0.0f) {
        vec3_scale(dir, dir, fpu_div(1.0f, len));
        off_ang = fpu_sub(u2f(0x3fc90fdbu), asin_f(vec3_dot(dir, V(cam))));
    }

    approach_step(w + 0x50u, k_aim, u2f(0x391d4952u));
    const float turn = camera_angle_spring(0.0f, off_ang, lwc1(w + 0x50u), damp, 0.0f, w + 0x64u);

    Vec4 axis{};
    vec3_cross_ba(axis, dir, V(cam + 0x40u));
    vec_rotate_axis(V(cam), V(cam + 0x40u), axis, turn);
    vec3_set_length(V(cam + 0x40u), V(cam), 1.0f);
    vec3_set_length(V(cam), V(cam), 1.0f);
    vec3_cross_ba(V(cam + 0x10u), V(cam), V(OV(0x00166e30u)));
    vec3_set_length(V(cam + 0x10u), V(cam + 0x10u), 1.0f);
    vec3_cross_ba(V(cam + 0x20u), V(cam + 0x10u), V(cam));
}

// The camera handed back (+0x7e: 5 for kind 3, else 3) unless the hero still asks for it and rides its rail forward.
// orig: L1:0x00316030 L5:0x003264d8 L8:0x00312a28 L16:0x002f0fe0 L17:0x002fd460
void cam_type3_exit(uint32_t cam) {
    const uint32_t data = lwu(entry_of(cam) + 0x1cu);
    if (lw(HERO_CAM_REQ) == lh(cam + 0x86u)) {
        const int32_t k = lw(data + 0x24u);
        if (k < 0) return;
        const uint32_t rail = lwu(lwu(OV(0x0015f70cu)) + static_cast<uint32_t>(k) * 0x20u + 0x10u);
        if (lwu(RAIL_PATH) == rail && lw(RAIL_REV) == 0) return;
    }
    sh(cam + 0x7eu, lw(data + 0x30u) == 3 ? 5u : 3u);
}

}  // namespace rac1
