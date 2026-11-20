// Oltanis (level 14), class 8: the searchlight drones of Gorda City.
// A drone flies along one path (pvar +0x74) while the spot of its light sweeps another (+0x78). The hero, or a moby of
// class 0xcb / 0x10e, caught in the beam wakes the drone's group of pop-up turrets (+0x70, class 30); the drone then
// keeps its light on him, moving along a third path (+0x204), until the turrets have gone down again, and returns to
// its patrol. Shot down, it spins away trailing smoke and explodes. A drone placed with +0x1b6 = 1 is the master of the
// beams: it only pulses the beams' alpha table and queues their draw (state 8).
// Also here: the turret group's helpers (0x2b44a8, 0x2b4500, 0x2b4670: in class 30's code, called only by the drones).
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

#include <cstdint>

namespace rac1 {

namespace {

void stop_drone_sound(uint32_t m, uint32_t pv) {
    const int32_t ch = lw(pv + 0x1d0);
    if (ch != -1) {
        const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
        if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) {
            sub_0028d918(ch);
        }
    }
    sw(pv + 0x1d0, static_cast<uint32_t>(-1));
}

// orig: L14:0x002ad8b8 -- hit handling and damage flash for class 8
void class8_hit_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    int32_t st = 0;
    float dmg = 0.0f;
    uint32_t react = static_cast<uint32_t>(moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4));
    if (st != 1 && lbu(m + 0x20) < 7) {
        const float hp = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, hp);
        if (hp <= 0.0f) {
            react = 1;
        }
        if (lbu(m + 0x20) == 1) {
            sb(m + 0x20, 2);
        }
        if (st == 3) {
            sb(m + 0xbc, 1);
        }
        // jump table L14:0x001fbb10 (12 entries)
        switch (react) {
        case 0:
        case 11:  // (0x002ad9e0)
            break;
        case 1:
        case 2:  // (0x002ad9bc)
            sb(m + 0x20, 7);
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xefffu));
            moby_drop_bolts(m, 0, -1);
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:  // (0x002ad998)
            sb(pv + 0x67, 0xc8);
            moby_anim_set(m, 1, 0, 5);
            break;
        case 9:
        case 10:  // (0x002ad990)
            sb(pv + 0x67, 0x64);
            moby_anim_set(m, 1, 0, 5);
            break;
        default:
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }

    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// orig: L14:0x002ae448 -- compute segment lengths on the drone's three paths and initial progress
void class8_paths_init(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    auto measure_path = [](uint32_t path, uint32_t total_out) {
        if (total_out != 0) sw(total_out, 0);
        const int32_t n = lw(path);
        for (int32_t i = 0; i < n; ++i) {
            const int32_t next = ee_rem(i + 1, n);
            const uint32_t p0 = path + 0x10u + (static_cast<uint32_t>(i) << 4);
            const uint32_t p1 = path + 0x10u + (static_cast<uint32_t>(next) << 4);
            const float d = vec3_distance(V(p0), V(p1));
            swc1(path + 0x1cu + (static_cast<uint32_t>(i) << 4), d);
            if (total_out != 0 && i != lw(path) - 1) {
                swc1(total_out, fpu_add(lwc1(total_out), d));
            }
        }
    };

    measure_path(lvd::path(lw(pv + 0x78)), pv + 0x16c);
    measure_path(lvd::path(lw(pv + 0x74)), pv + 0x190);
    measure_path(lvd::path(lw(pv + 0x204)), 0);

    const float start_frac = lwc1(pv + 0x210);
    if (start_frac == 0.0f) {
        swc1(pv + 0x1cc, 0.0f);
        sw(pv + 0x80, 0);
        sw(pv + 0x84, 0);
        swc1(pv + 0x8c, 0.0f);
        return;
    }

    const uint32_t spot_path = lvd::path(lw(pv + 0x78));
    sw(pv + 0x84, 0);
    swc1(pv + 0x1cc, 0.0f);
    {
        int32_t seg = 0;
        float dist = 0.0f;
        path_advance_v(spot_path, V(m + 0x10), &seg, &dist, lh(pv + 0x8a), fpu_mul(start_frac, lwc1(pv + 0x16c)));
        sw(pv + 0x84, static_cast<uint32_t>(seg));
        swc1(pv + 0x1cc, dist);
    }
    swc1(pv + 0x8c, 0.0f);
    sw(pv + 0x80, 0);
    {
        int32_t seg = 0;
        float dist = 0.0f;
        path_advance_v(spot_path, V(m + 0x10), &seg, &dist, lh(pv + 0x8a),
                       fpu_mul(lwc1(pv + 0x210), lwc1(pv + 0x190)));
        sw(pv + 0x80, static_cast<uint32_t>(seg));
        swc1(pv + 0x8c, dist);
    }
}

// orig: L14:0x002ae6d8 -- advance spotlight target along its patrol path
void class8_spot_advance(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 0x78));
    const int32_t prev_seg = lw(pv + 0x84);
    const float prev_dist = lwc1(pv + 0x1cc);
    int32_t seg = prev_seg;
    float dist = prev_dist;
    Vec4 target{};
    const int32_t wrapped = path_advance_v(path, target, &seg, &dist, lh(pv + 0x8a), lwc1(pv + 0x168));
    sw(pv + 0x84, static_cast<uint32_t>(seg));
    swc1(pv + 0x1cc, dist);

    const float k = lwc1(L14_ADDR(0x001613a0));
    const float damp = lwc1(L14_ADDR(0x001613a4));
    spring_to(pv + 0x170, pv + 0x180, target.x, k, damp, 0.0f);
    spring_to(pv + 0x174, pv + 0x184, target.y, k, damp, 0.0f);
    spring_to(pv + 0x178, pv + 0x188, target.z, k, damp, 0.0f);

    if (lh(pv + 0x8a) == 0 && wrapped != 0) {
        swc1(pv + 0x1cc, prev_dist);
        sw(pv + 0x84, static_cast<uint32_t>(prev_seg));
        swc1(pv + 0x168, -lwc1(pv + 0x168));
    }
}

// orig: L14:0x002ae7d8 -- spring drone body to match spotlight progress along the body path
void class8_body_follow_spot(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t spot_path = lvd::path(lw(pv + 0x78));
    const int32_t seg_count = lw(pv + 0x84);
    float acc = 0.0f;
    for (int32_t i = 0; i < seg_count; ++i) {
        acc = fpu_add(acc, lwc1(spot_path + 0x1cu + (static_cast<uint32_t>(i) << 4)));
    }
    const float frac = fpu_div(fpu_add(acc, lwc1(pv + 0x1cc)), lwc1(pv + 0x16c));
    const uint32_t body_path = lvd::path(lw(pv + 0x74));
    int32_t seg = 0;
    float dist = 0.0f;
    Vec4 target{};
    path_advance_v(body_path, target, &seg, &dist, lh(pv + 0x8a), fpu_mul(frac, lwc1(pv + 0x190)));

    const float k = lwc1(L14_ADDR(0x001613a0));
    const float damp = lwc1(L14_ADDR(0x001613a4));
    spring_to(m + 0x10, pv + 0x194, target.x, k, damp, 0.0f);
    spring_to(m + 0x14, pv + 0x198, target.y, k, damp, 0.0f);
    spring_to(m + 0x18, pv + 0x19c, target.z, k, damp, 0.0f);
}

// orig: L14:0x002ae900 -- spring drone along its alert/return path
void class8_alert_path_step(uint32_t m, float spring_k, float speed, float dir) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 0x204));
    Vec4 target{};
    if (lh(pv + 0x208) != 0) {
        target = V(path + (static_cast<uint32_t>(lw(path)) << 4));
    } else {
        int32_t seg = lw(pv + 0x80);
        float dist = lwc1(pv + 0x8c);
        const float step = fpu_mul(fpu_mul(speed, lwc1(0x0015ed6cu)), dir);
        const int32_t done = path_advance_v(path, target, &seg, &dist, 0, step);
        sw(pv + 0x80, static_cast<uint32_t>(seg));
        swc1(pv + 0x8c, dist);
        if (done != 0) {
            sh(pv + 0x208, 1);
        }
    }
    const float damp = lwc1(L14_ADDR(0x001613fc));
    spring_to(m + 0x10, pv + 0x194, target.x, spring_k, damp, 0.0f);
    spring_to(m + 0x14, pv + 0x198, target.y, spring_k, damp, 0.0f);
    spring_to(m + 0x18, pv + 0x19c, target.z, spring_k, damp, 0.0f);
}

// orig: L14:0x002aea08 -- hover bob and idle yaw sway
void class8_hover_sway(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sub_00262b00(m, pv + 0x1a0, pv + 0x1a4, lwc1(L14_ADDR(0x001613a8)), lwc1(L14_ADDR(0x001613ac)));
    sub_00262b80(m, pv + 0x1a8, pv + 0x1ac, lwc1(L14_ADDR(0x001613b0)), lwc1(L14_ADDR(0x001613b4)),
                 lwc1(L14_ADDR(0x001613b8)));
    if (lbu(m + 0x20) == 1) {
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1b4))) != 0) {
            sh(pv + 0x1b4, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x001613bc)))));
            float delta = lwc1(L14_ADDR(0x001613c0));
            if (rand_mod(2) != 0) delta = -delta;
            swc1(pv + 0x1b0, angle_add(lwc1(m + 0x48), delta));
        }
        spring_to(m + 0x48, pv + 0x1b8, lwc1(pv + 0x1b0), lwc1(L14_ADDR(0x001613c4)),
                  lwc1(L14_ADDR(0x001613c8)), lwc1(L14_ADDR(0x001613cc)));
    }
}

// orig: L14:0x002aead8 -- occasional joint matrix refresh while crashing
void class8_crash_pose_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (rand_mod(5) == 0) return;

    const uint32_t pt_addr = ee_stack_alloc(0x10);
    V(pt_addr) = V(m + 0x10);
    swc1(pt_addr + 0xc, lwc1(L14_ADDR(0x0016139c)));
    const int32_t vis = point_on_screen(pt_addr, int_to_float(lh(m + 0x32)));
    ee_stack_free(0x10);
    if (vis < 0) return;

    moby_chain_world_one(m, 1, pv + 0xa0);
    vec3_set_length(V(pv + 0xa0), V(pv + 0xa0), 1.0f);
    vec3_set_length(V(pv + 0xb0), V(pv + 0xb0), 1.0f);
    vec3_set_length(V(pv + 0xc0), V(pv + 0xc0), 1.0f);
    sw(pv + 0x20c, 2);
}

// orig: L14:0x002aeba0 -- orient searchlight turret joints toward the spot and optionally queue beam draw
int32_t class8_aim_spotlight(uint32_t m, int32_t queue_draw) {
    const uint32_t pv = lwu(m + 0x78);
    Vec4 half{};
    vec3_sub(half, V(pv + 0x170), V(pv + 0xd0));
    vec3_scale(half, half, 0.5f);

    const uint32_t mid_addr = ee_stack_alloc(0x10);
    vec3_add(V(mid_addr), V(pv + 0xd0), half);
    swc1(mid_addr + 0xc, fpu_add(vec3_length(half), 2.0f));
    const int32_t vis = point_on_screen(mid_addr, int_to_float(lh(m + 0x32)));
    ee_stack_free(0x10);
    if (vis < 0) return 0;

    moby_chain_world_one(m, 1, pv + 0xa0);
    vec3_set_length(V(pv + 0xa0), V(pv + 0xa0), 1.0f);
    vec3_set_length(V(pv + 0xb0), V(pv + 0xb0), 1.0f);
    vec3_set_length(V(pv + 0xc0), V(pv + 0xc0), 1.0f);

    const float yaw = angle_sub(atan2_f(half.x, half.y), lwc1(m + 0x48));
    quat_from_axis_angle(V(pv + 0xf0), 2, yaw);
    swc1(pv + 0x1bc, yaw);

    const float pitch = -atan2_f(vec2_length(half), half.z);
    quat_from_axis_angle(V(pv + 0x130), 1, pitch);
    swc1(pv + 0x1c0, pitch);

    V(pv + 0x90) = V(pv + 0x170);
    if (queue_draw != 0) {
        draw_hook_add_de80_l14(L14_ADDR(0x002ae260), m);
        sw(pv + 0x20c, lwu(pv + 0x20c) | 2u);
    }
    return 1;
}

// orig: L14:0x002aed50 -- pulse alpha table for the master searchlight draw controller
void class8_pulse_alpha_table(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float acc = fpu_add(lwc1(pv + 0x164), 0.5f);
    swc1(pv + 0x164, acc);
    const int32_t step = float_to_int(acc);
    if (1.0f <= lwc1(pv + 0x164)) {
        sw(pv + 0x164, 0);
    }
    const int32_t count = lw(L14_ADDR(0x00161418));
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t phase_addr = L14_ADDR(0x001d8210) + static_cast<uint32_t>(i) * 4u;
        int32_t v = lw(phase_addr);
        if (v != 0) {
            v += step;
            if (v >= 145) v -= 0x80;
            sw(phase_addr, static_cast<uint32_t>(v));
            const int32_t alpha = (v < 81) ? v : (0xa0 - v);
            const uint32_t col_addr = L14_ADDR(0x001d80c8) + static_cast<uint32_t>(i) * 4u;
            sw(col_addr, (lwu(col_addr) & 0x00ffffffu) | (static_cast<uint32_t>(alpha) << 24));
        }
    }
}

// orig: L14:0x002af048 -- count entries in this moby's own group
int32_t class8_own_group_count(uint32_t m) {
    uint32_t entry = lvd::group(lbu(m + 0x21));
    if (entry == 0) return 0;
    int32_t count = 0;
    for (;; entry += 2) {
        ++count;
        if (lh(entry) < 0) break;
    }
    return count;
}

// orig: L14:0x002b44a8 -- wake all dormant class 30 turrets in a group
void class30_group_wake(int32_t group_id) {
    uint32_t entry = lvd::group(group_id);
    if (entry == 0) return;
    const uint32_t pool = lwu(OV(0x0015ffd8));
    for (;; entry += 2) {
        const uint16_t raw = lhu(entry);
        const uint32_t tm = pool + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        if (lbu(tm + 0x20) == 1) {
            sh(lwu(tm + 0x78) + 0x88, 1);
        }
        if (lh(entry) < 0) break;
    }
}

// orig: L14:0x002b4500 -- if all turrets in a group are idle/retracting, trigger them to retract
int32_t class30_group_try_retract(int32_t group_id) {
    uint32_t entry = lvd::group(group_id);
    if (entry == 0) return 0;
    const uint32_t pool = lwu(OV(0x0015ffd8));
    for (;; entry += 2) {
        const uint16_t raw = lhu(entry);
        const uint32_t tm = pool + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        const uint32_t tpv = lwu(tm + 0x78);
        if (lbu(tpv + 0x8b) == 0) {
            const uint8_t st = lbu(tm + 0x20);
            if (st != 5 && st != 1) return 0;
        }
        if (lh(entry) < 0) break;
    }

    entry = lvd::group(group_id);
    for (;; entry += 2) {
        const uint16_t raw = lhu(entry);
        const uint32_t tm = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        if (lbu(tm + 0x20) == 3) {
            const uint32_t tpv = lwu(tm + 0x78);
            moby_set_anim(tm, 4, 0);
            sb(tm + 0x20, 5);
            const int16_t dur = static_cast<int16_t>(frame_scale_i(lw(L14_ADDR(0x0016150c))));
            sh(tpv + 0x74, static_cast<uint16_t>(dur));
            swc1(tpv + 0x78, fpu_div(1.0f, int_to_float(dur)));
            swc1(tm + 0x58, fpu_div(1.0f, fpu_mul(static_cast<float>(lh(tpv + 0x74)), lwc1(L14_ADDR(0x00161510)))));
            swc1(tpv + 0x8c, lwc1(tm + 0x48));
            sb(tpv + 0x8b, 0);
            sh(tpv + 0x88, 0);
        }
        if (lh(entry) < 0) break;
    }
    return 1;
}

// orig: L14:0x002b4670 -- true if any live turret in the group is already active, or none remain alive
int32_t class30_group_any_active_or_empty(int32_t group_id) {
    uint32_t entry = lvd::group(group_id);
    if (entry == 0) return 1;
    const uint32_t pool = lwu(OV(0x0015ffd8));
    int32_t alive = 0;
    for (;; entry += 2) {
        const uint16_t raw = lhu(entry);
        const uint32_t tm = pool + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        const int8_t st = static_cast<int8_t>(lbu(tm + 0x20));
        if (st >= 0) {
            ++alive;
            if (lh(lwu(tm + 0x78) + 0x88) != 0) return 1;
        }
        if (static_cast<int16_t>(raw) < 0) break;
    }
    return (alive < 1) ? 1 : 0;
}

}  // namespace

// orig: L14:0x002ac618
void olt_class8_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    if (lbu(pv + 0x2e) == 2) {
        sw(pv + 0x20c, 0);
        moby_retire(m);
        return;
    }

    if (lbu(m + 0x20) == 8) {
        class8_pulse_alpha_table(m);
        draw_callback_add(L14_ADDR(0x002aee28), m);
        return;
    }

    sw(pv + 0x20c, 0);
    if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == lbu(m + 0x53)) {
        uint8_t anim = lbu(m + 0x52);
        if (anim == 2) {
            moby_anim_set(m, 3, 0, 3);
            anim = lbu(m + 0x52);
        }
        if (anim == 1) {
            moby_anim_set(m, 2, 0, 10);
        }
    }

    class8_hit_update(m);

    if (static_cast<uint32_t>(lbu(m + 0x20) - 1u) < 6u) {
        if (vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 40.0f) {
            moby_floor_band(m);
        }
        sh(pv + 0x200, 0);
    }

    // jump table L14:0x001fbae0 (10 entries)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002ac774) init
        sb(static_cast<uint32_t>(moby_pvar_word0(m)) + 0xe, 1);
        if (lh(pv + 0x1b6) == 1) {
            sh(m + 0x34, static_cast<uint16_t>((lhu(m + 0x34) | 0x41u) & 0xefffu));
            sw(m + 0x94, 0);
            sb(m + 0x30, 0xff);
            sb(m + 0x20, 8);
            sb(pv + 0x58, static_cast<uint8_t>(float_to_int(17.0f)));
            sb(pv + 0x5a, static_cast<uint8_t>(float_to_int(6.0f)));
            sh(pv + 0x20a, static_cast<uint16_t>(class8_own_group_count(m)));
            return;
        }
        const int32_t p_body = lw(pv + 0x74);
        if (p_body == -1 || lw(lvd::path(p_body)) == 0) { moby_retire(m); return; }
        const int32_t p_spot = lw(pv + 0x78);
        if (p_spot == -1 || lw(lvd::path(p_spot)) == 0) { moby_retire(m); return; }
        if (lw(pv + 0x70) < 0) { moby_retire(m); return; }
        const int32_t p_alert = lw(pv + 0x204);
        if (p_alert == -1 || lw(lvd::path(p_alert)) == 0) { moby_retire(m); return; }

        sb(m + 0x7f, 0x20);
        swc1(pv + 0x20, 4.0f);
        sh(pv + 0x24, 4);
        sb(pv + 0x28, 3);
        V(m + 0x10) = V(lvd::path(lw(pv + 0x74)) + 0x10u);
        V(pv + 0x170) = V(lvd::path(lw(pv + 0x78)) + 0x10u);
        class8_paths_init(m);

        sh(pv + 0x88, 1);
        swc1(pv + 0x21c, -1.0f);
        swc1(pv + 0x188, 0.0f);
        swc1(pv + 0x184, 0.0f);
        swc1(pv + 0x180, 0.0f);
        swc1(pv + 0x19c, 0.0f);
        swc1(pv + 0x198, 0.0f);
        swc1(pv + 0x194, 0.0f);
        swc1(pv + 0x160, 0.0f);
        sw(pv + 0x20c, 0);
        swc1(pv + 0x164, 0.0f);
        swc1(pv + 0x1a0, 0.0f);
        swc1(pv + 0x1a4, 0.0f);
        swc1(pv + 0x1a8, 0.0f);
        swc1(pv + 0x1ac, 0.0f);
        sh(pv + 0x1b4, 0);
        swc1(pv + 0x1b8, 0.0f);
        swc1(pv + 0x1b0, lwc1(m + 0x48));
        mem_fill32(pv + 0xe0, 0, 0x40);
        mem_fill32(pv + 0x120, 0, 0x40);
        sub_0024f590(m, 0, pv + 0xe0);
        sub_0024f590(m, 1, pv + 0x120);
        swc1(pv + 0x1c8, 0.0f);
        swc1(pv + 0x1c4, 0.0f);
        sw(pv + 0x18c, 0);
        sb(m + 0x20, 1);
        sb(m + 0xbc, 0);
        break;
    }
    case 1: {  // (0x002ac980) patrol and search
        const uint32_t col = 0xff000000u | (lwu(L14_ADDR(0x00161408)) << 16) |
                             (lwu(L14_ADDR(0x00161404)) << 8) | lwu(L14_ADDR(0x00161400));
        sw(m + 0x90, col);
        if (class30_group_any_active_or_empty(lw(pv + 0x70)) != 0) {
            sb(m + 0x20, 3);
            sh(pv + 0x1f2, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x001613d0)))));
        }
        sh(pv + 0x202, 0);
        sh(pv + 0x200, 1);
        bool spotted = false;
        const int32_t on_scr = class8_aim_spotlight(m, 1);
        class8_spot_advance(m);
        class8_body_follow_spot(m);
        class8_hover_sway(m);
        if (on_scr != 0) {
            const uint32_t buf = ee_stack_alloc(0x20);
            const uint32_t mid_addr = buf;
            const uint32_t spot_addr = buf + 0x10u;
            vec3_lerp(V(mid_addr), V(pv + 0x90), V(pv + 0xd0), 0.5f);
            V(spot_addr) = V(pv + 0x90);
            const float r_base = lwc1(pv + 0x214);
            sub_001f6c30(spot_addr, fpu_mul(r_base, u2f(0x3f4ccccdu)));  // 0.8f
            sub_001f6c30(mid_addr, fpu_mul(lwc1(pv + 0x214), u2f(0x3ef5c28fu)));  // 0.48f
            bool hit = (mobys_in_sphere(mid_addr, fpu_mul(lwc1(pv + 0x214), u2f(0x3ef5c28fu)), 0, 0, 0) != 0);
            if (!hit) {
                hit = (mobys_in_sphere(spot_addr, fpu_mul(lwc1(pv + 0x214), u2f(0x3f666666u)), 0, 0, 0) != 0);  // 0.9f
            }
            ee_stack_free(0x20);
            if (hit) {
                const uint32_t hm = coll_hit_moby();
                if (hm == lwu(0x001413d0u) ||
                    (hm != 0 && (lh(hm + 0xa6) == 0xcb || lh(hm + 0xa6) == 0x10e))) {
                    spotted = true;
                    sw(pv + 0x18c, coll_hit_moby());
                }
            }
        }
        if (!spotted) break;
        [[fallthrough]];
    }
    case 2: {  // (0x002acb0c) alarm triggered -> wake turret group
        stop_drone_sound(m, pv);
        class30_group_wake(lw(pv + 0x70));
        sb(m + 0x20, 3);
        sh(pv + 0x1f2, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x001613d0)))));
        break;
    }
    case 3:  // (0x002acb74) tracking spotted target (falls through into case 4)
    case 4: {  // (0x002acd00) alert position while turrets are active
        if (lbu(m + 0x20) == 3) {
            const uint32_t col = 0xff000000u | (lwu(L14_ADDR(0x00161414)) << 16) |
                                 (lwu(L14_ADDR(0x00161410)) << 8) | lwu(L14_ADDR(0x0016140c));
            sw(m + 0x90, col);
            const uint32_t tgt = lwu(pv + 0x18c);
            if (tgt == 0) {
                sw(pv + 0x80, 0);
                sw(pv + 0x8c, 0);
                sh(pv + 0x208, 0);
                sb(m + 0x20, 4);
            } else {
                sh(pv + 0x202, 1);
                sh(pv + 0x200, 1);
                class8_aim_spotlight(m, 1);
                Vec4 top = V(lwu(pv + 0x18c) + 0x10u);
                top.z = fpu_add(top.z, 0.5f);
                Vec4 bot = top;
                bot.z = fpu_sub(bot.z, 4.0f);
                if (segment_collide_v(top, bot, 0x12, 0, nullptr) != 0) {
                    top = coll_hit_point();
                } else {
                    top.z = fpu_sub(top.z, 0.5f);
                }
                const float k = u2f(0x3b449ba6u);     // 0.003f
                const float damp = u2f(0x3e4ccccdu);  // 0.2f
                spring_to(pv + 0x170, pv + 0x180, top.x, k, damp, 0.0f);
                spring_to(pv + 0x174, pv + 0x184, top.y, k, damp, 0.0f);
                spring_to(pv + 0x178, pv + 0x188, top.z, k, damp, 0.0f);
                class8_body_follow_spot(m);

                const bool tgt_dead = (static_cast<int8_t>(lbu(lwu(pv + 0x18c) + 0x20)) < 0);
                if (tgt_dead || timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1f2))) != 0) {
                    swc1(pv + 0x8c, 0.0f);
                    sw(pv + 0x80, 0);
                    sh(pv + 0x208, 0);
                    sb(m + 0x20, 4);
                }
            }
        }

        // shared tail of state 3 and state 4 (0x002acd04)
        const uint32_t col = 0xff000000u | (lwu(L14_ADDR(0x00161414)) << 16) |
                             (lwu(L14_ADDR(0x00161410)) << 8) | lwu(L14_ADDR(0x0016140c));
        sw(m + 0x90, col);
        const uint32_t pt_addr = ee_stack_alloc(0x10);
        V(pt_addr) = V(m + 0x10);
        swc1(pt_addr + 0xc, lwc1(L14_ADDR(0x0016139c)));
        const int32_t vis = point_on_screen(pt_addr, int_to_float(lh(m + 0x32)));
        ee_stack_free(0x10);
        if (vis >= 0) {
            Vec4 joint{};
            moby_chain_point_v(m, 2, joint);
            swc1(pv + 0x1d4, joint.x);
            swc1(pv + 0x1d8, joint.y);
            swc1(pv + 0x1dc, joint.z);
            sw(pv + 0x20c, lwu(pv + 0x20c) | 8u);
            class8_hover_sway(m);
        }

        if (lbu(m + 0x20) != 4) break;

        if (lbu(m + 0x53) == 0) {
            moby_anim_set(m, 2, 0, 5);
        }
        class8_alert_path_step(m, lwc1(L14_ADDR(0x001613f8)), lwc1(L14_ADDR(0x001613f4)), 1.0f);

        if (lwc1(pv + 0x1c0) != 0.0f || lwc1(pv + 0x1bc) != 0.0f) {
            const float k = u2f(0x3b83126fu);     // 0.004f
            const float damp = u2f(0x3e4ccccdu);  // 0.2f
            const float eps = u2f(0x38d1b717u);   // 1e-4f
            spring_to(pv + 0x1c0, pv + 0x1c8, 0.0f, k, damp, 0.0f);
            spring_to(pv + 0x1bc, pv + 0x1c4, 0.0f, k, damp, 0.0f);
            if (abs_f(lwc1(pv + 0x1c0)) < eps) swc1(pv + 0x1c0, 0.0f);
            if (abs_f(lwc1(pv + 0x1bc)) < eps) swc1(pv + 0x1bc, 0.0f);
            quat_from_axis_angle(V(pv + 0xf0), 2, lwc1(pv + 0x1bc));
            quat_from_axis_angle(V(pv + 0x130), 1, lwc1(pv + 0x1c0));
        }

        if (class30_group_try_retract(lw(pv + 0x70)) != 0) {
            const uint32_t path = lvd::path(lw(pv + 0x204));
            sb(m + 0x20, 5);
            const int32_t last_seg = lw(path) - 2;
            sw(pv + 0x80, static_cast<uint32_t>(last_seg));
            sh(pv + 0x208, 0);
            swc1(pv + 0x8c, lwc1(path + 0x1cu + (static_cast<uint32_t>(last_seg) << 4)));
            class8_alert_path_step(m, u2f(0x3c23d70au), 5.0f, -1.0f);  // 0.01f
            moby_anim_set(m, 0, 0, frame_scale_i(10));
        }
        break;
    }
    case 5: {  // (0x002acf4c) returning along alert path
        class8_hover_sway(m);
        class8_alert_path_step(m, u2f(0x3c23d70au), 5.0f, -1.0f);  // 0.01f
        if (lh(pv + 0x208) != 0) {
            const uint32_t spot_path = lvd::path(lw(pv + 0x78));
            int32_t seg = 0;
            float dist = 0.0f;
            Vec4 closest{};
            path_closest_point_v(spot_path, V(m + 0x10), closest, &seg, &dist, 0, 40.0f, 5.0f, 0.0f);
            sw(pv + 0x84, static_cast<uint32_t>(seg));
            sh(pv + 0x200, 0);
            swc1(pv + 0x1cc, dist);
            sb(m + 0x20, 6);
        }
        break;
    }
    case 6: {  // (0x002acff0) rejoining patrol path
        class8_hover_sway(m);
        const uint32_t spot_path = lvd::path(lw(pv + 0x78));
        const int32_t seg_count = lw(pv + 0x84);
        float acc = 0.0f;
        for (int32_t i = 0; i < seg_count; ++i) {
            acc = fpu_add(acc, lwc1(spot_path + 0x1cu + (static_cast<uint32_t>(i) << 4)));
        }
        const float frac = fpu_div(fpu_add(acc, lwc1(pv + 0x1cc)), lwc1(pv + 0x16c));
        const uint32_t body_path = lvd::path(lw(pv + 0x74));
        int32_t tmp_seg = 0;
        float tmp_dist = 0.0f;
        Vec4 body_pt{};
        path_advance_v(body_path, body_pt, &tmp_seg, &tmp_dist, lh(pv + 0x8a), fpu_mul(frac, lwc1(pv + 0x190)));

        const float k_body = u2f(0x3ba3d70au);  // 0.005f
        const float damp = lwc1(L14_ADDR(0x001613a4));
        spring_to(m + 0x10, pv + 0x194, body_pt.x, k_body, damp, 0.0f);
        spring_to(m + 0x14, pv + 0x198, body_pt.y, k_body, damp, 0.0f);
        spring_to(m + 0x18, pv + 0x19c, body_pt.z, k_body, damp, 0.0f);
        class8_aim_spotlight(m, 0);
        if (vec3_distance(V(m + 0x10), body_pt) < 0.5f) {
            sb(m + 0x20, 1);
        }

        int32_t s_seg = lw(pv + 0x84);
        float s_dist = lwc1(pv + 0x1cc);
        Vec4 spot_pt{};
        path_advance_v(lvd::path(lw(pv + 0x78)), spot_pt, &s_seg, &s_dist, lh(pv + 0x8a), 0.0f);
        sw(pv + 0x84, static_cast<uint32_t>(s_seg));
        swc1(pv + 0x1cc, s_dist);
        const float k_spot = lwc1(L14_ADDR(0x001613a0));
        spring_to(pv + 0x170, pv + 0x180, spot_pt.x, k_spot, damp, 0.0f);
        spring_to(pv + 0x174, pv + 0x184, spot_pt.y, k_spot, damp, 0.0f);
        spring_to(pv + 0x178, pv + 0x188, spot_pt.z, k_spot, damp, 0.0f);
        break;
    }
    case 7: {  // (0x002ad208) shot down -> start crash spin
        sh(pv + 0x1f0, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x001613d4)))));
        class30_group_wake(lw(pv + 0x70));
        stop_drone_sound(m, pv);
        explosion_effect(m, pv + 0x40, m + 0x10, 5, 2, 4, 1, 1, 5, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);

        Vec4 away{};
        vec3_sub(away, V(m + 0x10), V(0x0013f3d0u));
        away.z = 0.0f;
        const float fscale = lwc1(0x0015ed60u);
        const float spd = rand_range_f(fpu_mul(lwc1(L14_ADDR(0x001613d8)), fscale),
                                       fpu_mul(lwc1(L14_ADDR(0x001613dc)), fscale));
        vec3_set_length(away, away, spd);
        Vec4 cross_unused{};
        vec3_cross_ba(cross_unused, V(0x0013f5e0u), away);
        const float deflect = rand_range_signed_f(u2f(0x3f32b8c2u), u2f(0x3f9c61aau));
        vec_rotate_axis(V(pv + 0x1e0), away, V(0x0013f5e0u), deflect);
        const float fscale2 = lwc1(0x0015ed60u);
        swc1(pv + 0x1e8, rand_range_f(fpu_mul(lwc1(L14_ADDR(0x001613e0)), fscale2),
                                      fpu_mul(lwc1(L14_ADDR(0x001613e4)), fscale2)));

        swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), u2f(0x3dcccccd)));  // 0.1f
        swc1(pv + 0x148, 10.0f);
        swc1(pv + 0x140, 10.0f);
        swc1(pv + 0x144, 10.0f);
        const float spin_max = u2f(0x3db2b8c2u);  // 5 deg
        const float tilt_max = u2f(0x3f490fdbu);  // 45 deg
        swc1(pv + 0x1f4, rand_range_signed_f(0.0f, spin_max));
        swc1(pv + 0x1f8, rand_range_signed_f(0.0f, spin_max));
        swc1(pv + 0x1fc, rand_range_signed_f(0.0f, spin_max));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), rand_range_signed_f(0.0f, tilt_max)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), rand_range_signed_f(0.0f, tilt_max)));
        sb(m + 0x20, 9);
        class8_crash_pose_update(m);
        return;
    }
    case 9: {  // (0x002ad438) crashing with smoke trail
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x1e0));
        auto damp_axis = [&](uint32_t vel_addr) {
            const float fscale = lwc1(0x0015ed60u);
            if (fpu_mul(lwc1(L14_ADDR(0x001613f0)), fscale) < abs_f(lwc1(vel_addr))) {
                const float factor = fpu_add(fpu_mul(fpu_sub(lwc1(L14_ADDR(0x001613ec)), 1.0f), fscale), 1.0f);
                swc1(vel_addr, fpu_mul(lwc1(vel_addr), factor));
            }
        };
        damp_axis(pv + 0x1e0);
        damp_axis(pv + 0x1e4);
        if (0.0f < lwc1(pv + 0x1e8)) {
            const float factor = fpu_add(fpu_mul(fpu_sub(lwc1(L14_ADDR(0x001613ec)), 1.0f), lwc1(0x0015ed60u)), 1.0f);
            swc1(pv + 0x1e8, fpu_mul(lwc1(pv + 0x1e8), factor));
        }
        swc1(pv + 0x1e8, fpu_sub(lwc1(pv + 0x1e8), fpu_mul(lwc1(L14_ADDR(0x001613e8)), lwc1(0x0015ed64u))));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x1fc)));
        swc1(pv + 0x148, 10.0f);
        swc1(pv + 0x140, 10.0f);
        swc1(pv + 0x144, 10.0f);

        const float px = lwc1(m + 0x10);
        const float py = lwc1(m + 0x14);
        const float pz = lwc1(m + 0x18);
        if (px < 2.0f || 1021.0f < px || py < 2.0f || 1021.0f < py || pz < 2.0f || 1021.0f < pz) {
            moby_retire(m);
            return;
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x1f0))) != 0 ||
            sphere_collide(m + 0x10, 0.5f, 0, m) != 0) {
            explosion_effect(m, pv + 0x40, m + 0x10, 5, 2, 4, 1, 1, 5, -1, 0, 2.0f, 1.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
            moby_retire(m);
            return;
        }

        const int32_t dmin = frame_scale_i(10);
        const int32_t dmax = frame_scale_i(20);
        const int32_t dur = rand_range(dmin, dmax);
        spawn_particle_15_v(V(m + 0x10), V(OV(0x0015f580)), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(dur), 1, 60000.0f);
        for (int k = 0; k < 2; ++k) {
            int32_t spin = rand_mod(6);
            if (rand_mod(2) != 0) spin = -spin;
            const float sz = rand_range_f(40000.0f, 100000.0f);
            const uint32_t c = static_cast<uint32_t>(rand_range(0x30, 0xff));
            const uint32_t col = 0x60000000u | (c << 16) | (c << 8) | c;
            const uint32_t p = spawn_particle_17(m + 0x10, spin, OV(0x0015f580), col, u2f(0x3ccccccdu), 1.0f, 1.0f, sz);
            if (p != 0) {
                sb(p + 3, 0x44);
                sh(p + 0xa, static_cast<uint16_t>(frame_scale_i(60)));
                sw(p + 0x24, 2);
                sb(p + 0x2a, 0x60);
                sb(p + 0x2b, lbu(p + 0xa));
            }
        }
        class8_crash_pose_update(m);
        return;
    }
    case 8:  // (0x002ad7c0) handled at entry
    default:
        break;
    }

    // common tail: hit sparks and engine/alarm loop sound (0x002ad7c0)
    if (static_cast<int8_t>(lbu(m + 0x20)) >= 0 && lbu(m + 0x53) == 1) {
        if (lbu(m + 0xbc) == 1) {
            moby_hit_sparks(m, V(OV(0x0015f580)), 0xffffu);
        }
    } else {
        sb(m + 0xbc, 0);
    }

    const uint8_t st_after = lbu(m + 0x20);
    if (st_after == 1 && sub_0028d8c0(m, lw(pv + 0x1d0)) == 0) {
        sw(pv + 0x1d0, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }
    if (static_cast<uint32_t>(lbu(m + 0x20) - 3u) < 2u && sub_0028d8c0(m, lw(pv + 0x1d0)) == 0) {
        sw(pv + 0x1d0, static_cast<uint32_t>(moby_sound_start(2, 4, m)));
    }
}

}  // namespace rac1
