// Ported overlay code: Suck Cannon (item 0x09) hand moby state machine, vortex beam simulation/draw,
// carried-moby suction helpers, and ammo bar HUD callbacks.
#include <game/ovl/suck_cannon.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350u;
constexpr uint32_t GOLD_SUCK_CANNON = 0x0013e529u;
constexpr uint32_t SUCK_SLOTS = 0x001413a0u;  // HERO + 0x2050
constexpr uint32_t PAD_BTN = 0x0013cae0u;

// Build the 3-row basis for the vortex beam (camera frame in first-person, hand/hero frame otherwise).
void build_aim_frame(uint32_t hand, Mat4& frame) {
    const uint32_t state = lwu(HERO + 0x2084);
    if ((state == 0x1eu || state == 1u) && (lhu(lwu(HERO + 0x2080) + 0x34) & 1u) != 0u) {
        const uint32_t cam = OV(0x00166fd0u);
        frame.r[0] = V(cam);
        frame.r[1] = V(cam + 0x10);
        frame.r[2] = V(cam + 0x20);
        return;
    }
    if (lh(HERO + 0x22c8) == 1) {
        vec3_scale(frame.r[0], V(hand + 0xd0), -1.0f);
        frame.r[1] = V(hand + 0xc0);
        frame.r[2] = V(hand + 0xe0);
        return;
    }
    frame.r[0] = V(HERO + 0x640);
    frame.r[1] = V(HERO + 0x650);
    frame.r[2] = V(HERO + 0x660);
    if (advanced_aiming()) {
        const uint32_t rows = ee_stack_alloc(0x30);
        V(rows) = frame.r[0];
        V(rows + 0x10) = frame.r[1];
        V(rows + 0x20) = frame.r[2];
        advanced_aim_frame_hand(hand, rows);  // enhancement: modern aim frame (advanced.h)
        frame.r[0] = V(rows);
        frame.r[1] = V(rows + 0x10);
        frame.r[2] = V(rows + 0x20);
        ee_stack_free(0x30);
    }
}

void stop_suck_sound(uint32_t m, uint32_t pvar) {
    const int32_t snd = lw(pvar + 0x4);
    if (snd != -1) {
        const uint32_t chan = 0x0013e550u + static_cast<uint32_t>(snd) * 0x70u;
        if (lwu(chan + 0x88) == m && lbu(chan + 0x74) != 0u) {
            sub_0028d918(snd);
        }
    }
    sw(pvar + 0x4, 0xffffffffu);
}

void start_suck_rumble(uint32_t m, uint8_t next_state) {
    constexpr uint32_t RUMBLE = 0x00141680u;
    const uint16_t cnt = lhu(RUMBLE + 0x48);
    if (cnt <= 0xfffeu) {
        sh(RUMBLE + 0x48, static_cast<int16_t>(cnt + 1u));
    }
    const int32_t scaled = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
    if (static_cast<int32_t>(lhu(RUMBLE + 0x4a)) < scaled) {
        const int32_t scaled2 = ee_div(frame_scale_i(lw(0x0015eea4u)), 0x258);
        sh(RUMBLE + 0x4a, static_cast<int16_t>(scaled2));
    }
    const uint32_t bit_idx = lwu(0x0015ed84u);
    const uint32_t mask = (1u << (bit_idx & 31u)) | 0x80000000u;
    sw(RUMBLE + 0x4c, lwu(RUMBLE + 0x4c) | mask);
    sb(m + 0x20, next_state);
}

int32_t call_class_hook_0(uint32_t fn, uint32_t target, uint32_t nozzle_ee, uint32_t hand, uint32_t fallback_a3) {
    int32_t ret = 0;
    if (class_hook_call_level(fn, target, nozzle_ee, hand, 0.0f, ret)) return ret;
    switch (ovl_code_to_veldin(fn)) {
    case 0x002d56e0u: return e749_hook0(target, nozzle_ee, hand, 0.0f);
    case 0x002cadc0u: return enemy10e_hook0(target, nozzle_ee, hand, 0.0f);
    case 0x002db848u: return hook_default_0(target);
    default:
        if (class_hook_call_l1(fn, target, nozzle_ee, hand, 0.0f, ret)) return ret;
        TODO_PORT(fn, "indirect call")(target, nozzle_ee, hand, fallback_a3);
        return 0;
    }
}

void call_class_hook_8(uint32_t fn, uint32_t proj, uint32_t dir_ee, uint32_t lock_tgt, float z_delta) {
    int32_t ret = 0;
    if (class_hook_call_level(fn, proj, dir_ee, lock_tgt, z_delta, ret)) return;
    switch (ovl_code_to_veldin(fn)) {
    case 0x002d5780u: e749_hook8(proj, dir_ee, lock_tgt, z_delta); break;
    case 0x002cae60u: enemy10e_hook8(proj, dir_ee, lock_tgt, z_delta); break;
    case 0x002db858u: hook_default_8(proj); break;
    default:
        if (!class_hook_call_l1(fn, proj, dir_ee, lock_tgt, z_delta, ret)) {
            TODO_PORT(fn, "indirect call")(proj, dir_ee, lock_tgt, 0u);
        }
        break;
    }
}

uint64_t sext32_u64(uint32_t x) {
    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(x)));
}

}  // namespace

// orig: 0x002d9f78
int32_t suck_2d9f78() {
    // the hero's sub-state (the original's jump table 0x1e9fb0, 13 entries)
    switch (lwu(HERO + 0x208c)) {
    case 0: case 1: case 5: case 12:  // (0x002d9fb0)
        return 0;
    case 2: case 4:  // (0x002d9fb8)
        return lwu(HERO + 0x2084) == 0x3cu ? 1 : 0;
    default:  // 3, 6..11 (0x002d9fc8), and the sub-states past the table
        return 1;
    }
}

// orig: 0x002defe0
void suck_2defe0(uint32_t m) {
    const uint32_t alphas = OV(0x001e6008u);
    if (lh(alphas) <= 0) return;
    for (uint32_t i = 0; i < 10u; ++i) {
        const uint32_t p = alphas + i * 2u;
        if (lh(p) >= 65) sh(p, 0x40);
        sh(p, static_cast<int16_t>(lhu(p) - 4u));
    }
    sub_001f93e0(OVF(0x002dd8e8u), m);
}

// orig: 0x002ddca0
void suck_2ddca0() {
    sw(OV(0x00161b44u), 1u);
    sh(OV(0x001e6008u), 0);
    sw(OV(0x00161b40u), 1u);
}

// orig: 0x002da1e0
int32_t suck_2da1e0(uint32_t m) {
    const uint32_t fn = lwu(lwu(lwu(m + 0x24) + 0x2c) + 0xc);
    if (fn == 0u) return 1;
    int32_t ret = 0;
    if (class_hook_call_level(fn, m, 0u, 0u, 0.0f, ret)) return ret;
    switch (ovl_code_to_veldin(fn)) {
    case 0x002d57d0u: return e749_hookc(m, 0u, 0u, 0.0f);
    case 0x002caf10u: return enemy10e_hookc(m, 0u, 0u, 0.0f);
    case 0x002db860u: return hook_default_c(m);
    default:
        if (class_hook_call_l1(fn, m, 0u, 0u, 0.0f, ret)) return ret;
        TODO_PORT(fn, "indirect call")(m, 0u, 0u, 0u);
        return 0;
    }
}

// orig: 0x002d9fd0
void suck_2d9fd0(uint32_t a0, uint32_t a1, uint32_t a2) {
    Mat4 rot{};
    euler_to_mat4_vu(rot, V(a2));
    Mat4 base{};
    mat3_copy_affine(base, M(a0 + 0xc0));
    Mat4 prod{};
    mat4_mul(prod, base, rot);
    mat3_copy(M(a1 + 0xc0), prod);
    sh(a1 + 0x34, static_cast<int16_t>(lhu(a1 + 0x34) | 0x4u));
    moby_update_transform_kept(a1);
}

// orig: 0x002da058
void suck_2da058(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0) {
    (void)a1;
    const uint32_t fn = lwu(lwu(lwu(a2 + 0x24) + 0x2c) + 0x0);
    if (fn == 0u) return;
    const int32_t res = call_class_hook_0(fn, a2, t0, a0, a3);
    if (res != 2) return;
    if (lh(a3 + 0x68) == 3) return;

    const bool gold = lbu(GOLD_SUCK_CANNON) != 0u;
    const int32_t total = static_cast<int32_t>(lwu(HERO + 0x2078) + lwu(HERO + 0x207c));
    if (total >= (gold ? 10 : 5)) return;

    const uint8_t anim = lbu(lwu(a3 + 0x70) + 0x3);
    if (lbu(a2 + 0x53) != anim) {
        moby_anim_set(a2, anim, 0, frame_scale_i(10));
    }
    sh(a3 + 0x68, 3);
    sw(a3 + 0x6c, 0u);
    swc1(a2 + 0x58, 1.0f);
    swc1(a3 + 0x64, vec2_distance(V(a2 + 0x10), V(t0)));

    const int32_t cap = (lbu(GOLD_SUCK_CANNON) != 0u) ? 10 : 5;
    for (int32_t slot = 0; slot < cap; ++slot) {
        const uint32_t slot_addr = SUCK_SLOTS + static_cast<uint32_t>(slot) * 4u;
        if (lwu(slot_addr) == 0u) {
            sw(slot_addr, a2);
            sh(a3 + 0x6a, static_cast<int16_t>(slot));
            sw(HERO + 0x207c, lwu(HERO + 0x207c) + 1u);
            break;
        }
    }
}

// orig: 0x002da250
void suck_2da250(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    moby_chain_point(a0, 0u, a3);
    const uint32_t state = lwu(HERO + 0x2084);
    if (state == 1u || state == 0x1eu) {
        const uint32_t cam = OV(0x00166c80u);
        swc1(a1, lwc1(cam + 0x158));
        swc1(a2, -lwc1(cam + 0x154));
        Vec4 ofs{u2f(0x3f99999au), 0.0f, 0.0f, 0.0f};
        mat3_transform(ofs, ofs, M(cam + 0x350));
        swc1(a3 + 0x8, fpu_add(ofs.z, lwc1(a0 + 0x18)));
        swc1(a3 + 0x0, fpu_add(ofs.x, lwc1(HERO + 0x80)));
        swc1(a3 + 0x4, fpu_add(ofs.y, lwc1(HERO + 0x84)));
    } else {
        const int32_t aim_mode = lh(HERO + 0x22c8);
        const float yaw = lwc1(HERO + 0x688);
        swc1(a1, yaw);
        swc1(a2, aim_mode == 1 ? lwc1(a0 + 0x44) : lwc1(HERO + 0x684));
    }
}

// orig: 0x002ddcb8
void suck_2ddcb8(uint32_t a0, uint32_t a1) {
    if (lwu(OV(0x00161b40u)) == 0u) return;

    swc1(OV(0x00161b84u), u2f(0x3fd9999au));
    sw(OV(0x00161b40u), 0u);

    Mat4 frame{};
    build_aim_frame(a0, frame);

    Vec4 step{};
    vec3_set_length(step, frame.r[0], lwc1(OV(0x00161b84u)));
    Vec4 up_unit{};
    vec3_set_length(up_unit, frame.r[2], 1.0f);

    const uint32_t pts = OV(0x001e5f40u);
    const uint32_t pts_prev = OV(0x001e5f30u);
    const uint32_t alphas = OV(0x001e6008u);
    V(pts) = V(a1);
    sh(alphas, 0);
    for (uint32_t i = 1; i < 10u; ++i) {
        vec3_add(V(pts + i * 0x10u), V(pts_prev + i * 0x10u), step);
        sh(alphas + i * 2u, 0);
    }

    vec_clear(V(OV(0x00161b60u)));
    const Vec4 sway_vec{0.0f, 0.0f, u2f(0x3dcccccd), 1.0f};
    const Vec4 sway_axis{1.0f, 0.0f, 0.0f, 1.0f};
    const float ang = rand_angle();
    vec_rotate_axis(V(OV(0x00161b70u)), sway_vec, sway_axis, ang);

    swc1(OV(0x00161b80u), ang);
    swc1(OV(0x00161b50u), u2f(0x3bc49ba6u));
    sw(OV(0x00161b54u), 0u);
    const uint32_t reset_particles = lwu(OV(0x00161b44u));
    sw(OV(0x00161b58u), 0u);
    if (reset_particles != 0u) {
        sw(OV(0x00161b44u), 0u);
        const uint32_t ptable = OV(0x001e4c80u);
        for (int32_t i = 199; i >= 0; --i) {
            sw(ptable + static_cast<uint32_t>(i) * 0x18u + 0x14u, 0u);
        }
    }
}

// orig: 0x002ddf60
void suck_2ddf60(uint32_t a0, uint32_t a1) {
    suck_2ddcb8(a0, a1);

    Mat4 frame{};
    build_aim_frame(a0, frame);

    Vec4 step{};
    vec3_set_length(step, frame.r[0], lwc1(OV(0x00161b84u)));
    Vec4 v10{};
    vec3_set_length(v10, frame.r[2], 1.0f);

    V(OV(0x001e5f40u)) = V(a1);

    Vec4 ray_end{};
    vec3_set_length(ray_end, frame.r[0], fpu_mul(lwc1(OV(0x00161b84u)), 10.0f));
    vec3_add(ray_end, ray_end, V(a1));

    const uint32_t alphas = OV(0x001e6008u);
    if (segment_collide_v(V(a1), ray_end, 0x14u, lwu(HERO + 0x2080), nullptr) != 0) {
        const float dist = vec2_distance(V(OV(0x00173e60u)), V(a1));
        const int32_t seg = float_to_int(fpu_div(dist, lwc1(OV(0x00161b84u))));
        for (int32_t i = seg; i < 10; ++i) {
            const uint32_t p = alphas + (static_cast<uint32_t>(i) << 1);
            if (lh(p) >= 65) sh(p, 0x40);
            const uint32_t dec = lhu(p) - 4u;
            sh(p, static_cast<int16_t>(dec));
            if (static_cast<int16_t>(dec) < 0) sh(p, 0);
        }
        for (int32_t i = seg - 1; i >= 0; --i) {
            const uint32_t p = alphas + (static_cast<uint32_t>(i) << 1);
            sh(p, static_cast<int16_t>(lhu(p + 2) + 0x20u));
        }
    } else {
        sh(alphas, static_cast<int16_t>(lhu(alphas) + 4u));
        for (uint32_t i = 1; i < 10u; ++i) {
            const uint32_t p = alphas + i * 2u;
            sh(p, static_cast<int16_t>(lhu(p - 2) - 0x20u));
        }
    }

    Vec4 sway_from{};
    mat3_transform(sway_from, V(OV(0x00161b60u)), frame);
    vec3_add(sway_from, sway_from, step);

    Vec4 sway_to{};
    mat3_transform(sway_to, V(OV(0x00161b70u)), frame);
    vec3_add(sway_to, sway_to, step);

    const float dur = int_to_float(frame_scale_i(0x3c));
    const int32_t tick = lw(OV(0x00161b54u));
    const float sway_t = fpu_div(static_cast<float>(tick), dur);
    sw(OV(0x00161b54u), static_cast<uint32_t>(tick) + 1u);
    step.x = fpu_add(sway_from.x, fpu_mul(fpu_sub(sway_to.x, sway_from.x), sway_t));
    step.z = fpu_add(sway_from.z, fpu_mul(fpu_sub(sway_to.z, sway_from.z), sway_t));
    step.y = fpu_add(sway_from.y, fpu_mul(fpu_sub(sway_to.y, sway_from.y), sway_t));

    Vec4 ring_up{};
    Vec4 v90{};
    Vec4 va0{};
    const int32_t limit = frame_scale_i(0x3c);
    if (lw(OV(0x00161b54u)) >= limit) {
        V(OV(0x00161b60u)) = V(OV(0x00161b70u));
        vec3_set_length(ring_up, frame.r[0], lwc1(OV(0x00161b84u)));
        v90 = Vec4{0.0f, 0.0f, u2f(0x3dcccccd), 1.0f};
        va0 = Vec4{1.0f, 0.0f, 0.0f, 1.0f};
        const float delta_deg = rand_range_f(45.0f, 315.0f);
        const float next_ang = angle_add(lwc1(OV(0x00161b80u)), fpu_mul(delta_deg, u2f(0x3c8efa35u)));
        vec_rotate_axis(V(OV(0x00161b70u)), v90, va0, next_ang);
        swc1(OV(0x00161b80u), next_ang);
        sw(OV(0x00161b54u), 0u);
    }

    swc1(OV(0x00161b58u), angle_add(lwc1(OV(0x00161b58u)), u2f(0x3dcccccd)));
    ring_up = frame.r[2];

    Mat4 basis0{};
    basis0.r[2] = ring_up;
    vec3_cross_ba(basis0.r[1], step, basis0.r[2]);
    vec3_set_length(basis0.r[1], basis0.r[1], 1.0f);
    vec3_cross_ba(basis0.r[0], basis0.r[2], basis0.r[1]);

    for (uint32_t j = 0; j < 20u; ++j) {
        vec3_scale(v90, V(OV(0x001e4b40u) + j * 0x10u), u2f(0x3dcccccd));
        mat3_transform(v90, v90, basis0);
        vec3_add(V(OV(0x001e6020u) + j * 0x10u), V(OV(0x001e5f40u)), v90);
    }

    Vec4 ve0{};
    Vec4 vf0{};
    Vec4 v100{};
    Vec4 v110{};
    Mat4 basis{};
    Vec4 ring_ofs{};
    for (int32_t ring = 1; ring < 10; ++ring) {
        const uint32_t pt_cur = OV(0x001e5f40u) + (static_cast<uint32_t>(ring) << 4);
        const uint32_t pt_prev = OV(0x001e5f30u) + (static_cast<uint32_t>(ring) << 4);
        vec3_sub(v90, V(pt_cur), V(pt_prev));
        va0 = step;
        vec3_cross_ba(ve0, va0, v90);

        const float dot = vec3_dot(v90, va0);
        const float denom = fpu_mul(vec3_length(v90), lwc1(OV(0x00161b84u)));
        const float target_ang = fpu_sub(u2f(0x3fc90fdbu), asin_f(fpu_div(dot, denom)));
        float spring_ang = camera_angle_spring(
            0.0f, target_ang, u2f(0x3d75c28fu), u2f(0x3e4ccccdu), 0.0f,
            OV(0x001e5fe0u) + (static_cast<uint32_t>(ring) << 2));
        const float diff = angle_sub(target_ang, spring_ang);
        if (u2f(0x3ea0d97cu) < abs_f(diff)) {
            spring_ang = angle_add(spring_ang, angle_sub(diff, u2f(0x3ea0d97cu)));
        }

        vec_rotate_axis(vf0, v90, ve0, spring_ang);
        vec3_set_length(v100, vf0, lwc1(OV(0x00161b84u)));
        vec3_lerp(vf0, vf0, v100, 1.0f);
        const float len_f0 = vec3_length(vf0);
        if (2.0f < len_f0) {
            vec3_scale(vf0, vf0, fpu_div(2.0f, len_f0));
        }

        const float ring_f = static_cast<float>(ring);
        if (ring >= 6) {
            vec3_lerp(vf0, vf0, v90, fpu_add(fpu_div(ring_f, int_to_float(10)), 0.0f));
        }
        vec3_add(V(pt_cur), V(pt_prev), vf0);

        v110 = step;
        basis.r[2] = ring_up;
        vec3_set_length(basis.r[0], v110, 1.0f);
        vec3_cross_ba(basis.r[1], basis.r[0], basis.r[2]);
        vec3_set_length(basis.r[1], basis.r[1], 1.0f);
        vec3_cross_ba(basis.r[2], basis.r[1], basis.r[0]);
        ring_up = basis.r[2];

        const float base_r = fpu_mul(ring_f, u2f(0x3e99999au));
        const float phase_ofs = fpu_mul(ring_f, u2f(0x3f333333u));
        const float wob_amp = u2f(0x3dcccccd);
        const uint32_t ring_verts = OV(0x001e6020u) + static_cast<uint32_t>(ring) * 0x140u;
        for (uint32_t j = 0; j < 20u; ++j) {
            const float base_plus = fpu_add(base_r, wob_amp);
            const float ph = angle_add(fpu_add(lwc1(OV(0x00161b58u)), phase_ofs), 0.0f);
            const float rad = fpu_add(base_plus, fpu_mul(sin_vu(ph), wob_amp));
            vec3_scale(ring_ofs, V(OV(0x001e4b40u) + j * 0x10u), rad);
            mat3_transform(ring_ofs, ring_ofs, basis);
            vec3_add(V(ring_verts + j * 0x10u), V(pt_cur), ring_ofs);
        }

        vec3_set_length(step, vf0, lwc1(OV(0x00161b84u)));
    }

    draw_callback_add(OVF(0x002dd8e8u), a0);

    const uint32_t ptable_base = OV(0x001e4c80u);
    const uint32_t ptable_dist = OV(0x001e4c84u);
    const uint32_t ptable_spd = OV(0x001e4c90u);
    const uint32_t ptable_ptr = OV(0x001e4c94u);
    int32_t spawned_main = 0;
    int32_t spawned_side = 0;

    for (int32_t idx = 0; idx < 200; ++idx) {
        const uint32_t ofs = static_cast<uint32_t>(idx) * 0x18u;
        if (lwu(ptable_ptr + ofs) == 0u && spawned_main < 5) {
            int32_t r = 9;
            while (r > 0 && lh(alphas + static_cast<uint32_t>(r) * 2u) < 64) {
                --r;
            }
            if (r > 0) {
                const uint32_t r_ofs16 = static_cast<uint32_t>(r) << 4;
                if (r >= 2 && spawned_side < 2) {
                    const uint32_t seg_idx = static_cast<uint32_t>(rand_mod(20));
                    const float lerp_t = rand_range_f(u2f(0x3e4ccccdu), 1.0f);
                    const uint32_t v0_addr = OV(0x001e6020u) + (seg_idx << 4);
                    vec3_lerp(v90, V(v0_addr), V(v0_addr + static_cast<uint32_t>(r) * 0x140u), lerp_t);
                    vec3_lerp(ve0, V(OV(0x001e5f40u)), V(OV(0x001e5f40u) + r_ofs16), lerp_t);
                    const float psize = rand_range_f(60000.0f, 180000.0f);
                    const int32_t spin = rand_mod(5) + 1;
                    const uint32_t side_p = spawn_particle_17_v(
                        v90, spin, va0, 0x00404040u, 0.0f, 1.0f, u2f(0x3f83d70au), psize);
                    if (side_p != 0u) {
                        vec3_sub(vf0, v90, ve0);
                        ++spawned_side;
                        vec3_set_length(V(side_p + 0x30), vf0, u2f(0x3c23d70au));
                        swc1(side_p + 0x38, fpu_add(lwc1(side_p + 0x38), u2f(0x3ca3d70au)));
                    }
                }

                v90 = V(OV(0x001e5f40u) + r_ofs16);
                vec_clear(va0);
                const float psize = rand_range_f(60000.0f, 180000.0f);
                const uint32_t p = spawn_particle_17_v(v90, 6, va0, 0x7f404040u, 0.0f, 1.0f, 1.0f, psize);
                sw(ptable_ptr + ofs, p);
                if (p != 0u) {
                    sh(p + 0xa, 0xc8);
                    sw(p + 0x24, 2u);
                    ++spawned_main;
                    sb(p + 0x2a, 0x7fu);
                    sb(p + 0x2b, 0xc8u);
                }
                sh(ptable_base + ofs, static_cast<int16_t>(r));
                sw(ptable_dist + ofs, 0u);
                sw(ptable_base + ofs + 0x8, 0u);
                sh(ptable_base + ofs + 0x2, static_cast<int16_t>(rand_mod(20)));
                swc1(ptable_base + ofs + 0xc, rand_range_f(u2f(0x3dcccccd), u2f(0x3f4ccccdu)));
                swc1(ptable_spd + ofs, rand_range_f(u2f(0x3dcccccd), u2f(0x3ecccccd)));
            }
        }

        uint32_t p = lwu(ptable_ptr + ofs);
        if (p == 0u) continue;

        int32_t r = lh(ptable_base + ofs);
        vec3_sub(v90, V(OV(0x001e5f30u) + (static_cast<uint32_t>(r) << 4)),
                 V(OV(0x001e5f40u) + (static_cast<uint32_t>(r) << 4)));
        float seg_len = vec3_length(v90);
        swc1(ptable_dist + ofs, fpu_add(lwc1(ptable_dist + ofs), lwc1(ptable_spd + ofs)));
        sh(p + 0xa, 0xc8);
        if (seg_len < lwc1(ptable_dist + ofs)) {
            const uint16_t next_r = static_cast<uint16_t>(lhu(ptable_base + ofs) - 1u);
            sh(ptable_base + ofs, static_cast<int16_t>(next_r));
            if (static_cast<int16_t>(next_r) <= 0) {
                free_particle(lwu(ptable_ptr + ofs));
                sw(ptable_ptr + ofs, 0u);
            } else {
                swc1(ptable_dist + ofs, fpu_sub(lwc1(ptable_dist + ofs), seg_len));
                r = lh(ptable_base + ofs);
                vec3_sub(v90, V(OV(0x001e5f30u) + (static_cast<uint32_t>(r) << 4)),
                         V(OV(0x001e5f40u) + (static_cast<uint32_t>(r) << 4)));
                seg_len = vec3_length(v90);
            }
        }

        p = lwu(ptable_ptr + ofs);
        if (p == 0u) continue;

        const float dist_cur = lwc1(ptable_dist + ofs);
        r = lh(ptable_base + ofs);
        const float frac_len = fpu_div(dist_cur, seg_len);
        if (r == 2) {
            sh(p + 0xa, 0x64);
        } else if (r < 2) {
            const int32_t life = float_to_int(fpu_mul(fpu_sub(1.0f, frac_len), 100.0f));
            sh(p + 0xa, static_cast<int16_t>(life));
            if (lh(p + 0xa) < 61) sh(p + 0xa, 0x3c);
        }

        r = lh(ptable_base + ofs);
        if (r == 1) {
            vec3_sub(va0, V(a1), V(p + 0x10));
            vec3_set_length(va0, va0, lwc1(ptable_spd + ofs));
            vec3_add(V(p + 0x10), V(p + 0x10), va0);
            const float shrink = fpu_add(fpu_mul(lwc1(0x0015ed60u), u2f(0xbd4cccd0u)), 1.0f);
            swc1(p + 0xc, fpu_mul(lwc1(p + 0xc), shrink));
        } else {
            vec3_scale(v90, v90, frac_len);
            r = lh(ptable_base + ofs);
            vec3_add(V(p + 0x10), V(OV(0x001e5f40u) + (static_cast<uint32_t>(r) << 4)), v90);

            r = lh(ptable_base + ofs);
            const int32_t seg0 = lh(ptable_base + ofs + 0x2);
            const uint32_t r_ring_ofs = static_cast<uint32_t>(r) * 0x140u;
            const uint32_t r_pt_ofs = static_cast<uint32_t>(r) << 4;
            vec3_sub(va0, V(OV(0x001e6020u) + (static_cast<uint32_t>(seg0) << 4) + r_ring_ofs),
                     V(OV(0x001e5f40u) + r_pt_ofs));
            vec3_sub(ve0, V(OV(0x001e5ee0u) + (static_cast<uint32_t>(seg0) << 4) + r_ring_ofs),
                     V(OV(0x001e5f30u) + r_pt_ofs));
            vec4_lerp_v(vf0, va0, ve0, frac_len);

            const int32_t seg1 = ee_rem(seg0 + 19, 20);
            vec3_sub(v100, V(OV(0x001e6020u) + (static_cast<uint32_t>(seg1) << 4) + r_ring_ofs),
                     V(OV(0x001e5f40u) + r_pt_ofs));
            vec3_sub(v110, V(OV(0x001e5ee0u) + (static_cast<uint32_t>(seg1) << 4) + r_ring_ofs),
                     V(OV(0x001e5f30u) + r_pt_ofs));
            vec4_lerp_v(basis.r[0], v100, v110, frac_len);
            vec4_lerp_v(vf0, vf0, basis.r[0], lwc1(ptable_base + ofs + 0x8));

            float ang_t = fpu_add(lwc1(ptable_base + ofs + 0x8), lwc1(ptable_base + ofs + 0xc));
            swc1(ptable_base + ofs + 0x8, ang_t);
            if (1.0f <= ang_t) {
                ang_t = fpu_sub(ang_t, 1.0f);
                swc1(ptable_base + ofs + 0x8, ang_t);
                sh(ptable_base + ofs + 0x2, static_cast<int16_t>(seg1));
            }
            vec3_add(V(p + 0x10), V(p + 0x10), vf0);
        }
    }
}

// orig: 0x002df1e0
void suck_2df1e0(uint32_t a0) {
    if (a0 == 0u) return;
    const uint8_t st = lbu(a0 + 0x20);
    if (st == 0xfeu || st == 0xfdu) return;
    if (rand_mod(10) != 0) return;

    const uint32_t cls = lwu(a0 + 0x24);
    if (!(cls != 0u && lh(cls + 0x46) == 0x13) && moby_is_pickup(a0) == 0) return;
    if (!(vec3_distance(V(a0 + 0x10), V(HERO + 0x80)) < 30.0f)) return;

    const uint32_t alphas = OV(0x001e6008u);
    if (lh(alphas) < 8) return;

    const uint32_t pts = OV(0x001e5f40u);
    for (int32_t i = 0; i < 10; ++i) {
        if (i > 0 && lh(alphas + static_cast<uint32_t>(i) * 2u) < 8) break;
        float rad = fpu_add(fpu_mul(static_cast<float>(i), u2f(0x3e99999au)), u2f(0x3dcccccd));
        if (rad < 2.0f) rad = 2.0f;
        if (vec3_distance(V(a0 + 0x10), V(pts + static_cast<uint32_t>(i) * 0x10u)) < rad) {
            const uint32_t cur_cls = lwu(a0 + 0x24);
            if (cur_cls != 0u && lh(cur_cls + 0x46) == 0x13) {
                bolt_fly_to(a0, lwu(HERO + 0x2080), 0.0f, 0.0f);
            } else {
                pickup_collect(a0);
            }
        }
    }
}

// orig: 0x002da360
void suck_2da360(uint32_t a0, uint32_t a1) {
    const uint32_t buf = ee_stack_alloc(0x20);  // nozzle (+0x00), yaw (+0x10), pitch (+0x14) passed by EE address
    const uint32_t nozzle_ee = buf;
    const uint32_t yaw_ee = buf + 0x10u;
    const uint32_t pitch_ee = buf + 0x14u;

    suck_2da250(a0, yaw_ee, pitch_ee, nozzle_ee);
    suck_2ddf60(a0, nozzle_ee);

    uint32_t m = lwu(OV(0x0015ffe4u));
    if (m != 0u) {
        const uint32_t default_hook0 = OVF(0x002db848u);
        do {
            if (lh(m + 0xa6) == 0 || (lhu(m + 0x34) & 0x1000u) == 0u ||
                lwu(lwu(lwu(m + 0x24) + 0x2c) + 0x0) == default_hook0) {
                suck_2df1e0(m);
            } else {
                const uint32_t rec = class_hook_10(m);
                if (rec != 0u && lh(rec + 0x68) < 6) {
                    const float dist2d = vec2_distance(V(a0 + 0x10), V(m + 0x10));
                    const bool can_suck =
                        (dist2d < 15.0f) &&
                        (suck_2d9f78() == 0) &&
                        (lw(HERO + 0x2078) < (lbu(GOLD_SUCK_CANNON) != 0u ? 10 : 5));
                    if (!can_suck) {
                        if (lh(rec + 0x68) != 0) suck_2da1e0(m);
                    } else {
                        const int32_t st = lh(rec + 0x68);
                        if (st >= 3) {
                            if (st < 4) suck_2da058(a0, a1, m, rec, nozzle_ee);
                        } else {
                            const float cone_sq = (dist2d < 3.0f) ? u2f(0x406be54eu) : u2f(0x3d33abb1u);
                            sh(rec + 0x6a, -1);
                            Vec4 mpos = V(m + 0x10);
                            mpos.z = fpu_add(mpos.z, u2f(0x3ecccccd));
                            const float yaw_to = atan2_f(fpu_sub(mpos.x, lwc1(a0 + 0x10)),
                                                         fpu_sub(mpos.y, lwc1(a0 + 0x14)));
                            const float dyaw = angle_dist(lwc1(yaw_ee), yaw_to);
                            bool in_cone = false;
                            if (fpu_mul(dyaw, dyaw) < cone_sq) {
                                const float pitch_to = atan2_f(dist2d, fpu_sub(mpos.z, lwc1(a0 + 0x18)));
                                const float dpitch = angle_dist(lwc1(pitch_ee), pitch_to);
                                if (fpu_mul(dpitch, dpitch) < cone_sq &&
                                    segment_collide_v(V(nozzle_ee), mpos, 2u, m, nullptr) == 0) {
                                    in_cone = true;
                                }
                            }
                            if (in_cone) {
                                suck_2da058(a0, a1, m, rec, nozzle_ee);
                            } else if (lh(rec + 0x68) > 0) {
                                suck_2da1e0(m);
                            }
                        }
                    }
                }
            }
            m = lwu(m + 0x28);
        } while (m != 0u);
    }

    ee_stack_free(0x20);
}

// orig: 0x002df048
void suck_2df048(uint32_t a0) {
    const float half_spd = fpu_mul(lwc1(OV(0x00161b50u)), 0.5f);
    const uint32_t ptable_base = OV(0x001e4c80u);
    const uint32_t pts = OV(0x001e5f40u);
    const uint32_t pts_prev = OV(0x001e5f30u);

    sw(OV(0x00161b44u), 1u);
    swc1(OV(0x00161b50u), half_spd);
    sw(OV(0x00161b40u), 1u);

    Vec4 dir{};
    Vec4 axis_pt{};
    Vec4 radial{};
    for (uint32_t entry = ptable_base; static_cast<int32_t>(entry) < static_cast<int32_t>(pts); entry += 0x18u) {
        const uint32_t p = lwu(entry + 0x14);
        if (p == 0u) continue;
        sw(entry + 0x14, 0u);

        int32_t r = lh(entry);
        vec3_sub(dir, V(pts_prev + (static_cast<uint32_t>(r) << 4)),
                 V(pts + (static_cast<uint32_t>(r) << 4)));
        const float len = vec3_length(dir);
        vec3_scale(dir, dir, fpu_div(lwc1(entry + 0x4), len));

        r = lh(entry);
        vec3_add(axis_pt, V(pts + (static_cast<uint32_t>(r) << 4)), dir);
        vec3_sub(radial, V(p + 0x10), axis_pt);
        vec3_set_length(radial, radial, u2f(0x3cf5c28fu));
        vec3_scale(dir, dir, fpu_div(u2f(0x3dcccccd), len));
        vec3_add(V(p + 0x30), radial, dir);
        sh(p + 0xa, 0x46);
    }

    suck_2defe0(a0);
}

// orig: 0x002da660
void suck_2da660() {
    sw(HERO + 0x2078, 0u);
    sw(HERO + 0x207c, 0u);
    for (int32_t i = 0; i < (lbu(GOLD_SUCK_CANNON) != 0u ? 10 : 5); ++i) {
        const uint32_t slot_addr = SUCK_SLOTS + (static_cast<uint32_t>(i) << 2);
        const uint32_t m = lwu(slot_addr);
        if (m == 0u) continue;
        const uint32_t rec = class_hook_10(m);
        int32_t st = 0;
        if (rec == 0u || (st = lh(rec + 0x68)) < 3 || st >= 5 || lh(rec + 0x6a) == -1) {
            debug_printf_stub();
            sw(slot_addr, 0u);
        } else if (st == 3) {
            sw(HERO + 0x207c, lwu(HERO + 0x207c) + 1u);
        } else if (st == 4) {
            sw(HERO + 0x2078, lwu(HERO + 0x2078) + 1u);
        }
    }
}

// orig: 0x002da790
void suck_cannon_update(uint32_t m) {
    camera_request(4u, 0x753fu, OVF(0x00237798u), OVF(0x002377b8u), OVF(0x00237840u), 0x001413c8u, 5u);

    const uint32_t pvar = lwu(m + 0x78);
    const bool mode_is_3 = (lwu(0x00140404u) == 3u);
    const uint8_t cur_state = lbu(m + 0x20);
    if (cur_state != 3u && cur_state != 0u) {
        suck_2defe0(m);
    }
    suck_2da660();

    // the state machine (the original's jump table 0x1ea010: one case per state, 0..6; states past it do nothing)
    const uint8_t st = lbu(m + 0x20);
    switch (st) {
    case 0: {  // init (0x002da880)
        for (int32_t i = 0; i < (lbu(GOLD_SUCK_CANNON) != 0u ? 10 : 5); ++i) {
            const uint32_t slot_addr = SUCK_SLOTS + (static_cast<uint32_t>(i) << 2);
            const uint32_t entry = lwu(slot_addr);
            if (entry != 0u) {
                sw(entry + 0x94, 0u);
                const uint32_t entry2 = lwu(slot_addr);
                sh(entry2 + 0x34, static_cast<int16_t>(lhu(entry2 + 0x34) & 0xefffu));
            }
        }
        const int32_t loaded = lw(HERO + 0x2078);
        sb(HERO + 0x10ac, 0u);
        if (loaded > 0) {
            sw(pvar + 0xc, 1u);
            moby_anim_set(m, 6u, 0, frame_scale_i(4));
            sb(m + 0x20, 4u);
        } else {
            sw(pvar + 0x8, 0u);
            sb(m + 0x20, 1u);
        }
        suck_2ddca0();
        break;
    }

    case 1: {  // idle, empty (0x002da938)
        if ((lbu(m + 0x70) & 2u) != 0u) {
            moby_anim_set(m, 1u, 0, frame_scale_i(0));
        }
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pvar + 0x8)));
        if (lwu(pvar + 0x8) == 0u) {
            hero_hold_release();
        }
        if (lw(HERO + 0x2078) > 0) {
            moby_anim_set(m, 6u, 0, frame_scale_i(4));
            sb(m + 0x20, 4u);
        } else if ((lwu(PAD_BTN) & lwu(HERO + 0x10a0)) != 0u &&
                   lwu(pvar + 0x8) == 0u &&
                   lbu(HERO + 0x20a7) == 0u &&
                   lbu(HERO + 0x20ac) == 0u &&
                   frame_scale_i(15) < lw(HERO + 0x10b0) &&
                   suck_2d9f78() == 0) {
            hero_hold_start();
            sw(pvar + 0x4, static_cast<uint32_t>(moby_sound_start(2, 4u, m)));
            sw(pvar + 0x8, static_cast<uint32_t>(frame_scale_i(10)));
            start_suck_rumble(m, 2u);
        }
        break;
    }

    case 2: {  // winding up (0x002daad4)
        if ((lbu(m + 0x70) & 2u) != 0u) {
            moby_anim_set(m, 3u, 0, frame_scale_i(5));
        }
        if (suck_2d9f78() != 0 || lbu(HERO + 0x20ac) != 0u || lbu(HERO + 0x20a7) != 0u) {
            stop_suck_sound(m, pvar);
            hero_hold_release();
            const int32_t loaded = lw(HERO + 0x2078);
            sb(HERO + 0x10ac, 0u);
            if (loaded > 0) {
                moby_anim_set(m, 6u, 0, frame_scale_i(4));
                sb(m + 0x20, 4u);
            } else {
                sw(pvar + 0x8, 0u);
                moby_anim_set(m, 1u, 0, frame_scale_i(4));
                sb(m + 0x20, 1u);
            }
        } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pvar + 0x8))) != 0) {
            sw(pvar + 0x8, static_cast<uint32_t>(frame_scale_i(20)));
            sb(m + 0x20, 3u);
        }
        break;
    }

    case 3: {  // sucking (0x002dac00)
        const uint8_t cur_anim = lbu(m + 0x53);
        if (cur_anim != 3u && (cur_anim != 0u || (lbu(m + 0x70) & 2u) != 0u)) {
            moby_anim_set(m, 3u, 0, frame_scale_i(5));
        }
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pvar + 0x8)));

        bool keep_sucking = false;
        if (lwu(HERO + 0x207c) != 0u) {
            keep_sucking = true;
        } else {
            const int32_t cap = (lbu(GOLD_SUCK_CANNON) != 0u) ? 10 : 5;
            if (lw(HERO + 0x2078) < cap && suck_2d9f78() == 0 &&
                ((lwu(PAD_BTN) & lwu(HERO + 0x10a0)) != 0u || lwu(pvar + 0x8) != 0u) &&
                lbu(HERO + 0x20ac) == 0u && lbu(HERO + 0x20a7) == 0u) {
                keep_sucking = true;
            }
        }

        if (!keep_sucking) {
            hero_hold_release();
            stop_suck_sound(m, pvar);
            suck_2df048(m);
            const int32_t loaded = lw(HERO + 0x2078);
            sb(HERO + 0x10ac, 0u);
            if (loaded > 0) {
                moby_anim_set(m, 6u, 0, frame_scale_i(4));
                sw(pvar + 0x8, 0u);
                sw(pvar + 0xc, 1u);
                sb(m + 0x20, 4u);
            } else {
                sw(pvar + 0x8, 0u);
                moby_anim_set(m, 1u, 0, frame_scale_i(4));
                sb(m + 0x20, 1u);
            }
        } else {
            if ((lbu(m + 0x70) & 2u) != 0u) {
                moby_anim_set(m, 3u, 0, frame_scale_i(2));
            }
            suck_2da360(m, pvar);
        }
        break;
    }

    case 4: {  // loaded, aiming (0x002dadd4)
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pvar + 0x8)));
        if (lwu(pvar + 0x8) == 0u) {
            hero_hold_release();
        }
        if ((lbu(m + 0x70) & 2u) != 0u) {
            moby_anim_set(m, 6u, 0, frame_scale_i(0));
            hero_hold_release();
        }
        if ((lwu(PAD_BTN) & lwu(HERO + 0x10a0)) == 0u) {
            sw(pvar + 0xc, 0u);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pvar + 0x14))) != 0) {
            sw(pvar + 0x10, 0u);
        } else {
            const uint32_t tgt = lwu(pvar + 0x10);
            if (tgt != 0u && (lbu(tgt + 0x20) & 0x80u) != 0u) {
                sw(pvar + 0x10, 0u);
            }
        }

        if (lwu(HERO + 0x2084) == 1u) {
            hero_set_state(0x1eu, 1);
        }
        if (lwu(HERO + 0x2084) == 0x1eu &&
            (lhu(lwu(HERO + 0x2080) + 0x34) & 1u) != 0u &&
            lbu(HERO + 0x20ac) == 0u) {
            Vec4 ray_vec{0.0f, 0.0f, 50.0f, 0.0f};
            const uint32_t cam = OV(0x00166c80u);
            Mat4 cam_rot{};
            mat3_copy(cam_rot, M(cam));
            Mat4 cam_inv{};
            mat3_transpose(cam_inv, cam_rot);
            mat4_transform(ray_vec, ray_vec, cam_inv);
            Vec4 ray_end{};
            vec3_add(ray_end, V(cam + 0x140), ray_vec);
            if (segment_collide_v(V(cam + 0x140), ray_end, 0u, m, nullptr) != 0) {
                const uint32_t col = OV(0x00173e40u);
                const uint32_t hit_m = lwu(col + 0x18);
                if (hit_m != 0u && (lhu(hit_m + 0x34) & 0x1000u) != 0u) {
                    sw(pvar + 0x10, hit_m);
                    sw(pvar + 0x14, static_cast<uint32_t>(frame_scale_i(20)));
                    const uint32_t cur_tgt = lwu(pvar + 0x10);
                    swc1(pvar + 0x18, fpu_sub(lwc1(col + 0x28), lwc1(cur_tgt + 0x18)));
                }
            }
            const uint32_t color = (lwu(pvar + 0x10) != 0u && lwu(pvar + 0x8) == 0u)
                                       ? 0xff0fff0fu
                                       : 0xff0f0fffu;
            cam_effect_add(m, color, nullptr, 0x21u, -1, 1.0f, 0.0f, 90.0f);
        }

        if (lwu(HERO + 0x12b0) != 0u) {
            sw(pvar + 0x10, lwu(HERO + 0x12b8));
            sw(pvar + 0x14, static_cast<uint32_t>(frame_scale_i(20)));
            const uint32_t cur_tgt = lwu(pvar + 0x10);
            swc1(pvar + 0x18, fpu_sub(lwc1(OV(0x00173e68u)), lwc1(cur_tgt + 0x18)));
        }

        if (lwu(pvar + 0xc) == 0u &&
            lw(HERO + 0x2078) > 0 &&
            lwu(pvar + 0x8) == 0u &&
            (lwu(PAD_BTN) & lwu(HERO + 0x10a0)) != 0u) {
            const uint32_t hs = lwu(HERO + 0x208c);
            if (hs != 7u && hs != 3u && hs != 0x12u && hs != 0x11u &&
                lbu(HERO + 0x20ac) == 0u &&
                lbu(HERO + 0x20a7) == 0u &&
                frame_scale_i(22) < lw(HERO + 0x10b0)) {
                hero_hold_start();
                moby_anim_set(m, 5u, 0, frame_scale_i(2));
                sb(HERO + 0x10ac, 2u);
                sw(pvar + 0x8, static_cast<uint32_t>(frame_scale_i(25)));
                start_suck_rumble(m, 5u);
            }
        }
        break;
    }

    case 5: {  // firing a captured moby (0x002db1ec)
        if (!(0.0f <= moby_anim_time(m))) break;

        uint32_t proj = 0u;
        for (int32_t i = 0; i < (lbu(GOLD_SUCK_CANNON) != 0u ? 10 : 5); ++i) {
            const uint32_t slot_addr = SUCK_SLOTS + (static_cast<uint32_t>(i) << 2);
            const uint32_t cand = lwu(slot_addr);
            if (cand != 0u) {
                proj = cand;
                sw(slot_addr, 0u);
                break;
            }
        }
        if (proj == 0u) break;

        const uint32_t shot_buf = ee_stack_alloc(0x20);  // dir (+0x00) and from (+0x10) passed by EE address
        const uint32_t dir_ee = shot_buf;
        const uint32_t from_ee = shot_buf + 0x10u;

        const uint32_t cam = OV(0x00166c80u);
        vec_clear(V(dir_ee));
        Mat4 cam_rot{};
        mat3_copy(cam_rot, M(cam));
        Mat4 cam_inv{};
        mat3_transpose(cam_inv, cam_rot);

        if (lwu(HERO + 0x2084) == 0x1eu && (lhu(lwu(HERO + 0x2080) + 0x34) & 1u) != 0u) {
            const Vec4 fwd_local{0.0f, 0.0f, fpu_mul(lwc1(0x0015ed60u), u2f(0x3ee47454u)), 0.0f};
            mat4_transform(V(dir_ee), fwd_local, cam_inv);
            const Vec4 muzzle_local{u2f(0x3e19999au), u2f(0x3e99999au), 1.0f, 0.0f};
            mat4_transform(V(from_ee), muzzle_local, cam_inv);
            vec3_add(V(from_ee), V(from_ee), V(cam + 0x140));
        } else {
            const Vec4 fwd_local{fpu_mul(lwc1(0x0015ed60u), u2f(0x3ee47454u)), 0.0f, 0.0f, 0.0f};
            mat3_transform(V(dir_ee), fwd_local, M(HERO + 0x640));
            moby_chain_point(m, 0u, from_ee);
            advanced_aim_shot(from_ee, dir_ee);  // enhancement: modern aim line (advanced.h)
        }

        V(proj + 0x10) = V(from_ee);
        moby_update_transform_kept(proj);
        vec3_add(V(dir_ee), V(dir_ee), V(HERO + 0x100));

        if (!(lwu(HERO + 0x2084) == 0x1eu && (lhu(lwu(HERO + 0x2080) + 0x34) & 1u) != 0u)) {
            float best_dist = 10000.0f;
            uint32_t best_m = 0u;
            float aim_yaw = (lwu(HERO + 0x12b0) != 0u)
                                ? lwc1(HERO + 0x12bc)
                                : atan2_f(lwc1(dir_ee), lwc1(dir_ee + 0x4));
            const float xy_len = vec2_length(V(dir_ee));
            float aim_pitch = -atan2_f(xy_len, lwc1(dir_ee + 0x8));

            uint32_t cand = lwu(OV(0x0015ffe4u));
            if (cand != 0u) {
                const float max_cone = u2f(0x3ee85696u);
                Vec4 cand_pos{};
                Vec4 sph_pt{};
                do {
                    if (static_cast<int8_t>(lbu(cand + 0x20)) >= 0 && cand != proj) {
                        cand_pos = V(cand + 0x10);
                        const uint32_t pv0 = moby_pvar_word0(cand);
                        const float z_ofs = (pv0 != 0u) ? lwc1(pv0 + 0x10) : 0.5f;
                        cand_pos.z = fpu_add(cand_pos.z, z_ofs);

                        const uint32_t cls_p =
                            (cand != 0u && (lhu(cand + 0x34) & 0x1000u) != 0u) ? lwu(cand + 0x24) : 0u;
                        if (cls_p != 0u && lh(cls_p + 0x46) == 5) {
                            const float cand_yaw = atan2_f(fpu_sub(cand_pos.x, lwc1(from_ee)),
                                                           fpu_sub(cand_pos.y, lwc1(from_ee + 0x4)));
                            const float d2 = vec2_distance(V(from_ee), cand_pos);
                            const float cand_pitch = atan2_f(d2, fpu_sub(cand_pos.z, lwc1(from_ee + 0x8)));
                            const float yaw_diff = angle_dist(aim_yaw, cand_yaw);
                            const float d3 = vec3_distance(V(from_ee), cand_pos);

                            if (d3 < 2.5f) {
                                const float hero_to_cand = atan2_f(
                                    fpu_sub(lwc1(cand + 0x10), lwc1(HERO + 0x80)),
                                    fpu_sub(lwc1(cand + 0x14), lwc1(HERO + 0x84)));
                                if (angle_dist(lwc1(HERO + 0x688), hero_to_cand) < u2f(0x3f860a92u) &&
                                    abs_f(cand_pitch) < u2f(0x3f490fdbu)) {
                                    best_m = cand;
                                    break;
                                }
                            }

                            if (!(best_dist < d3)) {
                                vec_from_spherical_v(sph_pt, d3, aim_yaw, aim_pitch);
                                const float half_pi = u2f(0x3fc90fdbu);
                                vec3_add(sph_pt, sph_pt, V(from_ee));
                                const float chord = vec3_distance(sph_pt, cand_pos);
                                const float num = fpu_mul(chord, chord);
                                const float den = fpu_mul(fpu_add(d3, d3), d3);
                                const float cos_a = fpu_sub(1.0f, fpu_div(num, den));
                                float ang = fpu_sub(half_pi, asin_f(cos_a));

                                if (max_cone < ang) {
                                    const float rad = fpu_mul(lwc1(cand + 0xc), u2f(0x3a800000u));
                                    const float hyp = sqrt_f(fpu_add(fpu_mul(d3, d3), fpu_mul(rad, rad)));
                                    const float sub_ang = fpu_sub(half_pi, asin_f(fpu_div(d3, hyp)));
                                    if (rad < d3) {
                                        ang = fpu_sub(ang, sub_ang);
                                    }
                                }

                                if (ang < max_cone) {
                                    if (segment_collide_v(V(OV(0x00166dc0u)), cand_pos, 6u, m, nullptr) != 0) {
                                        break;
                                    }
                                    best_dist = d3;
                                    const bool yaw_close = yaw_diff < u2f(0x3e32b8c2u);
                                    aim_pitch = -cand_pitch;
                                    if (yaw_close) {
                                        aim_yaw = cand_yaw;
                                        best_m = cand;
                                    }
                                }
                            }
                        }
                    }
                    cand = lwu(cand + 0x28);
                } while (cand != 0u);
            }

            if (best_m != 0u) {
                sw(pvar + 0x10, best_m);
            }
        }

        suck_2d9fd0(m, proj, OV(0x00161b30u));
        const uint32_t fn8 = lwu(lwu(lwu(proj + 0x24) + 0x2c) + 0x8);
        if (fn8 != 0u) {
            call_class_hook_8(fn8, proj, dir_ee, lwu(pvar + 0x10), lwc1(pvar + 0x18));
        }
        ee_stack_free(0x20);

        const int32_t rem_loaded = lw(HERO + 0x2078) - 1;
        sw(HERO + 0x2078, static_cast<uint32_t>(rem_loaded));
        if (rem_loaded > 0) {
            sb(HERO + 0x10ac, 0u);
            sb(m + 0x20, 4u);
        } else {
            sw(pvar + 0x8, static_cast<uint32_t>(frame_scale_i(20)));
            sb(HERO + 0x10ac, 0u);
            sb(m + 0x20, 1u);
        }
        break;
    }

    default:  // 6, inactive (0x002db778), and the states past the table
        break;
    }

    if (mode_is_3) {
        stop_suck_sound(m, pvar);
        if ((lwu(PAD_BTN) & 5u) != 0u) {
            sw(0x0013cae8u, lwu(0x0013cae8u) | 5u);
        }
    }
}

// orig: 0x002dd8e8
void suck_beam_draw(uint32_t m) {
    (void)m;
    const uint32_t desc = ee_stack_alloc(0x90);

    const uint64_t tex0 = static_cast<uint64_t>(effect_tex0(0x15));
    float v_scroll = lwc1(OV(0x00161b48u));
    float next_v = fpu_add(v_scroll, u2f(0x3a83126fu));
    sd(desc + 0x78, tex0);
    sd(desc + 0x80, 0x0000ff9000000260ull);
    sd(desc + 0x88, 0x0000008000000048ull);
    sd(desc + 0x70, 0ull);
    swc1(OV(0x00161b48u), next_v);
    if (7.0f < next_v) {
        next_v = fpu_sub(next_v, 7.0f);
        swc1(OV(0x00161b48u), next_v);
    }

    const float dv_step = u2f(0x3e99999au);
    v_scroll = fpu_add(v_scroll, dv_step);
    const float du_step = u2f(0x3d4ccccdu);

    Vec4 edge1{};
    Vec4 edge2{};
    Vec4 normal{};
    Vec4 to_cam{};
    for (int32_t ring = 1; ring < 10; ++ring) {
        float u_cur = lwc1(OV(0x00161b4cu));
        float next_u = fpu_add(u_cur, lwc1(OV(0x00161b50u)));
        swc1(OV(0x00161b4cu), next_u);
        if (7.0f < next_u) {
            next_u = fpu_sub(next_u, 7.0f);
            swc1(OV(0x00161b4cu), next_u);
        }

        const uint32_t alphas = OV(0x001e6008u);
        const uint32_t alpha_prev_addr = alphas + (static_cast<uint32_t>(ring - 1) << 1);
        const uint32_t alpha_cur_addr = alphas + (static_cast<uint32_t>(ring) << 1);
        const uint32_t ring_ofs = static_cast<uint32_t>(ring) * 0x140u;
        const float v_next_ring = fpu_add(v_scroll, dv_step);

        for (int32_t seg = 0; seg < 20; ++seg) {
            const uint32_t seg_ofs = static_cast<uint32_t>(seg) << 4;
            const uint32_t next_seg_ofs = static_cast<uint32_t>(ee_rem(seg + 1, 20)) << 4;

            V(desc + 0x00) = V(OV(0x001e6020u) + ring_ofs + seg_ofs);
            V(desc + 0x10) = V(OV(0x001e6020u) + ring_ofs + next_seg_ofs);
            V(desc + 0x20) = V(OV(0x001e5ee0u) + ring_ofs + seg_ofs);
            V(desc + 0x30) = V(OV(0x001e5ee0u) + ring_ofs + next_seg_ofs);

            vec3_sub(edge1, V(desc + 0x00), V(desc + 0x10));
            vec3_sub(edge2, V(desc + 0x20), V(desc + 0x10));
            vec3_cross_ba(normal, edge2, edge1);
            vec3_sub(to_cam, V(desc + 0x10), V(OV(0x00166dc0u)));

            if (0.0f <= vec3_dot(normal, to_cam)) {
                u_cur = fpu_add(u_cur, du_step);
            } else {
                const float u_next = fpu_add(u_cur, du_step);
                const float v_prev = fpu_sub(v_scroll, dv_step);
                auto pack_col = [](int32_t a) -> uint32_t {
                    if (a <= -1) a = 0;
                    if (a >= 65) a = 0x40;
                    return (static_cast<uint32_t>(a) << 24) | 0x00404040u;
                };
                const uint32_t col_cur = pack_col(lh(alpha_cur_addr));
                const uint32_t col_prev = pack_col(lh(alpha_prev_addr));
                sw(desc + 0x44, col_cur);
                sw(desc + 0x40, col_cur);
                sw(desc + 0x4c, col_prev);
                sw(desc + 0x48, col_prev);

                if (ring == 1) {
                    sw(desc + 0x48, 0x20404040u);
                    sw(desc + 0x4c, 0x20404040u);
                } else if (ring == 9) {
                    sw(desc + 0x40, 0x00404040u);
                    sw(desc + 0x44, 0x00404040u);
                }

                swc1(desc + 0x64, u_cur);
                swc1(desc + 0x54, u_cur);
                swc1(desc + 0x68, v_prev);
                u_cur = u_next;
                swc1(desc + 0x50, v_scroll);
                swc1(desc + 0x58, v_scroll);
                swc1(desc + 0x5c, u_next);
                swc1(desc + 0x60, v_prev);
                swc1(desc + 0x6c, u_next);
                quad_draw(desc, 0u);
            }
        }

        v_scroll = v_next_ring;
    }

    sub_002a0ba8(0x47u, 0x5360bull);
    ee_stack_free(0x90);
}

// orig: 0x00237798
void suck_camera_237798(uint32_t p0) {
    hud_plain_start(p0);
}

// orig: 0x002377b8
void suck_camera_2377b8(uint32_t p0) {
    hud_plain_update(p0);
    sw(p0 + 0x8, lbu(GOLD_SUCK_CANNON) == 0u ? 5u : 10u);
    if (lwu(HERO + 0x10b8) == 9u && lbu(HERO + 0x20a4) == 0u) {
        sw(p0 + 0x7c, static_cast<uint32_t>(frame_scale_i(0x78) + 0x1e));
    } else if (lw(p0 + 0x7c) >= 31) {
        sw(p0 + 0x7c, 0x1eu);
    }
}

// orig: 0x0023bac0
void suck_camera_23bac0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
    const uint32_t hud_tbl = OV(0x0017e4d8u);
    const uint32_t gs_ptr_addr = OV(0x001611c0u);

    const uint32_t entry_tbl = lwu(hud_tbl + 0x20);
    const uint32_t info_tbl = lwu(hud_tbl + 0x24);
    const int32_t info_idx = lh(entry_tbl + (p0 << 2) + 0x2);
    const uint32_t info = info_tbl + (static_cast<uint32_t>(info_idx) << 3);
    const uint32_t u_pow = lbu(info + 0x6);
    const uint32_t v_pow = lbu(info + 0x7);

    const uint32_t q = lwu(gs_ptr_addr);
    sw(q + 0x0, 0x10000007u);
    sw(q + 0x4, 0u);
    sw(q + 0x8, 0u);
    sw(q + 0xc, 0x50000007u);
    const uint32_t pkt = q + 0x10u;
    sw(gs_ptr_addr, pkt);

    sd(pkt + 0x00, 0xb400000000008001ull);
    sd(pkt + 0x08, 0x0000053535353106ull);

    const uint64_t tex0 = static_cast<uint64_t>(hud_tex0(static_cast<int32_t>(p0)));
    const uint64_t rgba = (sext32_u64(p5) << 24) | 0x007f7f7full;
    const uint32_t u_fixed = (1u << (u_pow & 31u)) << 4;
    const uint32_t v_fixed = (1u << (v_pow & 31u)) << 20;

    sd(pkt + 0x20, rgba);
    sd(pkt + 0x28, sext32_u64(u_fixed));
    sd(pkt + 0x10, tex0);
    sd(pkt + 0x18, 0x154ull);

    constexpr uint32_t SCR_OFS = 0x0013e500u;
    const uint32_t x0_4 = p1 << 4;
    const uint32_t y0_4 = p2 << 4;
    const uint32_t x1_4 = (p1 + p3) << 4;
    const uint32_t y1_4 = (p2 + p4) << 4;

    auto pack_xyz = [&](uint32_t x4, uint32_t y4) -> uint64_t {
        const uint32_t gy = y4 + lwu(SCR_OFS + 0x14) - 8u;
        const uint32_t gx = x4 + lwu(SCR_OFS + 0x10) - 8u;
        const uint32_t gz = lwu(hud_tbl + 0xc);
        return sext32_u64(gx) | (sext32_u64(gy) << 16) | (static_cast<uint64_t>(gz) << 32);
    };

    sd(pkt + 0x38, sext32_u64(v_fixed + u_fixed));
    sd(pkt + 0x30, pack_xyz(x0_4, y0_4));
    sd(pkt + 0x48, 0ull);
    sd(pkt + 0x40, pack_xyz(x1_4, y0_4));
    sd(pkt + 0x58, sext32_u64(v_fixed));
    sd(pkt + 0x50, pack_xyz(x0_4, y1_4));
    sd(pkt + 0x68, 0ull);
    sd(pkt + 0x60, pack_xyz(x1_4, y1_4));

    sw(gs_ptr_addr, lwu(gs_ptr_addr) + 0x70u);
}

// orig: 0x00237840
void suck_camera_237840(uint32_t p0) {
    const uint32_t xy = ee_stack_alloc(0x10);  // px (+0x0) and py (+0x4) passed by EE address
    const uint32_t px = xy;
    const uint32_t py = xy + 0x4u;

    const uint32_t w_cfg = lwu(OV(0x0015f760u));
    const uint32_t h_cfg = lwu(OV(0x0015f764u));
    sw(py, lwu(p0 + 0x54));
    sw(px, lwu(p0 + 0x50));
    sw(p0 + 0x58, w_cfg);
    sw(p0 + 0x5c, h_cfg);

    hud_element_place(p0, px, py);
    hud_element_offset(p0, px, py, lw(p0 + 0x6c), 0);

    const uint32_t w2 = lwu(OV(0x0015f760u));
    const uint32_t h2 = lwu(OV(0x0015f764u));
    const int32_t base_x = static_cast<int32_t>((lwu(px) + static_cast<uint32_t>(lh(p0 + 0x48))) << 4);
    const int32_t base_y = static_cast<int32_t>((lwu(py) + static_cast<uint32_t>(lh(p0 + 0x4a))) << 4);
    const int32_t x0 = base_x + static_cast<int32_t>(w2 * 0xd0u) / 64;
    const int32_t y0 = base_y + static_cast<int32_t>(h2 * 0xa0u) / 256;
    const int32_t x1 = x0 + static_cast<int32_t>(w2 * 0x260u) / 64;
    const int32_t y1 = y0 + static_cast<int32_t>(h2 * 0xea0u) / 256;
    gs_flat_rect(x0, y0, x1, y1, 0x80000000ull, 1);

    const uint32_t h3 = lwu(OV(0x0015f764u));
    const uint32_t cur_ammo = lwu(p0 + 0x74);
    const int32_t denom = static_cast<int32_t>(lwu(p0 + 0x8) << 8);
    const int32_t bar_bot =
        static_cast<int32_t>((lwu(py) + static_cast<uint32_t>(lh(p0 + 0x4a))) << 4) +
        static_cast<int32_t>(h3 * 0xf40u) / 256;
    const int32_t bar_h = ee_div(static_cast<int32_t>(cur_ammo * (h3 * 0xea0u)), denom);
    gs_flat_rect(x0, bar_bot, x1, bar_bot - bar_h, 0x80829e00ull, 1);

    for (int32_t tick = 1; tick < lw(p0 + 0x74); ++tick) {
        const uint32_t ht = lwu(OV(0x0015f764u));
        const int32_t tick_denom = static_cast<int32_t>(lwu(p0 + 0x8) << 8);
        const int32_t tick_bot =
            static_cast<int32_t>((lwu(py) + static_cast<uint32_t>(lh(p0 + 0x4a))) << 4) +
            static_cast<int32_t>(ht * 0xf40u) / 256;
        const int32_t tick_ofs = ee_div(static_cast<int32_t>(static_cast<uint32_t>(tick) * (ht * 0xea0u)), tick_denom);
        const int32_t ty = tick_bot - tick_ofs - 0x10;
        gs_flat_rect(x0, ty, x1, ty + 0x20, 0x80000000ull, 1);
    }

    const uint32_t tex = static_cast<uint32_t>(hud_frame(lwu(p0 + 0x0), 0));
    const uint32_t fx = lwu(px) + static_cast<uint32_t>(lh(p0 + 0x48));
    const uint32_t fy = lwu(py) + static_cast<uint32_t>(lh(p0 + 0x4a));
    suck_camera_23bac0(tex, fx, fy, lwu(OV(0x0015f760u)), lwu(OV(0x0015f764u)), 0x80u);

    ee_stack_free(0x10);
}

// orig: (none)
void suck_cannon_fns_register() {
    ovl_register_moby_fn(0x002da790, suck_cannon_update);
    ovl_register_moby_fn(0x002dd8e8, suck_beam_draw);
    ovl_register_moby_fn(0x00237798, suck_camera_237798);
    ovl_register_moby_fn(0x002377b8, suck_camera_2377b8);
    ovl_register_moby_fn(0x00237840, suck_camera_237840);
}

}  // namespace rac1
