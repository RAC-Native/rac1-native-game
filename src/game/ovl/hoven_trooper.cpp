// Hoven's (level 12) class 294: the armed Blarg trooper (with the gun moby 0x1f3 carried at joint 1, ambush path
// `pv + 0x254`, boundary path `pv + 0x26c`, and its 9 helpers 0x27b8e0 / 0x2e7290 / 0x2e86d0 / 0x2e81e0 / 0x2e8608 /
// 0x2e8720 / 0x2e87b0 / 0x271dd8 / 0x2e8808). Level 12 only: `orig: L12:`, level data through lvd::* and collision list
// through coll_hit_list_at, Hoven-only data L12_ADDR, base ELF bare. Registered in hoven_mobys.cpp.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/walker.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
}  // namespace

// Tests whether point `pt` lies within `max_dz` vertically, `range` in 3D, and (when `half_cone > 0`) `half_cone` in yaw
// of moby `m`.
// orig: L12:0x0027b8e0
uint32_t trooper_in_cone(uint32_t m, uint32_t pt, float range, float max_dz, float half_cone) {
    if (max_dz < abs_f(fpu_sub(lwc1(pt + 8), lwc1(m + 0x18)))) return 0;
    if (range < vec3_distance(V(pt), V(m + 0x10))) return 0;
    if (!(0.0f < half_cone)) return 1;
    const float yaw = atan2_f(fpu_sub(lwc1(pt + 0), lwc1(m + 0x10)), fpu_sub(lwc1(pt + 4), lwc1(m + 0x14)));
    return half_cone < angle_dist(lwc1(m + 0x48), yaw) ? 0u : 1u;
}

// Death burst for class 294.
// orig: L12:0x002e7290
void trooper_death_burst(uint32_t m, uint32_t pos) {
    enemy_death_burst(m, pos, 1.0f, 10.0f);
}

// Drops bolts, retires the carried gun moby (`pv + 0x248`), and retires `m`.
// orig: L12:0x002e86d0
void trooper_retire(uint32_t m) {
    sh(m + 0xb4, 0);
    moby_drop_bolts(m, 0, -1);
    const uint32_t gun = lwu(lwu(m + 0x78) + 0x248);
    if (gun != 0) moby_retire(gun);
    moby_retire(m);
}

// Updates alert sight (`pv + 0x230`), group-overcrowd self-destruct (`pv + 0x274`), kill box (`pv + 0x290`), weapon hits
// (mask 0x330000 -> knockback state 4 or death state 9), and colour pulse (`pv + 0x60`).
// orig: L12:0x002e81e0
void trooper_hits(uint32_t m) {
    const Frame f(144);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    if (lwu(pv + 0x38) != 0) {
        sw(pv + 0x38, 0);
        sh(pv + 0x240, static_cast<uint32_t>(frame_scale_i(0xf0)));
    }
    if (lbu(m + 0x20) != 0) {
        const int32_t expired = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x240)));
        swc1(pv + 0x230, expired != 0 ? lwc1(pv + 0x28c) : 20.0f);
    }
    if (lwu(pv + 0x274) != 0 && lbu(m + 0x20) != 2) {
        sw(pv + 0x288, lwu(pv + 0x288) + 1u);
        const int32_t t60 = frame_scale_i(0x3c);
        if ((t60 << 4) + t60 < lw(pv + 0x288)) {
            int32_t idle_cnt = 0;
            if (lw(pv + 0x278) != -1) idle_cnt += moby_group_count_l1(lw(pv + 0x278), 2);
            if (lw(pv + 0x27c) != -1) idle_cnt += moby_group_count_l1(lw(pv + 0x27c), 2);
            if (lw(pv + 0x280) != -1) idle_cnt += moby_group_count_l1(lw(pv + 0x280), 2);
            if (lw(pv + 0x284) != -1) idle_cnt += moby_group_count_l1(lw(pv + 0x284), 2);
            if (!(idle_cnt < 9) && rand_mod(0x27) == 0) {
                const float back = angle_add(lwc1(m + 0x48), u2f(0x40490fdbu));
                swc1(sp + 0x0, cos_vu(back));
                const float back2 = angle_add(lwc1(m + 0x48), u2f(0x40490fdbu));
                swc1(sp + 0x4, sin_vu(back2));
                sw(sp + 0x8, 0);
                sub_00259bc8(m, lwu(0x001413d0), 0x00010000u, m + 0x10, sp, 1.0f);
            }
        }
    }
    const int32_t kill_box = lw(pv + 0x290);
    if (kill_box != -1 && point_in_box(0x0013f3d0, kill_box) != 0) {
        trooper_retire(m);
        return;
    }
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    swc1(sp + 0x14, 0.0f);
    int32_t st = lw(sp + 0x10);
    float dmg = lwc1(sp + 0x14);
    moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    sw(sp + 0x10, static_cast<uint32_t>(st));
    swc1(sp + 0x14, dmg);
    if (hit != 0) {
        const int16_t src_cls = lh(lwu(hit + 0x20) + 0xa6);
        if (src_cls != lh(m + 0xa6) && src_cls != 0xb8 && lbu(m + 0x20) != 9 && lwc1(sp + 0x14) != 0.0f) {
            if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
            const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(sp + 0x14));
            const float grav = fpu_mul(lwc1(0x0015ed70), 30.0f);
            sb(pv + 0xad, 0);
            swc1(pv + 0xa8, 1.0f);
            swc1(pv + 0x20, hp);
            swc1(pv + 0x80, grav);
            sw(pv + 0x94, 9);
            swc1(pv + 0xa0, 1.0f);
            swc1(pv + 0xa4, 1.0f);
            if (hp <= 0.0f) {
                const float dt = lwc1(0x0015ed6c);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                swc1(pv + 0x88, fpu_mul(dt, 12.0f));
                swc1(pv + 0x8c, fpu_mul(dt, 9.0f));
                lq_sq(sp + 0x0, hit + 0x10);
                vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x18)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
                move_state_start(m, pv + 0x70, 9, 1, 0, lwc1(sp + 0x18));
                swc1(pv + 0xc0, 10.0f);
                swc1(pv + 0xc4, 19.0f);
                sb(m + 0x20, 9);
                sb(pv + 0x67, 0x78);
                color_pulse_start(m, pv + 0x60);
            } else {
                const float dt = lwc1(0x0015ed6c);
                swc1(pv + 0x88, fpu_mul(dt, u2f(0x40966666u)));
                swc1(pv + 0x8c, fpu_mul(dt, u2f(0x40f66666u)));
                lq_sq(sp + 0x0, hit + 0x10);
                vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x1c)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
                move_state_start(m, pv + 0x70, 6, 1, 0, lwc1(sp + 0x1c));
                swc1(pv + 0xc0, 4.0f);
                swc1(pv + 0xc4, 8.0f);
                sb(m + 0x20, 4);
                sb(pv + 0x67, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pv + 0x60);
            }
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// Turns torso / arm spring targets (`pv + 0x188` and `pv + 0x208`) toward `target` (clamped to 70 degrees).
// orig: L12:0x002e8608
void trooper_aim_joints(uint32_t m, uint32_t target) {
    const uint32_t pv = lwu(m + 0x78);
    float turn =
        angle_sub(atan2_f(fpu_sub(lwc1(target + 0), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14))),
                  lwc1(m + 0x48));
    const float lim = u2f(0x3f9c61aau);
    if (lim < turn) turn = lim;
    else if (turn < -lim) turn = -lim;
    swc1(pv + 0x188, fpu_mul(turn, u2f(0x3eb33333u)));
    swc1(pv + 0x208, fpu_mul(turn, u2f(0x3f4ccccdu)));
}

// Spawns or carries the trooper's gun moby (class 0x1f3) at joint 1 (`pv + 0x248`).
// orig: L12:0x002e8720
void trooper_gun_update(uint32_t m) {
    const Frame f(64);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t gun = lwu(pv + 0x248);
    if (gun == 0) {
        moby_chain_point_v(m, 1, V(sp));
        sw(pv + 0x248, spawn_like(m, sp, 0x1f3u));
    } else {
        moby_carry_at_joint(m, gun, 1);
        const uint32_t g2 = lwu(pv + 0x248);
        if ((lhu(m + 0x34) & 1u) != 0) sh(g2 + 0x34, lhu(g2 + 0x34) | 0x41u);
        else sh(g2 + 0x34, lhu(g2 + 0x34) & 0xffbeu);
    }
}

// Settles `m + 0x18` onto the floor below `m`.
// orig: L12:0x002e87b0
void trooper_to_floor(uint32_t m) {
    const float floor_z = floor_probe_v(0.5f, V(m + 0x10), 0);
    approach_f(m + 0x18, floor_z, fpu_mul(lwc1(0x0015ed70), 27.0f));
}

// Steps walker record `rec` 100 units forward along `m + 0x48`.
// orig: L12:0x00271dd8
uint32_t trooper_walk_step(uint32_t m, uint32_t rec) {
    const Frame f(96);
    const uint32_t sp = f.sp;
    lq_sq(sp, m + 0x10);
    const float c = cos_vu(lwc1(m + 0x48));
    swc1(sp + 0, fpu_add(lwc1(sp + 0), fpu_mul(c, 100.0f)));
    const float s = sin_vu(lwc1(m + 0x48));
    swc1(sp + 4, fpu_add(lwc1(sp + 4), fpu_mul(s, 100.0f)));
    return static_cast<uint32_t>(walker_record_step(1.0f, m, rec, sp, sp + 0x10));
}

// Turns and walks `m` toward `target` (adding `yaw_offset` beyond 6.5 units unless stuck flag `pv + 0x242` is set),
// keeping inside boundary polygon path `pv + 0x26c`.
// orig: L12:0x002e8808
void trooper_walk_toward(uint32_t m, uint32_t target, float yaw_offset, float speed_override) {
    const Frame f(192);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    if (lbu(pv + 0x242) != 0) {
        const float yaw = atan2_f(fpu_sub(lwc1(target + 0), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
        const float dt2 = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        yaw_seek(m + 0x48, pv + 0x23c, yaw, fpu_mul(dt2, u2f(0x417b53d1u)), fpu_mul(dt2, u2f(0x41490fdbu)),
                 fpu_mul(dt, u2f(0x41c90fdbu)));
        return;
    }

    const float dist = vec2_distance(V(m + 0x10), V(target));
    float yaw = atan2_f(fpu_sub(lwc1(target + 0), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
    if (6.5f < dist && yaw_offset != 0.0f) yaw = angle_add(yaw, yaw_offset);
    {
        const float dt2 = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        yaw_seek(m + 0x48, pv + 0x23c, yaw, fpu_mul(dt2, u2f(0x417b53d1u)), fpu_mul(dt2, u2f(0x41490fdbu)),
                 fpu_mul(dt, u2f(0x41c90fdbu)));
    }
    if (speed_override != 0.0f) {
        approach_f(pv + 0xf4, speed_override, fpu_mul(lwc1(0x0015ed70), 7.0f));
    } else {
        const float to_yaw =
            atan2_f(fpu_sub(lwc1(target + 0), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
        if (angle_dist(lwc1(m + 0x48), to_yaw) < u2f(0x3fc90fdbu) && 6.0f < dist) {
            const float dt = lwc1(0x0015ed6c), dt2 = lwc1(0x0015ed70);
            approach_f(pv + 0xf4, fpu_mul(dt, 7.0f), fpu_mul(dt2, 7.0f));
        } else {
            const float dt2 = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
            approach_f(pv + 0xf4, dt, fpu_mul(dt2, 7.0f));
        }
    }

    lq_sq(sp + 0x0, m + 0x10);
    trooper_walk_step(m, pv + 0xd0);
    const int32_t bound_path = lw(pv + 0x26c);
    if (bound_path != -1) {
        vec3_sub(V(sp + 0x30), V(sp + 0x0), V(m + 0x10));
        vec3_set_length(V(sp + 0x30), V(sp + 0x30), u2f(0x3d4ccccdu));
        vec3_add(V(sp + 0x20), V(sp + 0x0), V(sp + 0x30));
        if (path_crossing(bound_path, m + 0x10, sp + 0x20, sp + 0x10) != 0) {
            const uint32_t p = lvd::path(lw(pv + 0x26c));
            if (point_in_polygon(m + 0x10, p + 0x10, lw(p)) == 0) lq_sq(m + 0x10, sp + 0x10);
            if (vec3_distance(V(m + 0x10), V(sp + 0x0)) < fpu_mul(lwc1(0x0015ed6c), 0.5f)) {
                const uint8_t stuck = static_cast<uint8_t>(lbu(pv + 0x243) + 1u);
                sb(pv + 0x243, stuck);
                if (frame_scale_i(0xf) < static_cast<int32_t>(stuck)) sb(pv + 0x242, 1);
            } else {
                sb(pv + 0x243, 0);
            }
        }
    }
}

// Class 294's update: states 0..9 (jump table L12:0x20c4d0: 0 set up, 1 walking ambush path, 2 waiting to spot target,
// 3 chasing and pushing crates, 4 knocked back, 5 raising gun, 6 firing shot, 7 between shots, 8 returning home, 9 dying).
// orig: L12:0x002e72c0
void hov_class294_update_l12(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    moby_set_70(pv + 0x120, 3.0f);
    if (lw(L12_ADDR(0x0015f5c4)) == 2) {
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        if (lwu(pv + 0x248) != 0) sh(lwu(pv + 0x248) + 0x34, lhu(lwu(pv + 0x248) + 0x34) | 0x41u);
        return;
    }
    if (lbu(m + 0x20) >= 3) {
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        if (lwu(pv + 0x248) != 0) sh(lwu(pv + 0x248) + 0x34, lhu(lwu(pv + 0x248) + 0x34) & 0xffbeu);
    }
    if (lbu(pv + 0x269) != 0 && lw(0x001413d4) == 0 && lw(0x001413e0) == 0x67 &&
        vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 30.0f) {
        trooper_retire(m);
        return;
    }
    const int32_t vis_box = lw(pv + 0x270);
    if (vis_box != -1) {
        if (point_in_box(L12_ADDR(0x00167240), vis_box) == 0) {
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            if (lwu(pv + 0x248) != 0) sh(lwu(pv + 0x248) + 0x34, lhu(lwu(pv + 0x248) + 0x34) | 0x41u);
            return;
        }
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        if (lwu(pv + 0x248) != 0) sh(lwu(pv + 0x248) + 0x34, lhu(lwu(pv + 0x248) + 0x34) & 0xffbeu);
    }
    if (lbu(pv + 0x2e) == 2) {
        trooper_retire(m);
        return;
    }

    trooper_hits(m);
    target_select_any(m, sp, lwc1(pv + 0x230));
    uint32_t target_m = lwu(sp + 0x40);
    if (target_m == 0) {
        target_m = lwu(0x001413d0);
        lq_sq(sp + 0x20, 0x0013f420);
    }

    switch (lbu(m + 0x20)) {  // jump table L12:0x0020c4d0 (10 entries)
    case 0: {                 // (0x002e7484)
        const float sight = lwc1(pv + 0x230);
        swc1(pv + 0x20, 2.0f);
        sh(pv + 0x24, 2);
        sb(pv + 0x5a, 8);
        swc1(pv + 0x28c, sight);
        sw(pv + 0x90, 0x200);
        swc1(pv + 0x98, 0.5f);
        sb(pv + 0x58, 8);
        sb(pv + 0x2e, 1);
        sb(pv + 0x29, 1);
        walker_record_init_l1(pv + 0xd0);
        const float spd = fpu_mul(lwc1(0x0015ed6c), 5.5f);
        sw(pv + 0xd0, 0x38d);
        swc1(pv + 0xd8, 1.0f);
        sw(pv + 0x108, 0x18);
        swc1(pv + 0xf4, spd);
        swc1(pv + 0xdc, 1.0f);
        lq_sq(pv + 0x220, m + 0x10);
        if (lbu(m + 0x21) != 0xff) {
            const uint32_t grp_flag = lwu(pv + 0x24c);
            if (lw(grp_flag) == 1) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
            const uint32_t g2 = lwu(pv + 0x24c);
            sw(g2, lwu(g2) < 1u ? 1u : 0u);
        } else if ((libc_rand() & 1) != 0) {
            sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        }
        if (lwu(pv + 0x274) != 0) sw(pv + 0x288, static_cast<uint32_t>(frame_scale_i(rand_range(0, 0x12c))));
        sw(m + 0x40, 0);
        sw(m + 0x44, 0);
        if (lwc1(pv + 0x244) == lwc1(m + 0x40)) {
            if (lbu(m + 0x21) != 0xff) {
                const uint32_t ang_ptr = lwu(pv + 0x250);
                swc1(pv + 0x244, static_cast<float>(lw(ang_ptr)));
                const int32_t cur = lw(ang_ptr);
                sw(ang_ptr, static_cast<uint32_t>(cur <= 0 ? (cur + 0x14) : -cur));
            } else {
                swc1(pv + 0x244, rand_range_signed_f(15.0f, 35.0f));
            }
        }
        swc1(pv + 0x244, fpu_mul(lwc1(pv + 0x244), u2f(0x3c8efa35u)));
        swc1(m + 0x18, floor_probe_v(0.5f, V(m + 0x10), 0));
        if (lbu(pv + 0x268) != 0) {
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            if (lwu(pv + 0x248) != 0) sh(lwu(pv + 0x248) + 0x34, lhu(lwu(pv + 0x248) + 0x34) | 0x41u);
        }
        sb(m + 0x20, 2);
        break;
    }
    case 1: {  // (0x002e7824)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x258))) != 0) {
            if (lbu(m + 0x53) != 8 && lbu(m + 0x52) == lbu(m + 0x53)) {
                moby_anim_set_flags(m, 8, 0, frame_scale_i(0xb), 5);
            }
            const uint32_t path = lvd::path(lw(pv + 0x254));
            if (vec2_distance(V(m + 0x10), V(path + 0x10 + (static_cast<uint32_t>(lw(pv + 0x260)) << 4))) < 3.0f) {
                sw(pv + 0x260, lwu(pv + 0x260) + 1u);
            }
            const int32_t last = lw(path) - 1;
            if (last < lw(pv + 0x260)) sw(pv + 0x260, static_cast<uint32_t>(last));
            trooper_walk_toward(m, path + 0x10 + (static_cast<uint32_t>(lw(pv + 0x260)) << 4), 0.0f,
                                fpu_mul(lwc1(0x0015ed6c), 8.0f));
            if (lw(pv + 0x260) == lw(path) - 1 ||
                vec2_distance(V(m + 0x10), V(target_m + 0x10)) < lwc1(pv + 0x264)) {
                sb(m + 0x20, 3);
            }
        }
        break;
    }
    case 2: {  // (0x002e7680)
        if (64.0f < vec3_distance(V(m + 0x10), V(0x0013f3d0))) return;
        if (lbu(m + 0x53) != 0 && lbu(m + 0x52) == lbu(m + 0x53)) {
            moby_anim_set_flags(m, 0, 0, frame_scale_i(8), 5);
        }
        const uint32_t tpos = target_m + 0x10;
        const bool spotted = trooper_in_cone(m, tpos, lwc1(pv + 0x230), 3.0f, 0.0f) != 0 ||
                             (lw(pv + 0x25c) != -1 && point_in_box(tpos, lw(pv + 0x25c)) != 0) ||
                             lbu(m + 0xbc) != 0;
        if (spotted) {
            if ((lhu(m + 0x34) & 1u) != 0) {
                lq_sq(sp + 0x50, m + 0x10);
                swc1(sp + 0x58, fpu_add(lwc1(sp + 0x58), 0.5f));
                if (segment_collide_v(V(sp + 0x50), V(L12_ADDR(0x00167240)), 2, m, nullptr) == 0) {
                    if (lwu(pv + 0x248) != 0) moby_retire(lwu(pv + 0x248));
                    moby_retire(m);
                    return;
                }
                sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
                if (lwu(pv + 0x248) != 0) sh(lwu(pv + 0x248) + 0x34, lhu(lwu(pv + 0x248) + 0x34) & 0xffbeu);
            }
            if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
            const int32_t path_idx = lw(pv + 0x254);
            if (path_idx == -1) {
                sb(m + 0x20, 3);
            } else {
                sw(pv + 0x260, static_cast<uint32_t>(path_nearest_l1(m + 0x10, lvd::path(path_idx), 0.0f)));
                sb(m + 0x20, 1);
            }
        }
        break;
    }
    case 3: {  // (0x002e7948)
        if (lbu(pv + 0x268) == 1) {
            lq_sq(sp + 0x50, m + 0x10);
            swc1(sp + 0x50, fpu_add(lwc1(sp + 0x50), cos_vu(lwc1(m + 0x48))));
            swc1(sp + 0x54, fpu_add(lwc1(sp + 0x54), sin_vu(lwc1(m + 0x48))));
            const int32_t n = mobys_in_sphere(sp + 0x50, 1.5f, 0, m, 0);
            if (n > 0) {
                lq_sq(sp + 0x60, L12_ADDR(0x0015f580));
                for (int32_t i = 0; i < n; ++i) {
                    const uint32_t hit_m = coll_hit_list_at(i);
                    if (moby_in_range_class_500(hit_m) != 0) {
                        const uint32_t hm2 = coll_hit_list_at(i);
                        if (abs_f(fpu_sub(lwc1(m + 0x18), lwc1(hm2 + 0x18))) < u2f(0x3f333333u)) {
                            sub_00259bc8(coll_hit_list_at(i), lwu(0x001413d0), 0x00010000u, m + 0x10, sp + 0x60, 1.0f);
                        }
                    }
                }
            }
        }
        float turn = angle_sub(
            atan2_f(fpu_sub(lwc1(target_m + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target_m + 0x14), lwc1(m + 0x14))),
            lwc1(m + 0x48));
        const float lim = u2f(0x3f75be0bu);
        if (lim < turn) turn = lim;
        else if (turn < -lim) turn = -lim;
        const float joint_turn = fpu_mul(turn, 0.5f);
        swc1(pv + 0x208, joint_turn);
        swc1(pv + 0x188, joint_turn);
        trooper_walk_toward(m, target_m + 0x10, lwc1(pv + 0x244), 0.0f);
        bool start_aim = false;
        if (vec2_distance(V(m + 0x10), V(target_m + 0x10)) < 5.5f || lbu(pv + 0x242) != 0) {
            const float to_t =
                atan2_f(fpu_sub(lwc1(target_m + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target_m + 0x14), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), to_t) < u2f(0x3db2b8c2u)) start_aim = true;
        }
        if (start_aim) {
            sb(m + 0x20, 5);
        } else if (lbu(m + 0x53) != 8 && lbu(m + 0x52) == lbu(m + 0x53)) {
            moby_anim_set_flags(m, 8, 0, frame_scale_i(0xb), 5);
        }
        break;
    }
    case 4: {  // (0x002e7fa4)
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        if ((enemy_physics(m, pv + 0x70) & 0x60u) != 0) {
            if (vec2_distance(V(m + 0x10), V(target_m + 0x10)) < 6.0f || lbu(pv + 0x242) != 0) {
                const float to_t = atan2_f(fpu_sub(lwc1(target_m + 0x10), lwc1(m + 0x10)),
                                           fpu_sub(lwc1(target_m + 0x14), lwc1(m + 0x14)));
                sb(m + 0x20, angle_dist(lwc1(m + 0x48), to_t) < u2f(0x3dfa35ddu) ? 5 : 3);
            } else {
                sb(m + 0x20, 3);
            }
        } else if (lwc1(m + 0x18) < 2.0f) {
            trooper_retire(m);
            return;
        }
        break;
    }
    case 5: {  // (0x002e7ba4)
        if (lwc1(pv + 0x28c) < 8.0f) swc1(pv + 0x28c, 8.0f);
        trooper_aim_joints(m, target_m + 0x10);
        trooper_to_floor(m);
        if (lbu(m + 0x53) != 2 && lbu(m + 0x52) == lbu(m + 0x53)) {
            moby_anim_set_flags(m, 2, 0, frame_scale_i(8), 5);
        }
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lbu(m + 0x53) != 0xa && lbu(m + 0x52) == lbu(m + 0x53)) {
                moby_anim_set_flags(m, 0xa, 0, frame_scale_i(5), 5);
            }
            sb(m + 0x20, 6);
        }
        break;
    }
    case 6: {  // (0x002e7c68)
        trooper_aim_joints(m, target_m + 0x10);
        trooper_to_floor(m);
        if (lwu(pv + 0x248) != 0 && anim_crossed(m, 0.0f) != 0) {
            const uint32_t muzzle = sp + 0x50, dir = sp + 0x60;
            moby_chain_point_v(lwu(pv + 0x248), 0, V(muzzle));
            vec3_sub(V(dir), V(sp + 0x20), V(muzzle));
            const float to_aim = atan2_f(lwc1(dir + 0), lwc1(dir + 4));
            const float gun_yaw = angle_add(lwc1(m + 0x48), lwc1(pv + 0x208));
            const float yaw = u2f(0x3e685696u) < angle_dist(to_aim, gun_yaw) ? gun_yaw : to_aim;
            const float pitch = atan2_f(vec2_length(V(dir)), lwc1(dir + 8));
            vec_from_spherical_v(V(dir), fpu_mul(lwc1(0x0015ed6c), 15.0f), yaw, pitch);
            moby_sound_start(0, 0, m);
            const int32_t fuse = float_to_int(fpu_div(18.0f, fpu_mul(lwc1(0x0015ed6c), 15.0f)));
            shot_launch(m, muzzle, dir, fuse);
        }
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lbu(m + 0x53) != 5 && lbu(m + 0x52) == lbu(m + 0x53)) {
                moby_anim_set_flags(m, 5, 0, frame_scale_i(8), 5);
            }
            sb(m + 0x20, 7);
        }
        break;
    }
    case 7: {  // (0x002e7de0)
        trooper_aim_joints(m, target_m + 0x10);
        trooper_to_floor(m);
        const uint32_t cur_anim = lbu(m + 0x53);
        if ((cur_anim != 5 || (lbu(m + 0x70) & 2u) != 0) && cur_anim != 0 && lbu(m + 0x52) == cur_anim) {
            moby_anim_set_flags(m, 0, 0, frame_scale_i(8), 5);
        }
        if (rand_mod(9) == 0) {
            sb(m + 0x20, 5);
        } else if (10.5f < vec2_distance(V(m + 0x10), V(target_m + 0x10)) && lbu(pv + 0x242) == 0) {
            sb(m + 0x20, 3);
        } else {
            const float to_t =
                atan2_f(fpu_sub(lwc1(target_m + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target_m + 0x14), lwc1(m + 0x14)));
            if (u2f(0x3efa35ddu) < angle_dist(lwc1(m + 0x48), to_t)) sb(m + 0x20, 3);
        }
        break;
    }
    case 8: {  // (0x002e7ee0)
        trooper_walk_toward(m, pv + 0x220, 0.0f, 0.0f);
        float bend = fpu_mul(lwc1(pv + 0x23c), 30.0f);
        const float lim = u2f(0x3f490fdbu);
        if (lim < bend) bend = lim;
        else if (bend < -lim) bend = -lim;
        swc1(pv + 0x188, fpu_mul(bend, u2f(0x3f333333u)));
        swc1(pv + 0x208, fpu_mul(bend, u2f(0x3ecccccd)));
        if (vec2_distance(V(m + 0x10), V(pv + 0x220)) < 1.0f) sb(m + 0x20, 2);
        break;
    }
    case 9: {  // (0x002e8080)
        if ((enemy_physics(m, pv + 0x70) & 0x60u) != 0) {
            moby_drop_bolts(m, 0, -1);
            trooper_death_burst(m, m + 0x10);
            trooper_retire(m);
            return;
        }
        if (lwc1(m + 0x18) < 2.0f) {
            moby_drop_bolts(m, 0, -1);
            trooper_retire(m);
            return;
        }
        break;
    }
    default:
        break;
    }

    if (lbu(m + 0x31) == 0) {
        const uint32_t gun = lwu(pv + 0x248);
        if (gun == 0 || lbu(gun + 0x31) == 0) return;
    }
    trooper_gun_update(m);
    if ((lhu(m + 0x34) & 0x8000u) != 0) {
        swc1(pv + 0x188, -lwc1(pv + 0x188));
        swc1(pv + 0x208, -lwc1(pv + 0x208));
    }
    {
        const float k = lwc1(0x0015ed64);
        joint_node_spring(m, pv + 0x120, 5, fpu_mul(k, u2f(0x3c9374bcu)), fpu_mul(k, u2f(0x3e99999au)));
    }
    {
        const float k = lwc1(0x0015ed64);
        joint_node_spring(m, pv + 0x1a0, 6, fpu_mul(k, u2f(0x3cc49ba6u)), fpu_mul(k, u2f(0x3e99999au)));
    }
}

}  // namespace rac1
