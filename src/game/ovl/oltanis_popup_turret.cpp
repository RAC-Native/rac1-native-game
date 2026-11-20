// Oltanis (level 14), class 30: the turrets that rise out of the ground when a searchlight drone (class 8) spots the
// hero. Dormant underground (state 1) until its group is woken (+0x88), a turret rises (2), waits above ground (3),
// sweeps the line of its box (+0x80) firing shots of class 681 (0x2a9) while a target is inside its trigger box (+0x90)
// (4), and sinks again (5) when every turret of its group is idle. Destroyed, it explodes (6).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

#include <cstdint>

namespace rac1 {

namespace {

// orig: L14:0x002b4340 -- hit handling and damage flash for class 30
void class30_hit_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    if (hit != 0) {
        const uint32_t src = lwu(hit + 0x20);
        if (src != 0) {
            const int16_t src_cls = lh(src + 0xa6);
            if (src_cls == lh(m + 0xa6) || src_cls == 0x2a9) {
                hit = 0;
            }
        }
    }

    int32_t st = 0;
    float dmg = 0.0f;
    uint32_t react = static_cast<uint32_t>(moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4));
    if (st != 1 && lbu(m + 0x20) != 6) {
        const float hp = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, hp);
        if (hp <= 0.0f) {
            react = 1;
        }
        // jump table L14:0x001fbdc0 (12 entries)
        switch (react) {
        case 0:
        case 11:  // (0x002b446c)
            break;
        case 1:
        case 2:  // (0x002b4440)
            sb(m + 0x20, 6);
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xefffu));
            sb(pv + 0x67, 0xf0);
            moby_drop_bolts(m, 0, -1);
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:  // (0x002b4434)
            sb(pv + 0x67, 0xc8);
            break;
        case 9:
        case 10:  // (0x002b4428)
            sb(pv + 0x67, 0x64);
            break;
        default:
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }

    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// orig: L14:0x002eccf0 -- spawn a class 0x2a9 turret projectile
uint32_t class30_spawn_shot(const Vec4& pos, const Vec4& vel, uint32_t owner, uint32_t flag) {
    const uint32_t shot = moby_spawn(0x2a9);
    if (shot == 0) return 0;

    sw(shot + 0x40, 0);
    sh(shot + 0x32, 0x7f);
    sb(shot + 0x30, 0xff);
    sb(shot + 0x31, 1);

    const uint32_t pv = lwu(shot + 0x78);
    swc1(shot + 0x44, -atan2_f(vec2_length(vel), vel.z));
    swc1(shot + 0x48, atan2_f(vel.x, vel.y));
    V(shot + 0x10) = pos;
    V(pv + 0x10) = pos;
    V(pv + 0x00) = vel;

    moby_anim_set(shot, 1, 0, frame_scale_i(5));
    sh(shot + 0x34, 0x208);
    sw(pv + 0x24, flag);
    sw(pv + 0x20, owner);
    sb(shot + 0x23, 0x10);
    moby_update_transform(shot);
    return shot;
}

}  // namespace

// orig: L14:0x002b3bf0
void olt_class30_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class30_hit_update(m);

    // jump table L14:0x001fbda0 (7 entries)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002b3c44) init
        swc1(pv + 0x70, lwc1(m + 0x18));
        sw(pv + 0x8c, 0);
        swc1(pv + 0x84, lwc1(m + 0x48));
        sh(pv + 0x74, 0);
        if (lw(pv + 0x80) == -1 || lw(pv + 0x90) == -1) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        sh(pv + 0x76, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x00161534)))));
        sb(pv + 0x8b, 0);
        if (lh(pv + 0x88) != 0) {
            sb(m + 0x20, 3);
            sb(pv + 0x8a, 1);
        } else {
            sb(m + 0x20, 1);
            sw(m + 0x94, 0);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lwc1(L14_ADDR(0x00161508))));
            sb(pv + 0x8a, 0);
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xefffu));
        }
        sw(pv + 0x98, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x94))));
        break;
    }
    case 1: {  // (0x002b3d0c) dormant underground
        if (lh(pv + 0x88) == 0) return;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x98))) != 0) {
            sb(m + 0x20, 2);
            const int16_t dur = static_cast<int16_t>(frame_scale_i(lw(L14_ADDR(0x0016150c))));
            sh(pv + 0x74, static_cast<uint16_t>(dur));
            swc1(pv + 0x78, fpu_div(1.0f, int_to_float(dur)));
            moby_set_anim(m, 1, 0);
            swc1(m + 0x58, fpu_div(1.0f, fpu_mul(int_to_float(lh(pv + 0x74)), lwc1(L14_ADDR(0x00161510)))));
        }
        break;
    }
    case 2: {  // (0x002b3d80) rising
        const float t = fpu_mul(static_cast<float>(lh(pv + 0x74)), lwc1(pv + 0x78));
        const float lift = fpu_add(fpu_mul(fpu_sub(lwc1(L14_ADDR(0x00161508)), 0.0f), t), 0.0f);
        swc1(m + 0x18, fpu_sub(lwc1(pv + 0x70), lift));
        if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == 1) {
            moby_set_anim(m, 2, 0);
            swc1(m + 0x58, 1.0f);
        }
        if (lh(pv + 0x74) < frame_scale_i(lw(L14_ADDR(0x0016150c)) >> 1)) {
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) | 0x1000u));
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x74))) != 0) {
            sh(pv + 0x76, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x00161534)))));
            sb(m + 0x20, 3);
            sh(pv + 0x74, static_cast<uint16_t>(frame_scale_i(60)));
            swc1(m + 0x18, lwc1(pv + 0x70));
            sw(pv + 0x8c, 0);
        }
        break;
    }
    case 3: {  // (0x002b3e68) idle above ground
        const uint32_t tgt_addr = ee_stack_alloc(0x10);
        const int32_t sel = target_select_any(m, tgt_addr, 34.0f);
        const bool in_box = (sel != 2 && point_in_box(tgt_addr, lw(pv + 0x90)) != 0);
        ee_stack_free(0x10);
        if (in_box) {
            sb(m + 0x20, 4);
            moby_set_anim(m, 3, 0);
            sh(pv + 0x74, static_cast<uint16_t>(frame_scale_i(120)));
            sh(pv + 0x9c, static_cast<uint16_t>(frame_scale_i(20)));
        } else if (lbu(pv + 0x8a) == 0 && timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x76))) != 0) {
            sb(pv + 0x8b, 1);
        }
        break;
    }
    case 4: {  // (0x002b3ef0) sweeping and firing
        const float step = (lh(pv + 0x9e) != 0) ? -lwc1(L14_ADDR(0x00161514)) : lwc1(L14_ADDR(0x00161514));
        const float phase = angle_add(lwc1(pv + 0x8c), step);
        swc1(pv + 0x8c, phase);
        const float ang = angle_add(phase, fpu_mul(lwc1(pv + 0x84), u2f(0x3c8efa35u)));  // deg->rad
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x80));
        Vec4 aim{};
        vec3_scale(aim, V(box + 0x10), sin_vu(ang));
        vec3_add(aim, aim, V(box + 0x30));
        swc1(m + 0x48, atan2_f(fpu_sub(aim.x, lwc1(m + 0x10)), fpu_sub(aim.y, lwc1(m + 0x14))));

        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x9c)));
        if (ee_rem(lw(OV(0x0015f5cc)), lw(L14_ADDR(0x00161530))) == 0) {
            const int32_t barrel = rand_mod(3);
            Vec4 muzzle{};
            Vec4 side{};
            if (barrel == 0) {
                vec3_set_length(muzzle, V(m + 0xe0), lwc1(L14_ADDR(0x00161520)));
            } else if (barrel == 1) {
                vec3_set_length(muzzle, V(m + 0xe0), lwc1(L14_ADDR(0x00161524)));
                vec3_set_length(side, V(m + 0xd0), lwc1(L14_ADDR(0x0016152c)));
                vec3_add(muzzle, muzzle, side);
            } else {
                vec3_set_length(muzzle, V(m + 0xe0), lwc1(L14_ADDR(0x00161524)));
                vec3_set_length(side, V(m + 0xd0), -lwc1(L14_ADDR(0x0016152c)));
                vec3_add(muzzle, muzzle, side);
            }
            Vec4 fwd{};
            vec3_set_length(fwd, V(m + 0xc0), lwc1(L14_ADDR(0x00161528)));
            vec3_add(muzzle, muzzle, fwd);
            vec3_add(muzzle, muzzle, V(m + 0x10));
            vec3_set_length(fwd, V(m + 0xc0), fpu_mul(lwc1(L14_ADDR(0x0016151c)), lwc1(0x0015ed6cu)));
            if (lh(pv + 0x9c) == 0) {
                const uint32_t shot = class30_spawn_shot(muzzle, fwd, m, 1);
                moby_set_color_hi(shot, 0xe0, 0xe0, 0xe0);
            }
        }

        const uint32_t tgt_addr = ee_stack_alloc(0x10);
        const int32_t sel = target_select_any(m, tgt_addr, 34.0f);
        const bool lost = (sel == 2 || point_in_box(tgt_addr, lw(pv + 0x90)) == 0);
        ee_stack_free(0x10);
        if (lost && timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x74))) != 0 &&
            (lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == 3) {
            sh(pv + 0x76, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x00161534)))));
            sb(m + 0x20, 3);
            moby_set_anim(m, 2, 0);
            swc1(m + 0x58, 1.0f);
        }
        break;
    }
    case 5: {  // (0x002b41b0) retracting
        const float t = fpu_mul(static_cast<float>(lh(pv + 0x74)), lwc1(pv + 0x78));
        const float depth = lwc1(L14_ADDR(0x00161508));
        const float lift = fpu_add(depth, fpu_mul(fpu_sub(0.0f, depth), t));
        swc1(m + 0x18, fpu_sub(lwc1(pv + 0x70), lift));
        const float yaw0 = lwc1(pv + 0x84);
        swc1(m + 0x48, fpu_add(yaw0, fpu_mul(fpu_sub(lwc1(pv + 0x8c), yaw0), t)));
        if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == 4) {
            moby_set_anim(m, 0, 0);
            swc1(m + 0x58, 1.0f);
        }
        if (lh(pv + 0x74) < frame_scale_i(lw(L14_ADDR(0x0016150c)) >> 1)) {
            sw(m + 0x94, 0);
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x74))) != 0) {
            sb(m + 0x20, 1);
            swc1(m + 0x48, lwc1(pv + 0x84));
            sh(pv + 0x88, 0);
            sh(pv + 0x74, static_cast<uint16_t>(frame_scale_i(60)));
            sw(pv + 0x98, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x94))));
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xefffu));
            swc1(m + 0x18, fpu_sub(lwc1(pv + 0x70), lwc1(L14_ADDR(0x00161508))));
        }
        break;
    }
    case 6: {  // (0x002b42b0) exploding
        explosion_effect(m, pv + 0x40, m + 0x10, 5, 2, 4, 0, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(m);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
