// Quartu (level 15) class 491, ported by hand from the level's own code: a melee robot of the Giant Clank section. It waits
// hidden (while the hero is not Giant Clank) for its group's mates to be gone or the hero in its box, jumps in along its
// entry path, then walks to its place near its target (away from its mates), punches when the target is near, throws
// class 819 charges from further away (six, then a pause), hops between the points of its two jump paths when the target
// keeps away, is knocked back by a hit, and is destroyed with a blast, debris and bolts; state 0x40 clears its moby's saved
// bits and starts it again when the hero is not Giant Clank. While it flies (its entry path, a jump) its four jets smoke
// and their sound plays; else it falls to the floor.
//
// The pvars: the hit record +0x20.. (the health +0x20), the jump's speed +0x40 (vertical +0x48), the colour pulse +0x60..,
// the knock-back's move record +0x70.., the fall speed +0xe8, the target record +0x120 (target_select's 0x48 bytes: the
// moby +0x160, 2 at +0x164 when none is in range), the entry path +0x170, the waiting range +0x174, the range +0x178, the
// two jump paths +0x180 / +0x184 (the one in use +0x1b0, its point +0x1b4), the fist's last place +0x1a0, the range in
// use +0x1b8, the yaw's rate +0x1bc, the timer +0x1c0, the throws made +0x1c4, the knocked-back flag +0x1c8, the group
// +0x1cc, the zone polygon (a path index) +0x1d0, "no jump paths" +0x1d4, "no walking" +0x1d8, the start box +0x1dc, the
// jets' sound slot +0x1e0, the jets' joint node +0x200. The original's order of calls and float operations is kept.
// Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {

constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t HERO_MOBY = 0x001413d0;
constexpr uint32_t HERO_FORM = 0x001413f4;  // (the hero moby's +0x24: 2 while the hero is Giant Clank)
constexpr uint32_t SOUND_SLOTS = 0x0013e550;  // the sound slots, 0x70 bytes each (+0x74 playing, +0x88 the owner)
constexpr int32_t CHARGE_CLASS = 0x350;       // class 848: its hits are not taken

float frame_time() { return lwc1(0x0015ed6c); }
float frame_time2() { return lwc1(0x0015ed70); }
bool hero_is_giant() { return lbu(HERO_FORM) == 2; }
int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }

// its animation `set` over `frames`, unless it is the one playing
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
}

// hidden and still (its collision off)
void hide(uint32_t m) {
    sw(m + 0x94, 0);
    sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
}

// shown and solid (its collision from its class)
void show(uint32_t m) {
    sh(m + 0x34, (lhu(m + 0x34) & 0xffbe) | 0x1000);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
}

// the jets' sound stopped while it still plays for the moby; the slot forgotten
void stop_jets_sound(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0x1e0);
    if (slot != -1) {
        const uint32_t e = SOUND_SLOTS + static_cast<uint32_t>(slot) * 0x70;
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x1e0, 0xffffffffu);
}

// The jump toward `goal` (an EE vector): the horizontal speed +0x40 toward it at the level's jump speed, the vertical speed
// +0x48 that lands it there under the level's gravity.
void jump_toward(uint32_t m, uint32_t pv, uint32_t goal) {
    vec3_sub(V(pv + 0x40), V(goal), V(m + 0x10));
    vec2_set_length(V(pv + 0x40), V(pv + 0x40), fpu_mul(lwc1(L15_ADDR(0x00161b14u)), frame_time()));
    const float gravity = fpu_mul(lwc1(L15_ADDR(0x00161b18u)), frame_time2());
    const float speed = fpu_mul(lwc1(L15_ADDR(0x00161b14u)), frame_time());
    swc1(pv + 0x48, sub_0025abf0(m + 0x10, goal, nullptr, speed, -gravity));
}

// the jump path in use (+0x180 / +0x184 by +0x1b0)
uint32_t jump_path(uint32_t pv) { return lvd::path(lw(pv + 0x180 + (lwu(pv + 0x1b0) << 2))); }

// a jump (state 3) to the next point of the jump path in use
void hop_on(uint32_t m, uint32_t pv) {
    sb(m + 0x20, 3);
    sw(pv + 0x1c0, 0);
    sw(pv + 0x1b4, lwu(pv + 0x1b4) + 1);
    anim_to(m, 3, frame_scale_i(0x14));
    jump_toward(m, pv, jump_path(pv) + 0x10 + (lwu(pv + 0x1b4) << 4));
}

// the point after this one on the jump path in use exists (and the robot has jump paths)
bool can_hop_on(uint32_t pv) { return lw(pv + 0x1d4) == 0 && lw(pv + 0x1b4) < lw(jump_path(pv)) - 1; }

// The yaw seeking the target moby
void face_target(uint32_t m, uint32_t pv, uint32_t tm) {
    const float yaw = atan2_f(fpu_sub(lwc1(tm + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(tm + 0x14), lwc1(m + 0x14)));
    const float rate = fpu_mul(frame_time2(), u2f(0x410ba058));
    yaw_seek(m + 0x48, pv + 0x1bc, yaw, rate, rate, fpu_mul(frame_time(), u2f(0x41490fdb)));
}

// waiting (state 4)
void to_wait(uint32_t m) {
    sb(m + 0x20, 4);
    anim_to(m, 0, frame_scale_i(0x14));
}

// throwing (state 5), its count from 0
void to_throw(uint32_t m, uint32_t pv) {
    sb(m + 0x20, 5);
    anim_to(m, 6, frame_scale_i(0x14));
    sw(pv + 0x1c4, 0);
}

// Near the target: within the level's first range to its place (state 7), within the second to the punch (state 8)
bool close_in(uint32_t m, uint32_t tm) {
    if (vec2_distance(V(m + 0x10), V(tm + 0x10)) < lwc1(L15_ADDR(0x00161b24u))) {
        sb(m + 0x20, 7);
        anim_to(m, 1, frame_scale_i(0xa));
        return true;
    }
    if (vec2_distance(V(m + 0x10), V(tm + 0x10)) < lwc1(L15_ADDR(0x00161b28u))) {
        sb(m + 0x20, 8);
        anim_to(m, 2, frame_scale_i(0xa));
        return true;
    }
    return false;
}

// The jets' smoke: from each of its joints 2 .. 5, ten puffs a frame, blown down along its frame (`sp`: the update's frame)
void jets_smoke(uint32_t m, uint32_t sp) {
    const uint32_t vel = sp + 0x10, at = sp + 0x20;
    for (int pass = 0; pass < 10; ++pass) {
        for (uint32_t joint = 2; joint < 6; ++joint) {
            V(at) = Vec4{};
            const float x = rand_range_f(1.0f, -1.0f);
            swc1(at, fpu_mul(x, fpu_mul(frame_time(), 20.0f)));
            const float y = rand_range_f(1.0f, -1.0f);
            swc1(at + 4, fpu_mul(y, fpu_mul(frame_time(), 20.0f)));
            const float z = rand_range_f(u2f(0x3f4ccccd), u2f(0x3f99999a));
            swc1(at + 8, fpu_mul(z, fpu_mul(frame_time(), -20.0f)));
            lq_sq(vel, at);
            const bool dark = (rand_mod(100) & 1) != 0;
            moby_chain_point_v(m, joint, V(at));
            mat3_transform(V(vel), V(vel), M(m + 0xc0));
            const uint32_t colour = dark ? 0x400f0f7fu : 0x407f7f7fu;
            const float size = fpu_mul(rand_range_f(1.5f, 3.0f), 210000.0f);
            const int32_t life = dark ? rand_range(10, 15) : rand_range(20, 30);
            particle_16_spawn(at, vel, colour, NOT_ADDR(0x00272727u), frame_scale_i(life), size);
        }
    }
}

// Falling (the jets off): the fall speed +0xe8 gaining 30 units/s^2, down to the floor
void fall(uint32_t m, uint32_t pv) {
    const float speed = fpu_add(lwc1(pv + 0xe8), fpu_mul(frame_time2(), 30.0f));
    swc1(pv + 0xe8, speed);
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_sub(speed, 2.0f)));
    const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
    const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
    swc1(m + 0x18, z);
    if (z < floor) {
        swc1(m + 0x18, floor);
        sw(pv + 0xe8, 0);
    }
    moby_sphere_contact(m, m + 0x10, 0x10000, 0, 1, 0, 2.5f, 40.0f, 1.0f);
}

}  // namespace

// Its place near the target (the hero or a moby within `range`, target_select_any's record in its own frame): the target
// itself when no mate of its class is nearer the target, else away from the mates within 3 (weight 6 each) and on a ring
// 10 around the target, averaged and added to its position; its own position when that is within 1. `out` (an EE vector)
// receives it; the yaw toward it (toward the target when it stays) is returned.
// orig: L15:0x002cf110
float qua_2cf110_l15(uint32_t m, uint32_t out, float range) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t away = sp, target = sp + 0x10;  // (target: target_select_any's 0x48-byte record, its point +0)
    const uint32_t pos = m + 0x10;
    bool nearest = true;
    target_select_any(m, target, range);
    float weight = 0.0f;
    const float to_target = vec2_distance(V(pos), V(target));
    vec_clear(V(out));
    for (uint32_t mate = lwu(L15_ADDR(0x0015ffe4u)); mate != 0; mate = lwu(mate + 0x28)) {
        if (mate == m || lh(mate + 0xa6) != lh(m + 0xa6)) continue;
        if (vec2_distance(V(mate + 0x10), V(pos)) < 3.0f) {
            weight = fpu_add(weight, 6.0f);
            vec3_sub(V(away), V(pos), V(mate + 0x10));
            vec3_scale(V(away), V(away), 6.0f);
            vec3_add(V(out), V(out), V(away));
        }
        if (vec2_distance(V(mate + 0x10), V(target)) < to_target) nearest = false;
    }
    if (nearest) {
        lq_sq(out, target);
    } else {
        vec3_sub(V(away), V(pos), V(target));
        float pull = 0.0f;
        if (to_target < 9.5f) {
            pull = 10.0f;
        } else if (10.5f < to_target) {
            pull = -10.0f;
        }
        weight = fpu_add(weight, 10.0f);
        vec3_set_length(V(away), V(away), fpu_mul(pull, 10.0f));
        vec3_add(V(out), V(out), V(away));
        vec3_scale(V(out), V(out), fpu_div(1.0f, weight));
        vec3_add(V(out), V(out), V(pos));
    }
    float yaw;
    if (vec2_distance(V(pos), V(out)) < 1.0f) {
        lq_sq(out, pos);
        yaw = atan2_f(fpu_sub(lwc1(target), lwc1(pos)), fpu_sub(lwc1(target + 4), lwc1(pos + 4)));
    } else {
        yaw = atan2_f(fpu_sub(lwc1(out), lwc1(pos)), fpu_sub(lwc1(out + 4), lwc1(pos + 4)));
    }
    ee_stack_free(0x100);
    return yaw;
}

// Its hits and its target. The hits: the first entry of the Giant Clank hit table (0x178700, 64 of 0x40 bytes) naming it
// with a kind of 0xa0000 that is not the hero's or is a giant hit (0x80000; the hero's others are dropped); the damage off
// its health and the reaction by kind (the table L15:0x1e3150): a pause (9, 10), a knock-back (3 .. 8: state 9), destroyed
// (1, 2, or the health gone: state 10), its colour pulse; not taken from a class 848 charge, in state 10 / 0x40, or when
// the reaction says so. Its scale from its class (0x161b2c meanwhile, 0x161b30 after). The target: in its waiting range
// (state 1) or its range, inside its zone polygon when it has one; +0x164 = 2 when out of range; the hero's moby when none.
// orig: L15:0x002d0fa8
void qua_2d0fa8_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t result = sp + 0x40, damage = sp + 0x44;
    sw(damage, 0);
    swc1(m + 0x2c, fpu_mul(lwc1(L15_ADDR(0x00161b2cu)), lwc1(lwu(m + 0x24) + 0x24)));
    sw(pv + 0x1c8, 0);
    uint32_t hit = 0;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t e = L15_ADDR(0x00178700u) + (i << 6);
        if (lwu(e + 0x34) != m) continue;
        const uint32_t kinds = lwu(e + 0x24);
        if ((kinds & 0xa0000) == 0) continue;
        if (lwu(e + 0x20) != lwu(HERO_MOBY) || (kinds & 0x80000) != 0) {
            hit = e;
            break;
        }
        sw(e + 0x34, 0);
    }
    int32_t st = lw(result);
    float dmg = lwc1(damage);
    const int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    sw(result, static_cast<uint32_t>(st));
    swc1(damage, dmg);
    if (hit != 0) {
        const uint8_t state = lbu(m + 0x20);
        if (lw(result) != 1 && state != 0xa && state != 0x40 && lh(lwu(hit + 0x20) + 0xa6) != CHARGE_CLASS) {
            const float health = fpu_sub(lwc1(pv + 0x20), lwc1(damage));
            swc1(pv + 0x20, health);
            uint32_t kind = lhu(hit + 0x2a) != 0x100 ? static_cast<uint32_t>(react) : 9;
            if (state == 2) kind = 9;
            if (health <= 0.0f) kind = 1;
            sw(pv + 0x90, static_cast<uint32_t>(float_to_int(2560.0f)));
            swc1(pv + 0x98, 2.5f);
            swc1(pv + 0x80, fpu_mul(lwc1(L15_ADDR(0x00161b00u)), frame_time2()));
            swc1(pv + 0x84, u2f(0x3a03126f));
            sw(pv + 0x94, 9);
            sb(pv + 0xad, 0);
            switch (kind) {  // (the table L15:0x1e3150, 12 entries; 0, 11 and past the table: the pulse alone)
                case 9: case 10:  // (0x002d1168) a pause
                    sw(pv + 0x1c8, 1);
                    sb(pv + 0x67, 0xfa);
                    break;
                case 3: case 4: case 5: case 6: case 7: case 8: {  // (0x002d117c) knocked back, away from the hitter
                    const float ft = frame_time();
                    swc1(pv + 0x88, fpu_mul(lwc1(L15_ADDR(0x00161b08u)), ft));
                    swc1(pv + 0x8c, fpu_mul(lwc1(L15_ADDR(0x00161b04u)), ft));
                    sw(pv + 0x1c8, 1);
                    jump_arc_set(pv + 0x70, 4.0f, 1.0f);
                    swc1(pv + 0xc0, 12.0f);
                    swc1(pv + 0xc4, 16.0f);
                    const uint32_t from = lwu(hit + 0x20);
                    float yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(from + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(from + 0x14)));
                    const uint32_t push = sp + 0x20;
                    lq_sq(push, hit + 0x10);
                    float along = lwc1(pv + 0x88), up = lwc1(pv + 0x8c);
                    vec_yaw_marked_v(V(push), yaw, along, up);
                    swc1(sp + 0x48, yaw);
                    swc1(pv + 0x88, along);
                    swc1(pv + 0x8c, up);
                    move_state_start(m, pv + 0x70, 8, 1, 0, lwc1(sp + 0x48));
                    moby_sound_play(0, 0, m, 0x79);
                    sb(m + 0x20, 9);
                    sb(pv + 0x67, 0x78);
                    break;
                }
                case 1: case 2: {  // (0x002d125c) destroyed: a blast at its joint 7, bolts
                    sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                    V(sp + 0x20) = Vec4{};
                    moby_chain_point_v(m, 7, V(sp + 0x30));
                    explosion_effect(m, sp + 0x20, sp + 0x30, 0x14, 6, 0x20, -1, 1, 0x10, -1, 0, 0.0f, 0.0f, 16.0f, 8.0f, 9.0f, 4.0f, 32.0f);
                    moby_sound_play(0, 0, m, 0x79);
                    sb(m + 0x20, 0xa);
                    sb(pv + 0x67, 0xf0);
                    moby_drop_bolts(m, 0x200, -1);
                    break;
                }
                default:  // (0x002d1320)
                    break;
            }
            color_pulse_start(m, pv + 0x60);
        }
        sw(hit + 0x34, 0);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L15_ADDR(0x00161b30u))));
    sw(pv + 0x164, 0);

    const uint32_t target = pv + 0x120;  // (target_select's 0x48-byte record: +0x160 the moby, +0x164 the result)
    if (lbu(m + 0x20) == 1) {
        swc1(pv + 0x1b8, lwc1(pv + 0x174));
        target_select_any(m, target, lwc1(pv + 0x1b8));
    } else if (lw(pv + 0x1d0) == -1) {
        swc1(pv + 0x1b8, lwc1(pv + 0x178));
        target_select_any(m, target, lwc1(pv + 0x1b8));
    } else {
        const float range = lwc1(pv + 0x178);
        swc1(pv + 0x1b8, range);
        const uint32_t zone = lvd::path(lw(pv + 0x1d0));
        target_select(m, target, 0, 0, zone + 0x10, lw(zone), range);
    }
    if (lwc1(pv + 0x1b8) < vec3_distance(V(m + 0x10), V(target))) sw(pv + 0x164, 2);
    if (lwu(pv + 0x160) == 0) sw(pv + 0x160, lwu(HERO_MOBY));
    ee_stack_free(0xb0);
}

// orig: L15:0x002cf3a8
void qua_class491_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x150);
    const auto done = [] { ee_stack_free(0x150); };
    const uint32_t pos = m + 0x10;
    bool jets = false;  // (the original's sp+0x78: flying this frame, its jets on)
    qua_2d0fa8_l15(m);
    {
        const uint8_t st = lbu(m + 0x20);
        if (st != 0 && st != 0x40 && !hero_is_giant()) {  // hidden while the hero is not Giant Clank
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 0x41);
            stop_jets_sound(m, pv);
            sh(m + 0x34, lhu(m + 0x34) & 0xefff);
            sb(pv + 0x58, 0);
            sb(m + 0x20, 1);
            done();
            return;
        }
    }
    const uint32_t tm = lwu(pv + 0x160);  // the target's moby
    joint_node_scale(m, 6, pv + 0x200, 2.5f);

    switch (lbu(m + 0x20)) {  // (the table L15:0x1e3040, 65 entries; 11 .. 63: the tail alone)
        case 0: {  // (0x002cf4d4) set up: its numbers, at its entry path's start, hidden; to state 1
            sh(pv + 0x3e, lhu(pv + 0x3e) | 2);
            swc1(pv + 0x20, 12.0f);
            sh(pv + 0x24, 0xc);
            sb(pv + 0x28, 3);
            swc1(pv + 0x30, 6.0f);
            sb(pv + 0x29, 0);
            sb(m + 0x30, 0xff);
            sh(m + 0x32, 0xff);
            sw(pv + 0x1e0, 0xffffffffu);
            sw(pv + 0x1bc, 0);
            swc1(pv + 0x174, 90.0f);
            if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000);
            hide(m);
            if (lw(pv + 0x180) == -1 || lw(pv + 0x184) == -1) sw(pv + 0x1d4, 1);
            if (lw(pv + 0x170) != -1) lq_sq(pos, lvd::path(lw(pv + 0x170)) + 0x10);
            sb(m + 0x20, 1);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 5);
            break;
        }
        case 1: {  // (0x002cf5d0) waiting: its group's mates gone, or the hero in its box; then the jump in along its entry path
            const int32_t group = lw(pv + 0x1cc);
            if (group != -1) {
                if (moby_group_count_l1(group, 0x40) != 0) break;
                if (hero_is_giant()) show(m);
            } else if (point_in_box(HERO_POS, lw(pv + 0x1dc)) != 0) {
                if (hero_is_giant()) show(m);
            }
            if (lw(pv + 0x164) == 2 || lw(pv + 0x170) == -1) break;
            if (!hero_is_giant() || (lhu(m + 0x34) & 0x1000) == 0) break;
            if (lw(pv + 0x1d0) != -1) {
                const uint32_t zone = lvd::path(lw(pv + 0x1d0));
                if (point_in_polygon(HERO_POS, zone + 0x10, lw(zone)) == 0) {
                    hide(m);
                    break;
                }
            }
            sb(m + 0x20, 2);
            anim_to(m, 4, frame_scale_i(0x14));
            const uint32_t route = lvd::path(lw(pv + 0x170));
            for (int32_t i = 0; i < lw(route) - 2; ++i) {  // each segment's length into its first point's w
                const uint32_t p = route + 0x10 + static_cast<uint32_t>(i) * 0x10;
                swc1(p + 0xc, vec3_distance(V(p), V(p + 0x10)));
            }
            lq_sq(pos, route + 0x10);
            jump_toward(m, pv, route + (lwu(route) << 4));
            break;
        }
        case 2: {  // (0x002cf7e0) along its entry path at 12 a second, two units ahead of its nearest point; at its end, down to state 4
            const uint32_t route = lvd::path(lw(pv + 0x170));
            const float speed = fpu_mul(frame_time(), 12.0f);
            const uint32_t step = sp + 0x10, near = sp + 0x20, idx = sp + 0x70, frac = sp + 0x74;
            int32_t i = lw(idx);
            float t = lwc1(frac);
            path_closest_point_v(route, V(pos), V(near), &i, &t, 0, 999.0f, 5.0f, 0.0f);
            sw(idx, static_cast<uint32_t>(i));
            swc1(frac, t);
            const float len = lwc1(route + 0x1c);
            const float ahead = fpu_add(fpu_add(fpu_mul(int_to_float(lw(idx)), len), lwc1(frac)), 2.0f);
            const int32_t at = float_to_int(fpu_div(ahead, len));
            sw(idx, static_cast<uint32_t>(at));
            const float len2 = lwc1(route + 0x1c);
            swc1(frac, fpu_div(fpu_sub(ahead, fpu_mul(int_to_float(at), len2)), len2));
            const int32_t n = lw(route);
            if (!(lw(idx) < n - 1)) {
                const float floor = floor_probe_v(0.5f, V(pos), 0);
                if (lwc1(m + 0x18) < floor) swc1(m + 0x18, floor);
                sb(m + 0x20, 4);
                sw(pv + 0x1c0, static_cast<uint32_t>(frame_scale_i(0xf)));
                break;
            }
            if (vec3_distance(V(pos), V(route + (static_cast<uint32_t>(n) << 4))) < fpu_div(speed, frame_scale_f(26.0f))) {
                anim_to(m, 5, frame_scale_i(6));
            }
            {
                const uint32_t p = route + (lwu(idx) << 4);
                vec3_sub(V(near), V(p + 0x20), V(p + 0x10));
                vec3_scale(V(near), V(near), lwc1(frac));
                vec3_add(V(near), V(near), V(route + (lwu(idx) << 4) + 0x10));
            }
            const int32_t next = lw(idx) + 1;
            if (next < lw(route) - 8) {
                const uint32_t q = route + (static_cast<uint32_t>(next) << 4);
                const float yaw = atan2_f(fpu_sub(lwc1(q + 0x10), lwc1(pos)), fpu_sub(lwc1(q + 0x14), lwc1(pos + 4)));
                swc1(m + 0x48, angle_seek_rate(pv + 0x1bc, lwc1(m + 0x48), yaw, u2f(0x3c23d70a), u2f(0x3e99999a), u2f(0x3dcccccd)));
            }
            vec3_sub(V(step), V(near), V(pos));
            float move = vec3_length(V(step));
            if (speed < move) move = speed;
            jets = true;
            vec3_set_length(V(step), V(step), move);
            vec3_add(V(pos), V(pos), V(step));
            break;
        }
        case 3: {  // (0x002cfa68) a jump: along its speed, falling under the level's gravity; landing to state 4
            if (lbu(m + 0x53) == 3 && (lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 4, 0, frame_scale_i(6));
            vec3_add(V(pos), V(pos), V(pv + 0x40));
            const float vz = fpu_sub(lwc1(pv + 0x48), fpu_mul(lwc1(L15_ADDR(0x00161b18u)), frame_time2()));
            swc1(pv + 0x48, vz);
            if (vz < 0.0f) {
                const uint32_t probe = sp + 0x10;
                lq_sq(probe, pos);
                swc1(probe + 8, fpu_add(lwc1(probe + 8), 1.0f));
                const float floor = floor_probe_v(0.5f, V(probe), 0);
                const float above = fpu_sub(lwc1(m + 0x18), floor);
                const float k1 = frame_scale_f(26.0f);
                const float k2 = frame_scale_f(26.0f);
                const float k3 = frame_scale_f(26.0f);
                const float drop = fpu_mul(fpu_mul(fpu_mul(fpu_mul(lwc1(L15_ADDR(0x00161b18u)), frame_time2()), 0.5f), k2), k3);
                if (above < fpu_add(fpu_mul(lwc1(pv + 0x48), k1), drop)) anim_to(m, 5, frame_scale_i(6));  // (landing within 26 frames)
                if (lwc1(m + 0x18) < floor) {
                    swc1(m + 0x18, floor);
                    sb(m + 0x20, 4);
                    sw(pv + 0x1c0, static_cast<uint32_t>(frame_scale_i(0xf)));
                }
            }
            jets = true;
            break;
        }
        case 4: {  // (0x002cfbe0) waiting, facing the target: when the timer is out, its place / the punch when near, else throwing
            if (lbu(m + 0x53) != 0 && (lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
            if (lw(pv + 0x164) == 2) break;
            face_target(m, pv, tm);
            if (tick32(pv + 0x1c0) == 0) break;
            if (lw(pv + 0x1d8) != 0 || !close_in(m, tm)) to_throw(m, pv);
            break;
        }
        case 5: {  // (0x002cfd44) throwing class 819 charges at the target: six, then a pause (state 6); near: its place / the punch
            if (lbu(m + 0x53) == 6 && (lbu(m + 0x70) & 2) != 0) {
                moby_anim_set(m, 7, 0, frame_scale_i(5));
                swc1(m + 0x58, lwc1(L15_ADDR(0x00161b34u)));
            }
            if (lw(pv + 0x164) == 2) {
                to_wait(m);
                break;
            }
            face_target(m, pv, tm);
            if (lbu(m + 0x53) == 7 && anim_crossed(m, 0.0f) != 0) {  // the throw, from its joint 0, level
                const uint32_t hand = sp + 0x10, vel = sp + 0x20;
                moby_chain_point_v(m, 0, V(hand));
                vec3_sub(V(vel), V(tm + 0x10), V(hand));
                swc1(vel + 8, fpu_add(lwc1(vel + 8), 0.5f));
                vec3_set_length(V(vel), V(vel), fpu_mul(lwc1(L15_ADDR(0x00161b20u)), frame_time()));
                if (0.0f < lwc1(vel + 8)) swc1(vel + 8, 0.0f);
                if (lwc1(vel + 8) < 0.0f) swc1(vel + 8, 0.0f);
                class819_launch(vel, hand, m, tm, frame_scale_i(lw(L15_ADDR(0x00161b1cu))), 3.0f);
                vec3_set_length(V(vel), V(vel), 3.0f);
                vec3_add_inplace(V(hand), V(vel));
                spark_burst(m, hand, 0);
                sw(pv + 0x1c4, lwu(pv + 0x1c4) + 1);
                break;
            }
            if (6.0f <= static_cast<float>(lw(pv + 0x1c4)) && (lbu(m + 0x70) & 2) != 0) {
                sb(m + 0x20, 6);
                anim_to(m, 0, frame_scale_i(0xa));
                sw(pv + 0x1c0, static_cast<uint32_t>(float_to_int(frame_scale_f(fpu_mul(lwc1(pv + 0x188), 60.0f)))));
                break;
            }
            if (lw(pv + 0x1d8) != 0 || close_in(m, tm)) break;
            // the target keeps away: a jump to the other jump path (the point in use)
            if (!(10.0f < vec2_distance(V(pos), V(tm + 0x10))) || lw(pv + 0x1c8) == 0 || lw(pv + 0x1d4) != 0) break;
            sb(m + 0x20, 3);
            sw(pv + 0x1c0, 0);
            sw(pv + 0x1b0, (lwu(pv + 0x1b0) + 1) & 1);
            anim_to(m, 3, frame_scale_i(0x14));
            jump_toward(m, pv, jump_path(pv) + 0x10 + (lwu(pv + 0x1b4) << 4));
            break;
        }
        case 6: {  // (0x002d0198) the pause after its throws: on along its jump path when the target is within 8, else throwing again
            if (lw(pv + 0x164) == 2) {
                to_wait(m);
                break;
            }
            face_target(m, pv, tm);
            if (lw(pv + 0x1d8) == 0 && vec2_distance(V(pos), V(tm + 0x10)) < 8.0f && can_hop_on(pv)) {
                hop_on(m, pv);
                break;
            }
            if (tick32(pv + 0x1c0) == 0) break;
            to_throw(m, pv);
            break;
        }
        case 7: {  // (0x002d03c0) walking to its place near the target (6.5 a second); stopped (state 4) when the step fails or ends inside the polygon
            const uint32_t from = sp + 0x10, place = sp + 0x20, tmp = sp + 0x30;
            float yaw = atan2_f(fpu_sub(lwc1(tm + 0x10), lwc1(pos)), fpu_sub(lwc1(tm + 0x14), lwc1(pos + 4)));
            if (lw(pv + 0x164) == 2) {
                sb(m + 0x20, 4);
                anim_to(m, 0, frame_scale_i(0x14));
            } else {
                yaw = qua_2cf110_l15(m, place, lwc1(pv + 0x1b8));
                if (vec2_distance(V(pos), V(tm + 0x10)) < lwc1(L15_ADDR(0x00161b28u))) {
                    sb(m + 0x20, 8);
                    anim_to(m, 2, frame_scale_i(0xa));
                }
            }
            yaw_turn_damped(m, pv + 0x1bc, yaw, fpu_mul(frame_time2(), u2f(0x3f860a92)), fpu_mul(frame_time2(), u2f(0x40490fdb)),
                            fpu_mul(frame_time(), u2f(0x40490fdb)));
            lq_sq(from, pos);
            vec3_sub(V(tmp), V(place), V(pos));
            lq_sq(place, tmp);
            vec3_set_length(V(place), V(place), fpu_mul(frame_time(), 6.5f));
            vec3_add_inplace(V(place), V(pos));
            if (walker_step(m, from, place, 1, 2.25f, 1.75f, 0.5f, u2f(0x3f060a92)) != 0) {
                // (the original takes the jump path's alternation word +0x1b0, 0 or 1, as the polygon's path index here)
                const uint32_t zone = lvd::path(lw(pv + 0x1b0));
                if (point_in_polygon(from, zone + 0x10, lw(zone)) == 0) {
                    lq_sq(pos, from);
                    break;
                }
            }
            lq_sq(pos, from);
            sb(m + 0x20, 4);
            anim_to(m, 0, frame_scale_i(0xc));
            break;
        }
        case 8: {  // (0x002d05c4) the punch: the fist (joint 1) swept from its last place between frames 17 and 24; then on or waiting
            face_target(m, pv, tm);
            if ((lbu(m + 0x70) & 2) != 0) {
                if (vec2_distance(V(pos), V(tm + 0x10)) < lwc1(L15_ADDR(0x00161b28u)) && can_hop_on(pv)) {
                    hop_on(m, pv);
                } else if (lwc1(L15_ADDR(0x00161b28u)) < vec2_distance(V(pos), V(tm + 0x10))) {
                    to_wait(m);
                }
                break;
            }
            const float t = moby_anim_time(m);
            const uint32_t rec = sp + 0x10, dir = sp + 0x40, from = sp + 0x50, fist = sp + 0x60;
            moby_chain_point_v(m, 1, V(fist));
            if (!(17.0f <= t) || !(t <= 24.0f)) break;
            lq_sq(from, pos);
            swc1(from + 8, fpu_add(lwc1(from + 8), 0.5f));
            swc1(dir, cos_vu(lwc1(m + 0x48)));
            swc1(dir + 4, sin_vu(lwc1(m + 0x48)));
            swc1(dir + 8, 1.0f);
            // the punch's hit record (0x259888: its direction, the robot, kind 0x10007, strength 4; 9 words, word 6 as the frame
            // holds it), built in the frame as the original does, and handed to the collision routines as a copy
            sub_00259888(4.0f, rec, static_cast<int32_t>(m), 0x10007, dir);
            uint32_t record[9];
            for (uint32_t k = 0; k < 9; ++k) record[k] = lwu(rec + 4 * k);
            mobys_in_sphere_v(V(fist), 2.5f, 9, m, record);
            segment_collide_v(V(from), V(fist), 0, m, record);
            swept_collide(V(from), V(fist), V(from), V(pv + 0x1a0), m, record, 5);
            debug_stub_1f6c28();
            lq_sq(pv + 0x1a0, fist);
            break;
        }
        case 9: {  // (0x002d08f4) knocked back (its move record +0x70); landed: a jump to the other jump path's point (floor height)
            uint32_t moved = 0;
            if (lw(pv + 0x1d8) != 0) {
                swc1(m + 0x58, 1.0f);
            } else {
                moved = enemy_physics(m, pv + 0x70);
            }
            if ((moved & 0x40) == 0 && (lw(pv + 0x1d8) == 0 || (lbu(m + 0x70) & 2) == 0)) break;
            if (lw(pv + 0x1d4) != 0) {
                to_wait(m);
                break;
            }
            sb(m + 0x20, 3);
            sw(pv + 0x1c0, 0);
            const uint32_t which = (lwu(pv + 0x1b0) + 1) & 1;
            sw(pv + 0x1b0, which);
            const uint32_t goal = sp + 0x10;
            lq_sq(goal, lvd::path(lw(pv + 0x180 + (which << 2))) + 0x10 + (lwu(pv + 0x1b4) << 4));
            swc1(goal + 8, floor_probe_v(0.5f, V(goal), 0));
            anim_to(m, 3, frame_scale_i(0x14));
            jump_toward(m, pv, goal);
            break;
        }
        case 10: {  // (0x002d0aa0) destroyed: three debris pieces, a burst; to state 0x40
            const uint32_t vel = sp + 0x10;
            vec3_scale(V(vel), V(m + 0xc0), fpu_mul(lwc1(L15_ADDR(0x00161b38u)), lwc1(0x0015ed60)));
            swc1(vel + 8, fpu_add(lwc1(vel + 8), fpu_mul(lwc1(L15_ADDR(0x00161b3cu)), lwc1(0x0015ed60))));
            const uint32_t spin = L15_ADDR(0x0015f580u);
            debris_spawn(m, 0x64a, pos, m + 0x40, frame_scale_i(0x5a), 0, vel, spin, spin, fpu_mul(frame_time2(), 12.0f));
            debris_spawn(m, 0x64b, pos, m + 0x40, frame_scale_i(0x5a), 0, vel, spin, spin, fpu_mul(frame_time2(), 12.0f));
            debris_spawn(m, 0x64c, pos, m + 0x40, 0, 0, spin, spin, spin, 0.0f);
            debris_burst(m, 0x7a9, 1, 0x7a9, 2, 4, 2);
            sb(m + 0x20, 0x40);
            done();
            return;
        }
        case 64: {  // (0x002d0bc0) gone: its moby's saved bits cleared; when the hero is not Giant Clank, set up again (state 0)
            hide(m);
            sb(pv + 0x58, 0);
            const bool giant = hero_is_giant();
            const int32_t uid = lh(m + 0xb2);
            sb(L15_ADDR(0x001babd0u) + static_cast<uint32_t>(uid) + 0x454, 0);
            sb(L15_ADDR(0x001bb830u) + static_cast<uint32_t>(uid) + 0x454, 0);
            const uint32_t bit = 1u << (lhu(m + 0xb2) & 0x1f);
            const uint32_t word = static_cast<uint32_t>((uid >> 5) << 2);
            const uint32_t saved = 0x0014c190u + (lwu(0x0015ed84u) << 8) + word;
            sw(saved, lwu(saved) & ~bit);
            const uint32_t flags = L15_ADDR(0x001baad0u) + word;
            sw(flags, lwu(flags) & ~bit);
            if (!giant) sb(m + 0x20, 0);
            break;
        }
        default:  // (0x002d0c90)
            break;
    }

    // the tail (0x002d0c90): flying, the jets' sound kept playing and their smoke; else the sound stopped and the fall
    if (jets) {
        if (sub_0028d8c0(m, lw(pv + 0x1e0)) == 0) sw(pv + 0x1e0, static_cast<uint32_t>(moby_sound_start(7, 4, m)));
        jets_smoke(m, sp);
    } else {
        stop_jets_sound(m, pv);
        const uint8_t st = lbu(m + 0x20);
        if (st >= 2 && st != 0x40) fall(m, pv);
    }
    done();
}

}  // namespace rac1
