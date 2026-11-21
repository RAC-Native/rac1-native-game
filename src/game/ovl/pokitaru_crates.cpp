// Pokitaru's (level 11) parachute drops and energy-ball thrower, ported by hand from the level's own code: class 1218
// (the missile and ship-health crates floating under a parachute, with helpers L11:0x30fd98 and L11:0x30f660), class
// 1219 (the parachute itself, with its hit helper L11:0x30fe50) and class 1231 (the enemy that charges and throws an
// energy ball of class 1297, with helpers L11:0x318e78, L11:0x318ed8, L11:0x318d10, L11:0x318e98 and L11:0x310698).
// The original's order of calls and float operations kept.
#include <game/ovl/pokitaru_own.h>

#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6cu); }
float dt2() { return lwc1(0x0015ed70u); }
}  // namespace

// Spawns the parachute (class 0x4c3 = 1219) attached to crate `m`.
// orig: L11:0x0030fd98
uint32_t pok_30fd98_l11(uint32_t m) {
    const uint32_t c = moby_spawn(0x4c3);
    if (c != 0) {
        sh(c + 0x32, 0xff);
        const uint32_t cpv = lwu(c + 0x78);
        sb(c + 0x30, 0xff);
        sb(c + 0x31, 1);
        sb(c + 0x20, 0);
        sb(c + 0xbc, 0);
        lq_sq(c + 0x10, m + 0x10);
        lq_sq(c + 0x40, m + 0x40);
        sw(cpv + 0x70, m);
        if (lbu(c + 0x53) != 2) moby_anim_set(c, 2, 0xd, 0);
        moby_update_transform(c);
        moby_copy_hero_color(c);
    }
    return c;
}

// Updates crate `m`'s parachute (pvar +0x34), spawning one if unset, and records its attachment point (pvar +0) and
// hang offset (pvar +0x20); false when the parachute is missing or retired.
// orig: L11:0x0030f660
bool pok_30f660_l11(uint32_t m, uint32_t pv) {
    if (lw(pv + 0x34) == 0) {
        const uint32_t spawned = pok_30fd98_l11(m);
        sw(pv + 0x34, spawned);
        if (spawned == 0) {
            lq_sq(pv, m + 0x10);
            eer::sq(pv + 0x20, eer::Q128{});
            return false;
        }
    }
    const uint32_t c = lwu(pv + 0x34);
    const uint8_t cst = lbu(c + 0x20);
    if (cst == 0xfe || cst == 0xfd) {
        lq_sq(pv, m + 0x10);
        eer::sq(pv + 0x20, eer::Q128{});
        return false;
    }
    lq_sq(c + 0x10, m + 0x10);
    lq_sq(c + 0x40, m + 0x40);
    moby_update_transform(c);
    moby_chain_point_v(lwu(pv + 0x34), 0, V(pv));
    vec3_sub(V(pv + 0x20), V(lwu(pv + 0x34) + 0x10), V(pv));
    return true;
}

// Class 1218, the floating crate (0x4c2: +5 jet missiles; 0x4c4: +25% jet health): bursts on world/other collision;
// collected when touched by or within 4 of the jet in hero state 0x32; drifts down under its parachute (states 1..2,
// pulled toward the jet within 40 while drawn), falls freely once its parachute is shot (state 3), retires below 20 or
// in state 5.
// orig: L11:0x0030f728
void pok_class1218_update_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t jet = lwu(0x00140940u);
    const uint32_t pv = lwu(m + 0x78);
    bool touched_jet = false;
    float dist3 = 100000.0f;
    float dist2_mid = 100000.0f;

    moby_chain_point_v(m, 0, V(sp + 0x20));
    if (sphere_collide(sp + 0x20, u2f(0x3f0ccccdu), 0, m) != 0) {
        const uint32_t hit = coll_hit_moby();
        if (hit != 0) {
            const int16_t hcls = lh(hit + 0xa6);
            if (hcls == 0x4da || hcls == 0x45 || hcls == 0x563) touched_jet = true;
        }
        if (!touched_jet) {
            sw(sp, 1);
            sw(sp + 8, 0xffffffffu);
            sw(sp + 0x10, 0);
            explosion_effect(m, pv + 0x10, sp + 0x20, 3, 2, 5, -1, 0, 1, -1, 0, 0.0f, 0.0f, 0.0f, 0.0f, 9.0f, 1.0f, 0.0f);
            sb(m + 0x20, 5);
        }
    }

    if (lbu(m + 0x20) < 4u) {
        if (lw(0x001413d4u) == 0x32 && jet != 0) {
            const uint8_t jst = lbu(jet + 0x20);
            const int16_t jcls = lh(jet + 0xa6);
            if (jst != 0xfe && jst != 0xfd && (jcls == 0x4da || jcls == 0x45 || jcls == 0x563)) {
                dist3 = vec3_distance(V(jet + 0x10), V(m + 0x10));
                vec2_distance(V(jet + 0x10), V(m + 0x10));
                vec3_add(V(sp + 0x20), V(m + 0x10), V(pv));
                vec3_scale(V(sp + 0x20), V(sp + 0x20), 0.5f);
                dist2_mid = vec2_distance(V(jet + 0x10), V(sp + 0x20));
            }
        }
        if (touched_jet || dist3 < 4.0f || dist2_mid < 4.0f) {
            sw(m + 0x94, 0);
            const uint32_t chute = lwu(pv + 0x34);
            if (chute != 0) sw(chute + 0x94, 0);
            const int16_t cls = lh(m + 0xa6);
            if (cls == 0x4c2) {
                const uint32_t ship = lwu(0x00140940u);
                sb(0x00140946u, static_cast<uint8_t>(lbu(0x00140946u) + 5u));
                if (ship != 0) {
                    const uint8_t sst = lbu(ship + 0x20);
                    if (sst != 0xfe && sst != 0xfd) moby_sound_start(4, 0, ship);
                }
                if (lbu(0x00140947u) < lbu(0x00140946u)) sb(0x00140946u, lbu(0x00140947u));
            } else if (cls == 0x4c4) {
                const float bonus = fpu_mul(lwc1(0x00140950u), 0.25f);
                const uint32_t ship = lwu(0x00140940u);
                swc1(0x0014094cu, fpu_add(lwc1(0x0014094cu), bonus));
                if (ship != 0) {
                    const uint8_t sst = lbu(ship + 0x20);
                    if (sst != 0xfe && sst != 0xfd) moby_sound_start(5, 0, ship);
                }
                if (lwc1(0x00140950u) < lwc1(0x0014094cu)) swc1(0x0014094cu, lwc1(0x00140950u));
            }
            sb(m + 0x20, 4);
        }
    }

    const float grav = u2f(0x411ccccdu);  // (9.8)
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b510)
    case 0:  // (0x0030fa48)
        sw(pv + 0x34, 0);
        pok_30f660_l11(m, pv);
        sw(pv + 0x30, 0);
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x0030fa64)
        vec3_scale(V(pv + 0x10), V(pv + 0x10), u2f(0x3f666666u));
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(dt2(), grav)));
        vec3_add(V(pv), V(pv), V(pv + 0x10));
        vec3_add(V(m + 0x10), V(pv), V(pv + 0x20));
        if (pok_30f660_l11(m, pv) && lbu(lwu(pv + 0x34) + 0x52) == 1) {
            vec3_scale(V(pv + 0x10), V(pv + 0x10), u2f(0x3f4ccccdu));
            sb(m + 0x20, 2);
        }
        break;
    case 2: {  // (0x0030fb08)
        const float k08 = u2f(0x3f4ccccdu);
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(dt(), u2f(0x3d0efa35u))));
        const float sway = angle_add(lwc1(pv + 0x30), fpu_mul(dt(), u2f(0x3fc90fdbu)));
        swc1(pv + 0x30, sway);
        swc1(m + 0x44, fpu_mul(cos_vu(sway), u2f(0x3f490fdbu)));
        vec3_scale(V(pv + 0x10), V(pv + 0x10), k08);
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(dt2(), grav)));
        if (dist3 < 40.0f && lbu(m + 0x31) != 0) {
            const float pull = fpu_div(k08, sqrt_f(dist3));
            vec3_sub(V(sp + 0x20), V(jet + 0x10), V(m + 0x10));
            vec3_set_length(V(sp + 0x20), V(sp + 0x20), pull);
            vec3_add(V(pv + 0x10), V(pv + 0x10), V(sp + 0x20));
        }
        vec3_add(V(pv), V(pv), V(pv + 0x10));
        vec3_add(V(m + 0x10), V(pv), V(pv + 0x20));
        if (pok_30f660_l11(m, pv)) {
            const uint8_t cst = lbu(lwu(pv + 0x34) + 0x20);
            if (cst == 2) sb(m + 0x20, cst);
        }
        break;
    }
    case 3:  // (0x0030fc60)
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(dt2(), grav)));
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x10));
        break;
    case 4:  // (0x0030fc9c)
        sb(m + 0x20, 5);
        break;
    case 5:  // (0x0030fcc4)
        moby_retire(m);
        ee_stack_free(0xc0);
        return;
    default:
        break;
    }

    if (lwc1(m + 0x18) < 20.0f) {
        moby_retire(m);
        ee_stack_free(0xc0);
        return;
    }
    const float hi = 1003.0f;
    for (uint32_t off = 0x10; off <= 0x18; off += 4) {
        if (lwc1(m + off) < 20.0f) swc1(m + off, 20.0f);
        if (hi < lwc1(m + off)) swc1(m + off, hi);
    }
    floor_light_add(m, 0.5f);
    ee_stack_free(0xc0);
}

// Hit reaction and colour pulse for parachute `m` (`hp` = pvar +0x20): when killed, drops its crate (pvar +0x70) into
// free fall (state 3) and enters state 3 itself.
// orig: L11:0x0030fe50
void pok_30fe50_l11(uint32_t m, uint32_t pv, uint32_t hp) {
    const uint32_t sp = ee_stack_alloc(0x80);
    if (lbu(m + 0x20) != 4) {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
        if (hit != 0) {
            libgcc_extendsfdf2(lwc1(hit + 0x2c));
            debug_printf_stub();
        }
        int32_t st = static_cast<int32_t>(lw(sp));
        const uint32_t react = static_cast<uint32_t>(moby_hit_react_r(m, hit, hp, 0, st, nullptr, nullptr, 4));
        sw(sp, static_cast<uint32_t>(st));
        switch (react) {  // (jump table L11:0x21b560)
        case 1: case 2:  // (0x0030ff0c)
            sw(hp, 0);
            break;
        default:  // (0x0030ff10)
            break;
        }
        if (!(lw(sp) < 2)) {
            const float dmg = lwc1(hit + 0x2c);
            const float cur = lwc1(hp);
            if (cur <= dmg) {
                sw(hp, 0);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                sb(pv + 0x67, 0x78);
                color_pulse_start(m, pv + 0x60);
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 3);
                const uint32_t crate = lwu(pv + 0x70);
                if (crate != 0) {
                    const uint8_t cst = lbu(crate + 0x20);
                    if (cst != 0xfe && cst != 0xfd) sb(crate + 0x20, 3);
                }
                sb(m + 0x20, 3);
            } else {
                swc1(hp, fpu_sub(cur, dmg));
                sb(pv + 0x67, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pv + 0x60);
                if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, 3);
            }
        }
        sb(m + 0xa4, 0xff);
    }
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0x80);
}

// Class 1219, a crate's parachute: retires once its crate (pvar +0x70) is gone; opens (0 -> 1 -> 2), takes hits in 2,
// fades out over its tear animation in 3 -> 4 (retired).
// orig: L11:0x00310028
void pok_class1219_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t crate = lwu(pv + 0x70);
    if (crate == 0 || lbu(crate + 0x20) == 0xfe || lbu(crate + 0x20) == 0xfd) {
        if (lbu(m + 0x20) < 3u) sb(m + 0x20, 4);
    }
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b590)
    case 0:  // (0x003100a0)
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0xf);
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x003100c8)
        if ((lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 2);
            sb(m + 0xa4, 0xff);
        }
        break;
    case 2:  // (0x003100e8)
        pok_30fe50_l11(m, pv, pv + 0x20);
        if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0xa);
        break;
    case 3: {  // (0x00310124)
        const uint8_t alpha = lbu(m + 0x23);
        if (alpha != 0) sb(m + 0x23, static_cast<uint8_t>(alpha - 1u));
        if (13.5f < moby_anim_time(m)) sb(m + 0x20, 4);
        break;
    }
    case 4:  // (0x00310160)
        moby_retire(m);
        break;
    default:
        break;
    }
}

// Holds charging energy ball `shot` at `pos`, keeping its hold countdown (+0x28) at 5.
// orig: L11:0x00318e78
void pok_318e78_l11(uint32_t shot, uint32_t pos) {
    const uint32_t spv = lwu(shot + 0x78);
    lq_sq(shot + 0x10, pos);
    sw(spv + 0x28, 5);
}

// Spawns a converging particle-02 spark around energy ball `shot`.
// orig: L11:0x00318ed8
uint32_t pok_318ed8_l11(uint32_t shot) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const float dist = rand_range_f(2.5f, 3.0f);
    const float ang = rand_angle();
    vec3_set_length(V(sp + 0x30), V(shot + 0xd0), fpu_mul(dist, sin_vu(ang)));
    vec3_set_length(V(sp + 0x20), V(shot + 0xe0), fpu_mul(dist, cos_vu(ang)));
    vec3_add(V(sp + 0x20), V(sp + 0x20), V(sp + 0x30));
    vec3_sub(V(sp + 0x10), V(shot + 0x10), V(sp + 0x20));
    const int32_t sum = lw(L11_ADDR(0x00162394u)) + lw(L11_ADDR(0x00162398u)) + lw(L11_ADDR(0x0016239cu));
    const float inv = fpu_div(1.0f, int_to_float(frame_scale_i(sum)));
    vec3_scale(V(sp + 0x40), V(sp + 0x20), inv);
    lq_sq(sp + 0x50, sp + 0x40);
    swc1(sp + 0x4c, lwc1(L11_ADDR(0x001623a0u)));
    swc1(sp + 0x5c, lwc1(L11_ADDR(0x001623a4u)));
    const uint32_t res = spawn_particle_02(sp + 0x10, sp + 0x40, sp + 0x50, lwu(L11_ADDR(0x0016238cu)),
                                           lwu(L11_ADDR(0x00162390u)), lwu(L11_ADDR(0x00162394u)),
                                           lwu(L11_ADDR(0x00162398u)), lwu(L11_ADDR(0x0016239cu)),
                                           lw(L11_ADDR(0x001623a8u)));
    ee_stack_free(0xc0);
    return res;
}

// Spawns energy ball (class 0x511 = 1297) for thrower `m` at `pos` with charge timer `frames` and 50 initial sparks.
// orig: L11:0x00318d10
uint32_t pok_318d10_l11(uint32_t m, uint32_t pos, int32_t frames) {
    const uint32_t shot = moby_spawn(0x511);
    if (shot != 0) {
        sb(shot + 0x30, 0xff);
        sh(shot + 0x32, 0x40);
        sb(shot + 0x31, 1);
        sw(shot + 0x2c, 0);
        const uint32_t spv = lwu(shot + 0x78);
        sb(shot + 0x20, 1);
        sd(shot + 0x38, ld(m + 0x38));
        lq_sq(shot + 0x10, pos);
        lq_sq(shot + 0x40, m + 0x40);
        vec_clear(V(spv));
        sw(spv + 0x24, m);
        sw(spv + 0x20, static_cast<uint32_t>(frame_scale_i(frames)));
        sw(spv + 0x28, 5);
        euler_to_mat3_vu(M(shot + 0xc0), V(shot + 0x40));
        for (uint32_t k = 0; k < 4; ++k) {
            sb(spv + 0x10 + k, static_cast<uint8_t>(rand_mod(0xff)));
            sb(spv + 0x14 + k, (k & 1u) != 0 ? 0xffu : 1u);
            sb(spv + 0x18 + k, static_cast<uint8_t>((k + 1u) * 10u));
        }
        for (int k = 0; k < 50; ++k) pok_318ed8_l11(shot);
    }
    return shot;
}

// Launches energy ball `shot` into state 2 with velocity `vel` for 60 frames.
// orig: L11:0x00318e98
void pok_318e98_l11(uint32_t shot, uint32_t vel) {
    sb(shot + 0x20, 2);
    const uint32_t spv = lwu(shot + 0x78);
    lq_sq(spv, vel);
    sw(spv + 0x20, static_cast<uint32_t>(frame_scale_i(0x3c)));
}

// Class 1231's hit reaction, held energy-ball positioning, knockback / death launch, colour pulse, and target search
// inside its zone polygon (pvar +0x13c).
// orig: L11:0x00310698
void pok_310698_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t pv = lwu(m + 0x78);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x158)));
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L11_ADDR(0x00161f9cu))));
    if (lwu(pv + 0x160) != 0) {
        vec3_set_length(V(sp), V(m + 0xc0), lwc1(L11_ADDR(0x00161fc8u)));
        vec3_add(V(sp), V(sp), V(m + 0x10));
        swc1(sp + 8, fpu_add(lwc1(sp + 8), lwc1(L11_ADDR(0x00161fccu))));
        pok_318e78_l11(lwu(pv + 0x160), sp);
    }
    swc1(sp + 0x14, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    int32_t st = static_cast<int32_t>(lw(sp + 0x10));
    float dmg = lwc1(sp + 0x14);
    uint32_t kind = static_cast<uint32_t>(moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4));
    sw(sp + 0x10, static_cast<uint32_t>(st));
    swc1(sp + 0x14, dmg);
    if (lw(sp + 0x10) != 1 && lbu(m + 0x20) != 8) {
        const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(sp + 0x14));
        swc1(pv + 0x20, hp);
        if (hp <= 0.0f) kind = 1;
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(512.0f)));
        swc1(pv + 0x98, 0.5f);
        swc1(pv + 0x80, fpu_mul(lwc1(L11_ADDR(0x00161f88u)), dt2()));
        swc1(pv + 0x84, u2f(0x3a03126fu));
        sw(pv + 0x94, 9);
        sb(pv + 0xad, 0);
        switch (kind) {  // (jump table L11:0x21b5e0)
        case 9: case 10:  // (0x00310810)
            sb(pv + 0x67, 0xfa);
            break;
        case 3: case 4: case 5: case 6: case 7: case 8: {  // (0x0031081c)
            const float d = dt();
            const float sx = fpu_mul(lwc1(L11_ADDR(0x00161f90u)), d);
            const float sy = fpu_mul(lwc1(L11_ADDR(0x00161f8cu)), d);
            swc1(pv + 0xc0, 3.0f);
            swc1(pv + 0xc4, 6.0f);
            swc1(pv + 0x88, sx);
            swc1(pv + 0x8c, sy);
            const uint32_t src = lwu(hit + 0x20);
            swc1(sp + 0x18, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14))));
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x18)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x88)), *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, 5, 1, 0, lwc1(sp + 0x18));
            sb(m + 0x20, 7);
            sb(pv + 0x67, 0x78);
            const uint32_t shot = lwu(pv + 0x160);
            if (shot != 0) {
                moby_retire(shot);  // (L11:0x003191a0)
                sw(pv + 0x160, 0);
            }
            sw(pv + 0x158, lwu(pv + 0x158) + static_cast<uint32_t>(frame_scale_i(0x3c)));
            break;
        }
        case 1: case 2: {  // (0x003108f0)
            const float d = dt();
            const float grav = fpu_mul(lwc1(L11_ADDR(0x00161f88u)), dt2());
            const float sx = fpu_mul(lwc1(L11_ADDR(0x00161f98u)), d);
            const float sy = fpu_mul(lwc1(L11_ADDR(0x00161f94u)), d);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            swc1(pv + 0x80, grav);
            swc1(pv + 0xc0, 4.0f);
            swc1(pv + 0xc4, 10.0f);
            swc1(pv + 0x88, sx);
            swc1(pv + 0x8c, sy);
            const uint32_t src = lwu(hit + 0x20);
            swc1(sp + 0x1c, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14))));
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x1c)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x88)), *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, 6, 1, 0, lwc1(sp + 0x1c));
            sb(m + 0x20, 8);
            sb(pv + 0x67, 0xf0);
            const uint32_t shot = lwu(pv + 0x160);
            if (shot != 0) {
                moby_retire(shot);  // (L11:0x003191a0)
                sw(pv + 0x160, 0);
            }
            moby_drop_bolts(m, 0, -1);
            break;
        }
        default:  // (0x003109dc)
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    swc1(pv + 0x140, 24.0f);
    const uint32_t poly = lvd::path(lw(pv + 0x13c));
    if (target_select(m, pv + 0xd0, 0, 0, poly + 0x10, lw(poly), 24.0f) != 2) {
        if (lwc1(pv + 0x140) < vec2_distance(V(m + 0x10), V(pv + 0xd0)) ||
            3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0xd8)))) {
            sw(pv + 0x114, 2);
        }
    }
    if (lw(pv + 0x110) == 0) sw(pv + 0x110, lwu(0x001413d0u));
    ee_stack_free(0x80);
}

// Class 1231, the energy-ball thrower: state 0 initializes its health (3) and goes to 1 (placed on path +0x134 when
// +0x130 is set) or 3 (idle); 3 turns toward the hero and starts its throw (4) once a target is in range (+0x114 != 2);
// 4 spawns its energy ball at frame 15 -> 5; 5 launches the ball at frame 33 -> 6 (recovery -> 3); 7 is knocked back
// (dying if outside its polygon +0x13c); 8 falls dead and bursts.
// orig: L11:0x00310180
void pok_class1231_update_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t pv = lwu(m + 0x78);
    pok_310698_l11(m);
    const uint32_t tgt = lwu(pv + 0x110);
    joint_node_scale(m, 0, pv + 0x1e0, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L11_ADDR(0x001677c0u))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    const float two_pi = u2f(0x40c90fdbu);
    auto turn_to = [&](uint32_t who) {
        const float to = atan2_f(fpu_sub(lwc1(who + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(who + 0x14), lwc1(m + 0x14)));
        const float accel = fpu_mul(dt2(), two_pi);
        yaw_seek(m + 0x48, pv + 0x14c, to, accel, accel, fpu_mul(dt(), two_pi));
    };
    auto anim_to = [&](uint32_t set) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(0xa));
    };

    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b5b0)
    case 0:  // (0x00310230)
        sh(pv + 0x24, 3);
        swc1(pv + 0x20, 3.0f);
        sb(pv + 0x28, 2);
        sb(pv + 0x2a, 6);
        sb(pv + 0x58, static_cast<uint8_t>(float_to_int(8.0f)));
        sb(pv + 0x5a, static_cast<uint8_t>(float_to_int(8.0f)));
        sb(m + 0x20, 3);
        anim_to(0);
        if (lw(pv + 0x130) != 0) {
            sb(m + 0x20, 1);
            lq_sq(m + 0x10, lvd::path(lw(pv + 0x134)) + 0x10);
        } else {
            sb(m + 0x20, 3);
            anim_to(0);
        }
        break;
    case 3:  // (0x003102f4)
        turn_to(lwu(0x001413d0u));
        if (lw(pv + 0x114) != 2) {
            sb(m + 0x20, 4);
            anim_to(2);
        }
        break;
    case 4:  // (0x00310388)
        turn_to(tgt);
        if (anim_crossed(m, 15.0f) != 0) {
            vec3_set_length(V(sp), V(m + 0xc0), lwc1(L11_ADDR(0x00161fc8u)));
            vec3_add(V(sp), V(sp), V(m + 0x10));
            swc1(sp + 8, fpu_add(lwc1(sp + 8), lwc1(L11_ADDR(0x00161fccu))));
            sw(pv + 0x160, pok_318d10_l11(m, sp, 5));
        } else if ((lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 5);
            anim_to(3);
        }
        break;
    case 5:  // (0x00310474)
        turn_to(tgt);
        if (lwu(pv + 0x160) != 0 && anim_crossed(m, 33.0f) != 0) {
            vec3_set_length(V(sp), V(m + 0xc0), fpu_mul(dt(), 30.0f));
            pok_318e98_l11(lwu(pv + 0x160), sp);
            sw(pv + 0x160, 0);
        }
        if ((lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 6);
            anim_to(4);
        }
        break;
    case 6:  // (0x00310550)
        if ((lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 3);
            anim_to(0);
        }
        break;
    case 7: {  // (0x00310568)
        uint32_t phys = static_cast<uint32_t>(enemy_physics(m, pv + 0x70));
        if ((phys & 0x41u) != 0) {
            const uint32_t poly = lvd::path(lw(pv + 0x13c));
            if (point_in_polygon(m + 0x10, poly + 0x10, lw(poly)) != 0) {
                sb(m + 0x20, 3);
                anim_to(0);
                break;
            }
            phys = 0x100u;
        }
        if ((phys & 0x120u) != 0) {
            moby_drop_bolts(m, 0, -1);
            enemy_death_burst(m, m + 0x10, 0.5f, 10.0f);
            moby_retire(m);
        }
        break;
    }
    case 8:  // (0x0031062c)
        if ((static_cast<uint32_t>(enemy_physics(m, pv + 0x70)) & 0x160u) != 0) {
            enemy_death_burst(m, m + 0x10, 1.0f, 10.0f);
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
        }
        break;
    default:  // (1, 2: 0x00310678)
        break;
    }
    ee_stack_free(0x70);
}

}  // namespace rac1
