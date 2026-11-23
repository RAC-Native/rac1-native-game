// Orxon's own moby classes (level 10): class 1229 (a flying gun drone on its paths) and its frame helper. Header
// orxon_classes.h, registered in orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/orxon_classes.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/shared_class827.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

// Class 1229's frame part: its hits (mask 0x370000, pvars +0x20 its health) and its target. A hit that counts takes the
// damage off its health (the health stored whatever it gives); no health left, or a hit carrying 0x40000, is reaction 1.
// By the reaction (the table at L10:0x001dd410): 1 dies (8), 2 .. 8 and 11 (and any other) knocked (7, the state it
// left in +0xbc, animation 4, still for 60 frames), 9, 10 and 12 a pulse alone, 13 nothing. Then its target (pvars
// +0xd0, the moby at +0x110, +0x114 2 when it has none): with the hero as Clank the nearest of class 827's picks within
// 101 when it is inside the area of path +0x190, else the targets along that path; none: the hero.
// orig: L10:0x002e5968
void orx_2e5968_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x370000u), 0);
    int32_t state = 0;
    float dmg = 0.0f;
    int32_t reaction = moby_hit_react_r(m, hit, pv + 0x20, 5, state, &dmg, nullptr, 4);
    const uint32_t pulse = pv + 0xc0;
    if (hit != 0 && state != 1 && lbu(m + 0x20) != 8) {
        const float health = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, health);
        if (health <= 0.0f) reaction = 1;
        if ((lwu(hit + 0x24) & 0x40000u) != 0) reaction = 1;
        switch (reaction) {  // (the table at L10:0x001dd410, by reaction - 1)
        case 1:  // dies (0x002e5a58)
            sb(pv + 0xc7, 0xfa);
            color_pulse_start(m, pulse);
            sb(m + 0x20, 8);
            break;
        case 9: case 10: case 12:  // a pulse (0x002e5a3c)
            sb(pv + 0xc7, 0xfa);
            color_pulse_start(m, pulse);
            break;
        case 13:  // nothing (0x002e5ac4)
            break;
        default: {  // 2 .. 8, 11 and outside the table: knocked (0x002e5a78)
            sb(pv + 0xc7, 0xfa);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
            color_pulse_start(m, pulse);
            moby_anim_set(m, 4, 0, frame_scale_i(6));
            const uint32_t was = lbu(m + 0x20);
            sb(m + 0x20, 7);
            sb(m + 0xbc, was);
            break;
        }
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
    if (lbu(HERO_FORM) == 1) {
        class827_target_pick(m, pv + 0x110, 101.0f);
        const uint32_t t = lwu(pv + 0x110);
        if (t == 0) {
            sw(pv + 0x114, 2);
        } else {
            const uint32_t area = lvd::path(lw(pv + 0x190));
            if (point_in_polygon(t + 0x10, area + 0x10, lw(area)) != 0) {
                sw(pv + 0x114, 0);
            } else {
                sw(pv + 0x110, 0);
                sw(pv + 0x114, 2);
            }
        }
    } else {
        const uint32_t area = lvd::path(lw(pv + 0x190));
        if (target_select(m, pv + 0xd0, 0, 0, area + 0x10, lw(area), 101.0f) != 2 && lbu(m + 0x31) == 0) sw(pv + 0x114, 2);
    }
    if (lwu(pv + 0x110) == 0) sw(pv + 0x110, lwu(HERO + 0x2080));
}

namespace {
// the point (frame +0x20, its height +0x28) class 1229 aims at in states 4 and 5: its target's place, or (the hero as
// Ratchet) its pvars' point +0xf0 with a line to it tested (the moby it meets in the collision record); as Clank the
// floor under the hero's place 20 frames on when the target is the hero, 0.2 above; the collision record's moby is then
// the target
void aim_point(uint32_t m, uint32_t pv, uint32_t target, uint32_t frame) {
    const uint32_t aim = frame + 0x20;
    lq_sq(aim, target + 0x10);
    if (lbu(HERO_FORM) != 1) {
        lq_sq(aim, pv + 0xf0);
        segment_collide_v(V(m + 0x10), V(aim), 0, m, nullptr);
        return;
    }
    if (lwu(HERO + 0x2080) == target) {
        const uint32_t ahead = frame + 0x40, step = frame + 0x30;
        vec3_scale(V(ahead), V(HERO_POS + 0x80), int_to_float(frame_scale_i(0x14)));
        lq_sq(step, ahead);
        vec3_add_inplace(V(aim), V(step));
        swc1(aim + 8, floor_probe_v(0.5f, V(aim), 0));
    }
    coll_hit_set_moby(target);
    swc1(aim + 8, fpu_add(lwc1(aim + 8), u2f(0x3e4ccccdu)));
}

// class 1229 turned towards its target (yaw) and pitched towards `at` (the point, its height)
void face(uint32_t m, uint32_t pv, uint32_t target, uint32_t at, float z, float yaw_rate) {
    yaw_turn_damped(m, pv + 0x198, atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x14), lwc1(m + 0x14))),
                    u2f(0x3ca3d70au), u2f(0x3e99999au), fpu_mul(dt(), yaw_rate));
    const float d = vec2_distance(V(m + 0x10), V(at));
    swc1(m + 0x44, angle_seek_rate(pv + 0x19c, lwc1(m + 0x44), -atan2_f(d, fpu_sub(z, lwc1(m + 0x18))), u2f(0x3ca3d70au), u2f(0x3e99999au),
                                   fpu_mul(dt(), u2f(0x40490fdbu))));
}

// the countdown +0x168 set to `seconds` (a pvar float) of frames
void wait_set(uint32_t pv, float seconds) { sw(pv + 0x168, static_cast<uint32_t>(float_to_int(frame_scale_f(fpu_mul(seconds, 60.0f))))); }
}  // namespace

// Class 1229: a gun drone. 0 starts it (health 9, at the first point of its path +0x140, its wait +0x196 the level's
// L10:0x161ee4 frames; gone for good: retired); 1 waits for its target inside box +0x148 (then rises, 2; the help
// message 0x2715 once, unless a scene, the hero as Clank or the message's flag); 2 rises by 4 a second to its path's
// first point (then 3); 3 flies along path +0x140 at 8 a second (its end: 4, waiting +0x160 seconds); 4 and 5 hover along
// path +0x144 (8 a second) facing the target, 4 shooting it (class 819's shots at its hands at the animation's cues 1
// and 16, once its aim is within 10 degrees and the line to the target is clear) and 5 resting, each for its pvars'
// seconds (+0x160 / +0x164); the hero inside box +0x1b0 sends it off (6: up, shrinking, gone at 250 high or 0.01 size);
// 7 knocked (back to the state it was hit in); 8 explodes (debris 0x5fd and 0x5fe, bolts). The floor band near the
// camera. No path: a debug print and gone.
// orig: L10:0x002e4a88
void orx_class1229_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    joint_node_scale(m, 2, pv + 0x1d0, 2.5f);
    if (lw(pv + 0x140) == -1) {
        debug_printf_stub();
        moby_retire(m);
        return;
    }
    orx_2e5968_l10(m);
    const uint32_t target = lwu(pv + 0x110);
    const uint32_t frame = ee_stack_alloc(0x70);
    bool gone = false;
    // the path follow of states 4 and 5: towards point +0x158 of path +0x144 at 8 a second, the next point once there
    auto hover = [&] {
        const uint32_t path = lvd::path(lw(pv + 0x144));
        const uint32_t pt = frame + 0x30, step = frame + 0x40;
        lq_sq(pt, path + 0x10 + (lwu(pv + 0x158) << 4));
        vec3_sub(V(step), V(pt), V(m + 0x10));
        vec3_clamp_length(step, fpu_mul(dt(), 8.0f));
        vec3_add(V(m + 0x10), V(m + 0x10), V(step));
        if (!(vec3_length(V(step)) < dt())) return;
        const int32_t next = lw(pv + 0x158) + 1;
        if (next < lw(path)) sw(pv + 0x158, static_cast<uint32_t>(next));
    };
    switch (lbu(m + 0x20)) {  // (the table at L10:0x001dd3e0)
    case 0: {  // (0x002e4b34)
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        swc1(pv + 0x20, 9.0f);
        sh(pv + 0x24, static_cast<uint32_t>(float_to_int(9.0f)));
        sb(pv + 0x28, 3);
        sb(pv + 0x29, 0);
        sw(pv + 0x19c, 0);
        sh(m + 0x32, 0x7f);
        lq_sq(m + 0x10, lvd::path(lw(pv + 0x140)) + 0x10);
        sh(pv + 0x196, static_cast<uint32_t>(frame_scale_i(lw(L10_ADDR(0x00161ee4)))));
        sw(m + 0x94, 0);
        const uint32_t anim = lbu(m + 0x53);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        if (anim != 0) moby_anim_set(m, 0, 0, 0);
        sb(m + 0x20, 1);
        if (gone_byte(m) == 0xff) gone = true;
        break;
    }
    case 1:  // waiting (0x002e4c10)
        if (point_in_box(target + 0x10, lw(pv + 0x148)) == 0) break;
        sb(m + 0x20, 2);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        if (lw(OV(0x00179410)) != 0 || lw(OV(0x00179434)) != -1 || lbu(HERO_FORM) != 0 || lhu(0x00141c48) != 0 || lbu(0x0013d4cb) != 0) break;
        sub_00203908(0x2715, 0x5c);
        break;
    case 2: {  // rising (0x002e4ccc)
        yaw_turn_damped(m, pv + 0x198, atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x14), lwc1(m + 0x14))),
                        u2f(0x3ca3d70au), u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40490fdbu)));
        const float d = vec2_distance(V(m + 0x10), V(target + 0x10));
        const float pitch = angle_seek_rate(pv + 0x19c, lwc1(m + 0x44), -atan2_f(d, fpu_sub(lwc1(target + 0x18), lwc1(m + 0x18))), u2f(0x3ca3d70au),
                                            u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40490fdbu)));
        swc1(m + 0x44, pitch);
        if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) == 1) moby_anim_set(m, 2, 0, frame_scale_i(0x14));
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(dt(), 4.0f)));
        if (!(vec3_distance(V(m + 0x10), V(lvd::path(lw(pv + 0x140)) + 0x10)) < 0.5f)) break;
        sb(m + 0x20, 3);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(10));
        break;
    }
    case 3: {  // along path +0x140 (0x002e4e4c)
        const uint32_t path = lvd::path(lw(pv + 0x140));
        const uint32_t pt = frame + 0x20, to = frame + 0x30;
        lq_sq(pt, path + 0x10 + (lwu(pv + 0x158) << 4));
        yaw_turn_damped(m, pv + 0x198, atan2_f(fpu_sub(lwc1(pt), lwc1(m + 0x10)), fpu_sub(lwc1(pt + 4), lwc1(m + 0x14))), u2f(0x3ca3d70au),
                        u2f(0x3e99999au), fpu_mul(dt(), u2f(0x41490fdbu)));
        const float d = vec2_distance(V(m + 0x10), V(target + 0x10));
        swc1(m + 0x44, angle_seek_rate(pv + 0x19c, lwc1(m + 0x44), -atan2_f(d, fpu_sub(lwc1(target + 0x18), lwc1(m + 0x18))), u2f(0x3ca3d70au),
                                       u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40490fdbu))));
        vec3_sub(V(to), V(pt), V(m + 0x10));
        vec3_set_length(V(pv + 0x120), V(to), fpu_mul(dt(), 8.0f));
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x120));
        if (!(vec3_length(V(to)) < 0.5f)) break;
        const int32_t next = lw(pv + 0x158) + 1;
        if (next < lw(path)) {
            sw(pv + 0x158, static_cast<uint32_t>(next));
            break;
        }
        sw(pv + 0x158, 0);
        sb(m + 0x20, 4);
        wait_set(pv, lwc1(pv + 0x160));
        break;
    }
    case 4: {  // shooting (0x002e4fec)
        if (point_in_box(HERO_POS, lw(pv + 0x1b0)) != 0) {
            sb(m + 0x20, 6);
            break;
        }
        aim_point(m, pv, target, frame);
        face(m, pv, target, frame + 0x20, lwc1(frame + 0x28), u2f(0x41490fdbu));
        if (angle_dist(lwc1(m + 0x48), atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x14), lwc1(m + 0x14)))) <
            u2f(0x3e32b8c2u)) {
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(6));
            swc1(m + 0x58, lwc1(L10_ADDR(0x00161eec)));
        }
        if (lbu(m + 0x52) == 3) {
            if (lw(pv + 0x114) != 2 && coll_hit_moby() == target) {
                const int32_t at1 = anim_crossed(m, 1.0f);
                const int32_t at16 = anim_crossed(m, 16.0f);
                if (at1 != 0 || at16 != 0) {  // a shot from its hand (joint 1 at the first cue, 0 at the second)
                    const uint32_t hand = frame + 0x40, dir = frame + 0x30, flash = frame + 0x50;
                    moby_chain_point_v(m, at1 != 0 && at16 == 0 ? 1u : 0u, V(hand));
                    vec_from_spherical_v(V(dir), 1.0f, lwc1(m + 0x48), -lwc1(m + 0x44));
                    vec3_add_inplace(V(hand), V(dir));
                    vec3_sub(V(dir), V(frame + 0x20), V(hand));
                    vec3_set_length(V(dir), V(dir), fpu_mul(lwc1(L10_ADDR(0x00161ee8)), dt()));
                    vec3_scale(V(flash), V(dir), frame_scale_f(3.0f));
                    vec3_add_inplace(V(flash), V(hand));
                    spark_burst(m, flash, 0);
                    class819_launch(dir, hand, m, target, frame_scale_i(0x12c), 1.0f);
                }
            } else if (lbu(m + 0x53) != 2) {
                moby_anim_set(m, 2, 0, frame_scale_i(10));
            }
        }
        if (tick32(pv + 0x168) != 0) {  // its time up: resting (5)
            if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(10));
            sb(m + 0x20, 5);
            wait_set(pv, lwc1(pv + 0x164));
        }
        hover();
        break;
    }
    case 5:  // resting (0x002e53c8)
        if (point_in_box(HERO_POS, lw(pv + 0x1b0)) != 0) {
            sb(m + 0x20, 6);
            break;
        }
        aim_point(m, pv, target, frame);
        face(m, pv, target, frame + 0x20, lwc1(frame + 0x28), u2f(0x41490fdbu));
        if (tick32(pv + 0x168) != 0) {  // its rest over: shooting again (4)
            moby_anim_set(m, 3, 0, frame_scale_i(10));
            sb(m + 0x20, 4);
            swc1(m + 0x58, lwc1(L10_ADDR(0x00161eec)));
            wait_set(pv, lwc1(pv + 0x160));
        }
        hover();
        break;
    case 6: {  // sent off: up and shrinking (0x002e5690)
        const float shrink = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbc7aacc0u)), 1.0f);
        const float z = fpu_add(lwc1(m + 0x18), fpu_mul(dt(), 10.0f));
        const float size = fpu_mul(lwc1(m + 0x2c), shrink);
        swc1(m + 0x18, z);
        swc1(m + 0x2c, size);
        if (250.0f < z || size < u2f(0x3c23d70au)) gone = true;
        break;
    }
    case 7: {  // knocked: back to the state it was hit in (0x002e5718)
        if (lbu(m + 0x52) != 4 || (lbu(m + 0x70) & 2) == 0) break;
        const uint32_t was = lbu(m + 0xbc);
        if (was == 4) {
            moby_anim_set(m, 3, 0, frame_scale_i(10));
            sb(m + 0x20, 4);
            swc1(m + 0x58, lwc1(L10_ADDR(0x00161eec)));
            wait_set(pv, lwc1(pv + 0x160));
        } else if (was == 6) {
            moby_anim_set(m, 2, 0, frame_scale_i(10));
            sb(m + 0x20, 6);
        } else {
            moby_anim_set(m, 2, 0, frame_scale_i(10));
            sb(m + 0x20, 5);
            wait_set(pv, lwc1(pv + 0x164));
        }
        break;
    }
    case 8: {  // explodes (0x002e57f8)
        explosion_effect(m, pv + 0x40, m + 0x10, 5, 2, 4, 5, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        const uint32_t shared = OV(0x0015f580);  // (the engine's block the pieces take their speed, spin and size from)
        moby_drop_bolts(m, 0, -1);
        debris_spawn(m, 0x5fd, m + 0x10, m + 0x40, 0, 0, shared, shared, shared, 0.0f);
        debris_spawn(m, 0x5fe, m + 0x10, m + 0x40, 0, 0, shared, shared, shared, 0.0f);
        gone = true;
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x70);
    if (gone) {
        moby_retire(m);
        return;
    }
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
}

}  // namespace rac1
