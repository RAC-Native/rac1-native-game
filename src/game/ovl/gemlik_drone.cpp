// Gemlik Base's (level 13) class 101: a hovering gun drone. It bobs around its home point, turns toward its target
// (target_select along its path, the hero's moby when nothing else) and fires the two shots (class 1233, 0x4d1) it holds
// at its chain points 0 and 1 in turn; hit, it flinches (state 7) or explodes (state 8). Level 13 only: the addresses
// are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp.
//
// Its pvars: +0x20 its hit record (+0x20 its health 4), +0x60 its colour pulse, +0x70 its target_select record (0x48
// bytes: +0xb0 the target, +0xb4 the result: 2 not seen), +0xc0 its home point, +0xd0 its path, +0xd4 a box to fly to
// first (-1 none), +0xd8 its range (40; 50 while it fires), +0xdc its yaw's speed, +0xe8 its shots' cooldown, +0xec
// its shots' range, +0xf0 its speed toward the box, +0xf4 the gun that fires next (0 / 1), +0xf8 / +0xfc the shots it
// holds (guns 1 / 0), +0x100 / +0x104 and +0x108 / +0x10c its bobbing's angles, +0x110 its hum (a sound, -1 none).
// States (the table L13:0x1f4630): 0 set up, 1 / 3 nothing, 2 watch (turns to the target), 4 fire (its +0xbc the step
// below, the table L13:0x1f4660), 5 wait to see the target, 6 fly to the box, 7 flinch, 8 explode.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

// A shot (class 1233) held at the drone `m`'s chain point `joint` (16 bits): drawn with the drone, waiting (state 1).
// The shot, or 0.
// orig: L13:0x003058a8
uint32_t gml_3058a8_l13(uint32_t m, int32_t joint) {
    const uint32_t s = moby_spawn(0x4d1);
    if (s == 0) return 0;
    const uint32_t pv = lwu(s + 0x78);
    sh(s + 0x32, 0x7f);
    sb(s + 0x20, 1);
    sb(s + 0x31, 1);
    sb(s + 0x30, 0xff);
    sw(pv + 0x20, m);
    sh(pv + 0x26, static_cast<uint32_t>(joint));
    sh(pv + 0x24, 1);
    moby_chain_point_v(m, static_cast<uint32_t>(joint), V(s + 0x10));
    lq_sq(s + 0x40, m + 0x40);
    lq_sq(s + 0xc0, m + 0xc0);
    lq_sq(s + 0xd0, m + 0xd0);
    lq_sq(s + 0xe0, m + 0xe0);
    lq_sq(pv + 0x10, s + 0x10);
    sw(s + 0x94, 0);
    moby_update_transform(s);
    return s;
}

// The held shot `s` fired from its chain point along `vel` (an EE vector) at `target` for `range` (state 0).
// orig: L13:0x00305988
void gml_305988_l13(uint32_t s, uint32_t vel, uint32_t target, float range) {
    const uint32_t pv = lwu(s + 0x78);
    const uint32_t owner = lwu(pv + 0x20);
    swc1(pv + 0x28, range);
    lq_sq(s + 0x40, owner + 0x40);
    lq_sq(s + 0xc0, owner + 0xc0);
    lq_sq(s + 0xd0, owner + 0xd0);
    lq_sq(s + 0xe0, owner + 0xe0);
    moby_chain_point_v(lwu(pv + 0x20), static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x26))), V(s + 0x10));
    lq_sq(pv + 0x10, s + 0x10);
    lq_sq(pv + 0x00, vel);
    sw(pv + 0x2c, target);
    sb(s + 0x20, 0);
    sh(s + 0x34, lhu(s + 0x34) & 0xfffeu);
    sw(s + 0x94, lwu(lwu(s + 0x24) + 0x10));
    moby_update_transform(s);
}

// The drone's every frame (but its first): its hum, its bobbing about its home point, its engines' sparks, its scale,
// its hits, and its target.
// orig: L13:0x002c2d30
void gml_2c2d30_l13(uint32_t m) {
    const Frame f(192);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t jet0 = sp, jet1 = sp + 0x10, vel = sp + 0x20;
    if (lbu(m + 0x20) == 0) return;
    {  // the hum (started again when it stopped)
        const int32_t snd = lw(pv + 0x110);
        if (snd == -1 || sub_0028d8c0(m, snd) == 0) {
            sw(pv + 0x110, 0xffffffffu);
            sw(pv + 0x110, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        }
    }
    sub_00262b00(m, pv + 0x100, pv + 0x104, 0.5f, fpu_mul(dt(), u2f(0x3fb2b8c2u)));
    sub_00262b80(m, pv + 0x108, pv + 0x10c, u2f(0x3dd67750u), fpu_mul(dt(), u2f(0x3ea0d97cu)), fpu_mul(dt(), u2f(0x3ebba866u)));
    swc1(m + 0x10, fpu_add(lwc1(pv + 0xc0), sin_vu(lwc1(pv + 0x108))));
    swc1(m + 0x14, fpu_add(lwc1(pv + 0xc4), sin_vu(lwc1(pv + 0x10c))));
    // its two engines' sparks (chain points 2 and 3) blown down: red on odd frames, white on even ones, and a flame
    sq_zero(sp + 0x30);
    swc1(sp + 0x38, -fpu_mul(rand_range_f(4.5f, 5.5f), dt()));
    lq_sq(vel, sp + 0x30);
    moby_chain_point_v(m, 2, V(jet0));
    moby_chain_point_v(m, 3, V(jet1));
    if ((lw(L13_ADDR(0x0015f5cc)) & 1) != 0) {
        for (const uint32_t at : {jet0, jet1})
            spawn_particle_4(at, vel, 0xcf0000ffu, 0xcf, static_cast<uint32_t>(frame_scale_i(rand_range(0x12, 0x16))), 0x32, static_cast<uint32_t>(-10), 1);
    } else {
        for (const uint32_t at : {jet0, jet1})
            spawn_particle_4(at, vel, 0x6000ffffu, 0x80, static_cast<uint32_t>(frame_scale_i(rand_range(0x14, 0x18))), 0x64, static_cast<uint32_t>(-10), 1);
    }
    for (const uint32_t at : {jet0, jet1}) {
        const int32_t size = rand_range(10, 0x19);
        spawn_particle_4(at, vel, 0xefff7f4fu, 0xff0000u, static_cast<uint32_t>(frame_scale_i(rand_range(10, 0xf))), static_cast<uint32_t>(size), static_cast<uint32_t>(-size), 1);
    }
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L13_ADDR(0x00161490))));

    // its hits (its own shots' do nothing)
    swc1(sp + 0x44, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t st = lw(sp + 0x40);
    float dmg = lwc1(sp + 0x44);
    int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    if (hit != 0) {
        const uint32_t by = lwu(hit + 0x20);
        if (by != 0 && lh(by + 0xa6) == 0x4d1) {
            dmg = 0.0f;
            st = 1;
        }
    }
    if (st != 1) {
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), dmg));
        if (lbu(m + 0x53) != 7) moby_anim_set(m, 7, 0, 5);
        if (lwc1(pv + 0x20) <= 0.0f) react = 1;
        else sb(m + 0x20, 7);
        switch (static_cast<uint32_t>(react)) {  // (the table L13:0x1f4680)
        case 1: case 2:  // (0x002c315c): it explodes
            moby_drop_bolts(m, 0, -1);
            sb(m + 0x20, 8);
            sb(pv + 0x67, 0xf0);
            break;
        case 3: case 4: case 5: case 6: case 7: case 8:  // (0x002c3150)
            sb(pv + 0x67, 0x78);
            break;
        case 9: case 10:  // (0x002c3144)
            sb(pv + 0x67, 0xfa);
            break;
        default:  // (0, 11 and beyond: 0x002c317c)
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);

    // its target: along its path within its range, not more than 8 above or below it, in its line of sight (the hero's
    // head); +0xb4 2 when not seen
    swc1(pv + 0xd8, lbu(m + 0x20) == 4 ? 50.0f : 40.0f);
    {
        const uint32_t path = lvd::path(lw(pv + 0xd0));
        if (target_select(m, pv + 0x70, 0, 0, path + 0x10, lw(path), lwc1(pv + 0xd8)) != 2) {
            if (lwc1(pv + 0xd8) < vec2_distance(V(m + 0x10), V(pv + 0x70)))
                sw(pv + 0xb4, 2);
            else if (8.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x78))))
                sw(pv + 0xb4, 2);
            else if (lbu(m + 0x20) != 5 && segment_collide_v(V(m + 0x10), V(0x0013f420), 2, m, nullptr) != 0)
                sw(pv + 0xb4, 2);
        }
    }
    if (lwu(pv + 0xb0) == 0) sw(pv + 0xb0, lwu(0x001413d0));
}

// orig: L13:0x002c25b8
void gml_class101_update_l13(uint32_t m) {
    const Frame f(192);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    gml_2c2d30_l13(m);
    auto anim_to = [m](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    // (state 4) the next step once the animation ended (+0x70 bit 2)
    auto ended = [m] { return (lbu(m + 0x70) & 2u) != 0; };
    auto step = [m, anim_to](uint32_t next) {
        sb(m + 0xbc, next);
        anim_to(next, 2);
    };
    const uint32_t tgt = lwu(pv + 0xb0);
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4630)
    case 0: {  // (0x002c2610): set up
        lq_sq(pv + 0xc0, m + 0x10);
        sh(m + 0x34, lhu(m + 0x34) | 0x5000u);
        swc1(pv + 0xec, 15.0f);
        swc1(pv + 0x20, 4.0f);
        sh(pv + 0x24, 4);
        sb(pv + 0x28, 3);
        sb(pv + 0x58, 0x32);
        sb(pv + 0x5a, 0);
        sh(m + 0x32, 0xff);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
        anim_to(0, 0xa);
        sb(m + 0xbc, 0);
        sw(pv + 0xfc, gml_3058a8_l13(m, 0));
        sw(pv + 0xf8, gml_3058a8_l13(m, 1));
        sb(m + 0x20, lw(pv + 0xd4) == -1 ? 2 : 5);
        swc1(pv + 0x100, rand_angle());
        swc1(pv + 0x108, rand_angle());
        swc1(pv + 0x10c, rand_angle());
        sw(pv + 0x110, 0xffffffffu);
        return;
    }
    case 2: {  // (0x002c2828): watch; once the target is seen, fire
        const float yaw = atan2_f(fpu_sub(lwc1(tgt + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 0x14), lwc1(m + 0x14)));
        const float acc = fpu_mul(dt_sq(), u2f(0x4096cbe4u));
        yaw_seek(m + 0x48, pv + 0xdc, yaw, acc, acc, fpu_mul(dt(), u2f(0x4121f770u)));
        if (lw(pv + 0xb4) == 2) return;
        swc1(pv + 0xec, 15.0f);
        sb(m + 0xbc, 1);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
        sb(m + 0x20, 4);
        return;
    }
    case 4: {  // (0x002c28cc): fire
        const float yaw = atan2_f(fpu_sub(lwc1(tgt + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 0x14), lwc1(m + 0x14)));
        yaw_seek(m + 0x48, pv + 0xdc, yaw, fpu_mul(dt_sq(), u2f(0x41490fdbu)), fpu_mul(dt_sq(), u2f(0x41c90fdbu)), fpu_mul(dt(), u2f(0x40c90fdbu)));
        switch (lbu(m + 0xbc)) {  // (the table L13:0x1f4660)
        case 0:  // (0x002c2bf8)
            step(1);
            return;
        case 1:  // (0x002c2964)
            if (!ended()) return;
            step(2);
            sw(pv + 0xe8, 0);
            return;
        case 2:  // (0x002c29a0)
            if (ended()) step(3);
            return;
        case 3: {  // (0x002c29cc): a shot from the next gun every second, its range growing by 5 up to the target
            if (tick32(pv + 0xe8) != 0) {
                sw(pv + 0xe8, static_cast<uint32_t>(frame_scale_i(0x3c)));
                const uint32_t muzzle = sp + 0x20, aim = sp + 0x30;
                const bool gun1 = lw(pv + 0xf4) != 0;
                moby_chain_point_v(m, gun1 ? 1 : 0, V(muzzle));
                vec3_sub(V(aim), V(tgt + 0x10), V(muzzle));
                vec3_set_length(V(aim), V(aim), fpu_mul(lwc1(L13_ADDR(0x0016149c)), dt()));
                const uint32_t held = lwu(pv + (gun1 ? 0xf8 : 0xfc));
                if (held != 0) {
                    gml_305988_l13(held, aim, tgt, lwc1(pv + 0xec));
                    moby_sound_start(0, 0, m);
                }
                sw(pv + (gun1 ? 0xf8 : 0xfc), gml_3058a8_l13(m, gun1 ? 1 : 0));
                sw(pv + 0xf4, gun1 ? 0u : 1u);
                swc1(pv + 0xec, fpu_add(lwc1(pv + 0xec), 5.0f));
                if (vec2_distance(V(m + 0x10), V(tgt + 0x10)) < lwc1(pv + 0xec)) swc1(pv + 0xec, vec2_distance(V(m + 0x10), V(tgt + 0x10)));
            }
            if (lw(pv + 0xb4) != 2) return;
            anim_to(4, 2);
            sb(m + 0xbc, 4);
            return;
        }
        case 4:  // (0x002c2b40)
            if (ended()) step(5);
            return;
        case 5:  // (0x002c2b6c): the target lost: back down; else fire again
            if (lw(pv + 0xb4) == 2) {
                anim_to(6, 2);
                sb(m + 0xbc, 6);
            } else {
                step(2);
            }
            return;
        case 6:  // (0x002c2bb4)
            if (!ended()) return;
            if (lw(pv + 0xb4) == 2) {
                anim_to(0, 2);
                sb(m + 0x20, 2);
                sb(m + 0xbc, 0);
            } else {
                step(1);
            }
            return;
        default:
            return;
        }
    }
    case 5:  // (0x002c26f8): wait for the target to be seen
        if (lw(pv + 0xb4) == 2) return;
        sb(m + 0xbc, 0);
        sb(m + 0x20, 6);
        return;
    case 6: {  // (0x002c2710): fly to the centre of box +0xd4
        sw(sp + 0x40, 0);
        const uint32_t centre = lvd::box(lvd::BOXES, lw(pv + 0xd4)) + 0x30;
        const float d = vec3_distance(V(m + 0x10), V(centre));
        vec3_sub(V(sp + 0x20), V(lvd::box(lvd::BOXES, lw(pv + 0xd4)) + 0x30), V(m + 0x10));
        const float acc = fpu_mul(dt_sq(), 10.0f);
        seek_1d(sp + 0x40, pv + 0xf0, d, acc, acc, fpu_mul(dt(), 20.0f));
        vec3_set_length(V(sp + 0x20), V(sp + 0x20), lwc1(pv + 0xf0));
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x20));
        if (!(d < 0.1f)) return;
        if (!(lwc1(pv + 0xf0) < 0.01f)) return;
        anim_to(0, 0xa);
        sb(m + 0xbc, 0);
        sb(m + 0x20, 2);
        return;
    }
    case 7:  // (0x002c2cd0): flinch; back to firing at the animation's end
        if (!ended()) return;
        anim_to(lbu(m + 0xbc), 0xa);
        sb(m + 0x20, 4);
        return;
    case 8: {  // (0x002c2c28): explodes with the shots it holds
        vec_clear(V(sp + 0x20));
        explosion_effect(m, sp + 0x20, m + 0x10, 0xa, 3, 0x10, 2, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        if (lwu(pv + 0xf8) != 0) moby_retire(lwu(pv + 0xf8));
        if (lwu(pv + 0xfc) != 0) moby_retire(lwu(pv + 0xfc));
        moby_retire(m);
        return;
    }
    default:  // (1, 3 and beyond 8: nothing)
        return;
    }
}

}  // namespace rac1
