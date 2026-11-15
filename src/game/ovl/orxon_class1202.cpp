// Orxon's own moby classes (level 10): class 1202 and its helpers. Header orxon_classes.h, registered in orxon_mobys.cpp;
// level 10's own addresses (`orig: L10:`).
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/metal.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/walker.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/shared_spawns.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

// Class 1202's exhaust: L10:0x161e74 puffs a frame, each a burst of L10:0x161e78 smoke particles (particle 0x02) from a
// point jittered about the moby (0.55 up; lower and slower while it falls, state 11), thrown at a random heading (the
// level's speed and lift ranges) plus half its velocity (pvar +0x70), falling by the level's gravity; their sizes,
// colours (two blends of the level's pairs), lives and textures from the level's table L10:0x161e7c .. 0x161ebc.
// orig: L10:0x002e3a90
void orx_2e3a90_l10(uint32_t m) {
    const int32_t puffs = lw(L10_ADDR(0x00161e74));
    const uint32_t pv = lwu(m + 0x78);
    if (puffs <= 0) return;
    const uint32_t frame = ee_stack_alloc(0x70);
    const uint32_t pos = frame + 0x10, vel = frame + 0x20, end = frame + 0x30, half = frame + 0x40;
    const float up = u2f(0x3f0ccccdu);  // (0.55)
    for (int32_t i = 0; i < puffs; ++i) {
        lq_sq(pos, m + 0x10);
        metal_25f8e0(pos, lwc1(L10_ADDR(0x00161e7c)));
        swc1(pos + 8, fpu_add(lwc1(pos + 8), up));
        const float heading = rand_angle();
        float speed = rand_range_f(lwc1(L10_ADDR(0x00161e88)), lwc1(L10_ADDR(0x00161e8c)));
        float lift = rand_range_f(lwc1(L10_ADDR(0x00161e90)), lwc1(L10_ADDR(0x00161e94)));
        if (lbu(m + 0x20) == 11) {
            speed = fpu_mul(speed, up);
            lift = fpu_mul(lift, up);
            swc1(pos + 8, fpu_sub(lwc1(pos + 8), u2f(0x3e4ccccdu)));
        }
        swc1(vel, fpu_mul(cos_vu(heading), fpu_mul(speed, dt())));
        const float s = sin_vu(heading);
        swc1(vel + 8, fpu_add(fpu_mul(lift, dt()), 0.0f));
        swc1(vel + 4, fpu_mul(s, fpu_mul(speed, dt())));
        vec3_scale(V(half), V(pv + 0x70), 0.5f);
        swc1(half + 8, 0.0f);
        vec3_add(V(vel), V(vel), V(half));
        lq_sq(end, vel);
        const float frames = static_cast<float>(frame_scale_i(lw(L10_ADDR(0x00161eb8))));
        swc1(end + 8, fpu_sub(lwc1(end + 8), fpu_mul(fpu_mul(lwc1(L10_ADDR(0x00161e98)), dt2()), frames)));
        for (int32_t k = 0; k < lw(L10_ADDR(0x00161e78)); ++k) {
            const float r = rand_range_f(0.5f, 1.5f);
            swc1(vel + 0xc, fpu_mul(r, lwc1(L10_ADDR(0x00161e9c))));
            swc1(end + 0xc, fpu_mul(r, lwc1(L10_ADDR(0x00161ea0))));
            metal_25f8e0(pos, lwc1(L10_ADDR(0x00161e80)));
            metal_25f8e0(end, fpu_mul(lwc1(L10_ADDR(0x00161e84)), dt()));
            const uint32_t c1 = rgba_lerp(lwu(L10_ADDR(0x00161ea4)), lwu(L10_ADDR(0x00161eac)), rand_range_f(0.0f, 1.0f));
            const uint32_t c2 = rgba_lerp(lwu(L10_ADDR(0x00161ea8)), lwu(L10_ADDR(0x00161eb0)), rand_range_f(0.0f, 1.0f));
            const uint32_t life1 = static_cast<uint32_t>(frame_scale_i(lw(L10_ADDR(0x00161eb4))));
            const uint32_t life2 = static_cast<uint32_t>(frame_scale_i(lw(L10_ADDR(0x00161eb8))));
            const float t = static_cast<float>(lw(L10_ADDR(0x00161ebc)));
            const uint32_t size = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(fpu_mul(t, 0.5f), fpu_mul(t, 2.5f)))));
            spawn_particle_02(pos, vel, end, c1, c2, life1, life2, size, -1);
        }
    }
    ee_stack_free(0x70);
}

// Class 1202's frame before its state. Waking: when the hero's state +0x10b8 is 0x15 with the moby +0x1090 whose pvar
// +0x10 is this one (its group woken; state 1 -> 2), or when +0x38 asks (its group woken, the alert timer +0x1ac re-armed
// to 180 .. 240 frames). A hit (mask 0xb30000, moby_hit_react_r with its damage word) unless the reaction state says
// none or it is knocked down (10 .. 12): on level 10 a hit by class 0x31a while alerted plays the level's first such
// message (the game's byte 0x13d419); while it is still pulsing (+0x26) a weak hit (< 3) does nothing and any other is
// a dud; the damage is capped at 2 while the hero is in states 0x20; a hit of its own class, by the hero as Clank while it
// is in animations 6 / 7, or blocked (the hero's shot 0x47 behind a wall, another shot across a wall face-on) does
// nothing; the damage comes off its health +0x20 (1: dead). Its group woken, its knock-back set, and by the reaction
// (the table at L10:0x001dd360): 3, 6 .. 10 knocked away high (10), 4 / 5 knocked away low (10; higher unless the hero is
// Clank), 1 / 2 down (11). Then, unless knocked down: its range +0x1a0 (alerted: L10:0x161e40 plus 6; state 1: 6 when
// the hero has it in +0x23c / +0x240, else 0, and as Clank the hero wakes its group; else L10:0x161e40), its target
// (+0x120, from +0x170: alerted any target, +0x164 = 2 when it is beyond its range or 3 below / above; else a target
// along its path +0x1b0 while the old one is near, any target otherwise); 300 frames with +0x164 = 2 send it home (1,
// far from +0x170: 8). +0x160 defaults to the hero.
// orig: L10:0x002e3ef0
void orx_2e3ef0_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(0x00140408) == 0x15 && lwu(lwu(lwu(0x001403e0) + 0x78) + 0x10) == m) {
        group_wake(m);
        if (lbu(m + 0x20) == 1) sb(m + 0x20, 2);
    }
    if (lw(pv + 0x38) != 0) {
        group_wake(m);
        sw(pv + 0x1ac, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    tick32(pv + 0x1ac);
    const uint32_t frame = ee_stack_alloc(0x60);
    const uint32_t state_w = frame + 0x50, damage_w = frame + 0x54, yaw_w = frame + 0x58;
    sw(state_w, 1);
    sw(damage_w, 0);
    const uint32_t hit = sub_0025a420(m, 0xb30000u, 0);
    int32_t reaction;
    {
        int32_t st = lw(state_w);
        float dmg = lwc1(damage_w);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(state_w, static_cast<uint32_t>(st));
        swc1(damage_w, dmg);
    }
    tick16(pv + 0x26);
    const uint32_t pulse = pv + 0x60;
    const uint32_t st0 = lbu(m + 0x20);
    if (!(lwu(state_w) < 2u) && st0 != 11 && st0 != 12) {
        if (level_number() == 10 && hit != 0 && lwu(hit + 0x20) != 0 && lh(lwu(hit + 0x20) + 0xa6) == 0x31a && lw(pv + 0x1ac) != 0 &&
            lbu(0x0013d419) == 0) {
            sb(0x0013d419, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        if (lh(pv + 0x26) != 0 && lbu(hit + 0x28) == 0) {  // (still pulsing: a dud, a weak one nothing)
            reaction = 11;
            if (lwc1(damage_w) < 3.0f) sw(damage_w, 0);
        }
        if (lw(HERO + 0x2084) == 0x20 || (lw(HERO + 0x2090) == 0x20 && lw(HERO + 0x198) < frame_scale_i(0x1e))) {
            if (2.0f < lwc1(damage_w)) swc1(damage_w, 2.0f);
        }
        auto nothing = [&] {
            sw(damage_w, 0);
            reaction = 11;
        };
        if (static_cast<int32_t>(lhu(hit + 0x2a)) == lh(m + 0xa6)) nothing();
        if (lbu(m + 0x53) - 6u < 2u && lbu(HERO_FORM) == 1 && lwu(hit + 0x20) == lwu(HERO + 0x2080)) nothing();
        const uint32_t from = frame, to = frame + 0x10;
        if (lhu(hit + 0x2a) == 0x47) {  // the hero's shot: a wall between him and it
            lq_sq(to, m + 0x10);
            lq_sq(from, HERO_POS);
            swc1(to + 8, fpu_add(lwc1(to + 8), 1.0f));
            swc1(from + 8, fpu_add(lwc1(from + 8), 1.0f));
            if (segment_collide_v(V(from), V(to), 2, m, nullptr) != 0) {
                reaction = 11;
                sw(damage_w, 0);
            }
        } else {
            const uint32_t src = lwu(hit + 0x20);
            if (src != 0 && lh(src + 0xa6) != 0x359) {  // another shot across a wall, face-on
                lq_sq(to, m + 0x10);
                lq_sq(from, src + 0x10);
                swc1(to + 8, fpu_add(lwc1(to + 8), 1.0f));
                const int32_t cls = lh(lwu(hit + 0x20) + 0xa6);
                if (cls == 0xba || cls == 0) swc1(from + 8, fpu_add(lwc1(from + 8), 0.25f));
                if (segment_collide_v(V(from), V(to), 0x12, m, nullptr) != 0) {
                    const uint32_t d = frame + 0x40, n1 = frame + 0x20, n2 = frame + 0x30;
                    vec3_sub(V(d), V(to), V(from));
                    lq_sq(n1, d);
                    vec3_set_length(V(n1), V(n1), 1.0f);
                    vec3_set_length(V(n2), V(OV(0x00173e80)), 1.0f);  // (the hit record's normal)
                    if (0.5f < abs_f(vec3_dot(V(n1), V(n2)))) nothing();
                }
            }
        }
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(damage_w));
        swc1(pv + 0x20, health);
        if (health <= 0.0f) reaction = 1;
        group_wake(m);
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(768.0f)));
        swc1(pv + 0x98, 0.75f);
        sw(pv + 0x94, 9);
        sb(pv + 0xad, 0);
        swc1(yaw_w, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
        auto knock = [&](float height, float out, float up, bool marked_yaw) {
            sb(m + 0x20, 10);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
            swc1(pv + 0x80, fpu_mul(lwc1(L10_ADDR(0x00161e2c)), dt2()));
            jump_arc_set(pv + 0x70, height, 0.5f);
            swc1(pv + 0xc0, out);
            swc1(pv + 0xc4, up);
            const Vec4 dir = V(hit + 0x10);
            vec_yaw_marked_v(dir, *static_cast<float*>(ee_ram_ptr(yaw_w)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            const float yaw = marked_yaw ? lwc1(yaw_w) : atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14));
            move_state_start(m, pv + 0x70, 9, 1, 0, yaw);
            sb(pv + 0x67, 0xfa);
            color_pulse_start(m, pulse);
        };
        switch (static_cast<uint32_t>(reaction)) {
        case 3: case 6: case 7: case 8: case 9: case 10:  // (0x002e4390)
            knock(0.5f, 8.0f, 10.0f, true);
            break;
        case 4: case 5:  // (0x002e4414)
            knock(lbu(HERO_FORM) == 1 ? 1.0f : 3.0f, 3.5f, 7.0f, false);
            break;
        case 1: case 2:  // (0x002e44f0)
            sb(m + 0x20, 11);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            break;
        default:  // 0, 11 and up (0x002e4508)
            break;
        }
    }
    ee_stack_free(0x60);
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
    const uint32_t st = lbu(m + 0x20);
    if (st - 10u < 3u) {
        sw(pv + 0x1d4, 0);
        return;
    }
    const int32_t alert = lw(pv + 0x1ac);
    if (alert != 0) {
        swc1(pv + 0x1a0, fpu_add(lwc1(L10_ADDR(0x00161e40)), 6.0f));
    } else if (st != 1) {
        swc1(pv + 0x1a0, lwc1(L10_ADDR(0x00161e40)));
    } else {
        if (m == lwu(HERO + 0x240) || m == lwu(HERO + 0x23c)) swc1(pv + 0x1a0, 6.0f);
        else sw(pv + 0x1a0, 0);
        if (lbu(HERO_FORM) == 1) group_wake(m);
    }
    const uint32_t target = pv + 0x120, from = pv + 0x170;
    if (alert != 0) {
        target_select_any(m, target, lwc1(pv + 0x1a0));
        if (lwc1(pv + 0x1a0) < vec2_distance(V(from), V(target)) || 3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128)))) sw(pv + 0x164, 2);
    } else if (lwc1(pv + 0x1a0) < vec2_distance(V(from), V(target)) || 3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128)))) {
        target_select_any(m, target, lwc1(pv + 0x1a0));
        sw(pv + 0x164, 2);
    } else {
        const uint32_t path = lvd::path(lw(pv + 0x1b0));
        target_select(m, target, 0, 0, path + 0x10, lw(path), lwc1(pv + 0x1a0));
    }
    if (lbu(m + 0x20) == 1 || lw(pv + 0x164) != 2) {
        sw(pv + 0x1d4, 0);
    } else {
        sw(pv + 0x1d4, lwu(pv + 0x1d4) + 1);
        if (frame_scale_i(0x12c) < lw(pv + 0x1d4)) {  // lost it for 300 frames: home
            if (1.0f < vec3_distance(V(m + 0x10), V(from))) {
                sb(m + 0x20, 8);
                if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, frame_scale_i(0xc));
            } else if (lbu(HERO_FORM) != 1) {
                sb(m + 0x20, 1);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
            }
        }
    }
    if (lw(pv + 0x160) == 0) sw(pv + 0x160, lwu(HERO + 0x2080));
}

// Class 1202's heading away from its leader (+0x160) into the widest gap between its group's mobys (+0x21; 0xff: straight
// away): the nearest bearing on either side among the members nearer the leader than it is (or in state 5), within the
// level's half-angle L10:0x161e48 degrees, and the middle of that gap.
// orig: L10:0x002e4840
float orx_2e4840_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t leader = lwu(pv + 0x160);
    const uint32_t g = lbu(m + 0x21);
    const float base = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(leader + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(leader + 0x14)));
    if (g == 0xff) return base;
    float right = fpu_mul(lwc1(L10_ADDR(0x00161e48)), u2f(0x3c8efa35u));
    float left = -right;
    const float reach = vec3_distance(V(leader + 0x10), V(m + 0x10));
    // (the original also adds the hero's place to a point two units along `base` on its stack, then leaves it unused)
    const uint32_t list = lvd::group(static_cast<int32_t>(g));
    if (list != 0) {
        for (uint32_t e = list;; e += 2) {
            const uint32_t o = moby_at(static_cast<int32_t>(lhu(e) & 0x7fffu));
            if (lbu(o + 0x20) == 5 || vec3_distance(V(o + 0x10), V(leader + 0x10)) < reach) {
                const float a = angle_sub(atan2_f(fpu_sub(lwc1(o + 0x10), lwc1(leader + 0x10)), fpu_sub(lwc1(o + 0x14), lwc1(leader + 0x14))), base);
                if (0.0f < a) {
                    if (a < right) right = a;
                } else if (a < 0.0f && left < a) {
                    left = a;
                }
            }
            if (lh(e) < 0) break;
        }
    }
    return angle_add(angle_add(left, fpu_mul(angle_dist(right, left), 0.5f)), base);
}

namespace {
// whether moby `o` (the class 1202's guarded moby) is gone: none, dying, or its one-shot bit of the level's words set
bool guarded_gone(uint32_t o) {
    if (o == 0) return true;
    const uint32_t st = lbu(o + 0x20);
    if (st == 0xfe || st == 0xfd) return true;
    const uint32_t word = lwu(0x0014c190 + (static_cast<uint32_t>(lh(o + 0xb2) >> 5) << 2) + (level_number() << 8));
    return ((static_cast<int32_t>(word) >> (lhu(o + 0xb2) & 0x1f)) & 1) != 0;
}
}  // namespace

// Class 1202: a trooper who guards a moby (+0x1bc) and follows a leader (+0x160, the hero by default), walking on its walker
// record (+0xd0). Each frame its common part (orx_2e3ef0_l10: hits, target, alert), its scale (the class's times
// L10:0x161e24), its joint 2's node, and whether it sees its leader (within 15, inside the level's cube, no wall between;
// on another height only while the hero stands on something). States (the table at L10:0x001dd320): 0 sets it up
// (gone at once with +0x1dc set in Clank's part); 1 stands guard (turning to +0x17c) until its guarded moby goes or it
// has a target (2); 2 waits a little and readies (3: the hero beyond the level's range; else the chase, 5); 3 turns to
// its target and waits (4) a random time (+0x1b8 frames at most); 5 follows its leader at a gap in the group's ring
// (orx_2e4840_l10), slowing near it and when turned away, shooting (6) when something stands in the way or it got stuck,
// and swinging (7) when the leader is near and it faces it (a blow at its hand in the animation's frames 4 .. 7); 6 goes
// back to the chase once the way is clear; 8 walks home (then 3); 9 backs off from its leader and swings; 10 is knocked
// away (enemy_physics; a swing on landing, a fall off a ledge); 12 falls (into a pit: gone; on the ground: knocked
// down, 11); 11 dies (puff, smoke, explosion, six pieces, bolts). In states 3, 5, 7 and 8 it keeps to the floor and
// falls (12) off a ledge 2 below its home. The floor band near the camera.
// orig: L10:0x002e1d38
void orx_class1202_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    orx_2e3ef0_l10(m);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L10_ADDR(0x00161e24))));
    const uint32_t leader = lwu(pv + 0x160);
    joint_node_scale(m, 2, pv + 0x2b0, 2.5f);
    const uint32_t frame = ee_stack_alloc(0xa0);
    bool sees = false;
    {
        const uint32_t a = frame + 0x10, b = frame + 0x20;
        lq_sq(a, m + 0x10);
        swc1(a + 8, fpu_add(lwc1(a + 8), 1.0f));
        lq_sq(b, leader + 0x10);
        swc1(b + 8, fpu_add(lwc1(b + 8), 1.0f));
        if (vec3_distance(V(a), V(b)) < 15.0f) {
            bool inside = true;
            for (const uint32_t w : {a, a + 4, a + 8, b, b + 4, b + 8}) {
                const float v = lwc1(w);
                if (v < 2.0f || 1021.0f < v) inside = false;
                if (!inside) break;
            }
            if (inside) sees = segment_collide_v(V(a), V(b), 2, m, nullptr) == 0;
        }
        if (sees && !(abs_f(fpu_sub(lwc1(leader + 0x18), lwc1(m + 0x18))) < 1.0f) && moby_in_range_class_500(lwu(0x0013f64c)) == 0) sees = false;
    }
    auto anim_if = [&](uint32_t set, int32_t frames) {  // the animation `set` unless it plays
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
    };
    auto chase_start = [&] {  // the chase (5): from here, its speed 0, a random sway
        sb(m + 0x20, 5);
        sw(pv + 0x1d8, 0);
        lq_sq(pv + 0x1c0, m + 0x10);
        const float half = fpu_mul(lwc1(pv + 0x198), u2f(0x3c8efa35u));
        sw(pv + 0xf4, 0);
        swc1(pv + 0x194, rand_range_f(half, -half));
        anim_if(3, 5);
    };
    auto shoot_start = [&] {  // the shot (6) for 60 frames
        sb(m + 0x20, 6);
        sw(pv + 0x1a8, static_cast<uint32_t>(frame_scale_i(0x3c)));
        anim_if(1, 7);
    };
    auto wake_group = [&] {
        sw(pv + 0x1a8, static_cast<uint32_t>(frame_scale_i(1)));
        if (lbu(m + 0x21) != 0xff) group_wake(m);
    };
    const float two_pi = u2f(0x40c90fdbu), degree = u2f(0x3c8efa35u);
    auto turn = [&](float yaw) { yaw_turn_damped(m, pv + 0x190, yaw, u2f(0x3ca3d70au), u2f(0x3e99999au), fpu_mul(dt(), two_pi)); };
    auto gap_to = [&]() { return lh(leader + 0xa6) == 0x359 ? 2.0f : u2f(0x40647ae1u); };  // (the ring's radius: 2 about a Gadgebot)
    auto swing_start = [&](uint32_t st) {  // a swing (7) at five frame times with one of two animations
        sb(m + 0x20, st);
        swc1(pv + 0xf4, fpu_mul(dt(), 5.0f));
        const uint32_t set = (rand_mod(100) & 1) != 0 ? 6 : 7;
        moby_anim_set(m, set, 0, frame_scale_i(6));
    };
    auto dies = [&] {  // its puff, its two exhaust bursts, its explosion, six pieces (0x775, 0x77a .. 0x77c, 0x623, 0x624), its bolts
        particle34_puff_1(m);
        orx_2e3a90_l10(m);
        orx_2e3a90_l10(m);
        enemy_death_burst(m, m + 0x10, 1.25f, 20.0f);
        const uint32_t shared = OV(0x0015f580);  // (the engine's block the pieces take their speed, spin and size from)
        for (const uint32_t piece : {0x775u, 0x77au, 0x77bu, 0x77cu}) debris_spawn(m, piece, m + 0x10, m + 0x40, 0, 2, shared, shared, shared, 0.0f);
        // (the original jitters a copy of its place by these two, then hands the last two pieces its place itself)
        rand_range_signed_f(0.0f, u2f(0x3e99999au));
        rand_range_signed_f(0.0f, u2f(0x3e99999au));
        debris_spawn(m, 0x623, m + 0x10, m + 0x40, 0, 2, shared, shared, shared, 0.0f);
        debris_spawn(m, 0x624, m + 0x10, m + 0x40, 0, 2, shared, shared, shared, 0.0f);
        moby_drop_bolts(m, 0, -1);
        moby_retire(m);
    };
    bool tail = true;
    switch (lbu(m + 0x20)) {  // (the table at L10:0x001dd320)
    case 0: {  // (0x002e1f70)
        if (lw(pv + 0x1dc) != 0 && lbu(HERO_FORM) == 1) {
            moby_retire(m);
            tail = false;
            break;
        }
        swc1(pv + 0x30, 0.5f);
        sh(pv + 0x24, 5);
        sb(pv + 0x28, 3);
        swc1(pv + 0x20, 5.0f);
        sb(pv + 0x29, 1);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(10.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(16.0f)));
        sh(m + 0x34, lhu(m + 0x34) | 0x5000u);
        lq_sq(pv + 0x170, m + 0x10);
        swc1(pv + 0x17c, lwc1(m + 0x48));
        walker_record_init_l1(pv + 0xd0);
        swc1(pv + 0xdc, 0.5f);
        swc1(pv + 0xd8, 2.0f);
        sw(pv + 0xd0, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(L10_ADDR(0x00161e24)), 1024.0f))));
        swc1(pv + 0xd4, lwc1(L10_ADDR(0x00161e24)));
        swc1(pv + 0xf4, fpu_mul(lwc1(pv + 0x1a4), dt()));
        const int32_t guarded = lw(pv + 0x1bc);
        if (guarded != -1) {
            const uint32_t o = moby_at(guarded);
            if (o == 0 || lbu(o + 0x20) == 0xfe || lbu(o + 0x20) == 0xfd ||
                (guarded_gone(o) && lbu(0x0014c050 + (level_number() << 4) + lbu(o + 0xb0)) == 0xff))
                sw(pv + 0x1bc, 0xffffffffu);
        }
        const uint32_t anim = lbu(m + 0x53);
        sb(m + 0x20, 1);
        if (anim != 0) moby_anim_set(m, 0, 0, 0);
        swc1(pv + 0x1a0, lwc1(L10_ADDR(0x00161e40)));
        break;
    }
    case 1: {  // guarding (0x002e2120)
        turn(lwc1(pv + 0x17c));
        if (lw(pv + 0x1bc) != -1 && guarded_gone(moby_at(lw(pv + 0x1bc)))) {
            sb(m + 0x20, 2);
            wake_group();
        }
        if (lw(pv + 0x164) != 2 || lw(pv + 0x1b4) != 0) {
            sb(m + 0x20, 2);
            wake_group();
        }
        break;
    }
    case 2:  // readying (0x002e224c)
        if (lbu(m + 0x53) == 0) {
            if (lw(pv + 0x1a8) == 0) {
                sw(pv + 0x1a8, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(0.0f, 20.0f)))));
            } else if (tick32(pv + 0x1a8) != 0) {
                anim_if(lw(pv + 0x1b4) != 0 ? 1 : 2, 10);
            }
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < lwc1(L10_ADDR(0x00161e40))) {
            chase_start();
        } else {
            sb(m + 0x20, 3);
            anim_if(1, 10);
        }
        break;
    case 3:  // turning to its target (0x002e23ac)
        turn(atan2_f(fpu_sub(lwc1(pv + 0x120), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x124), lwc1(m + 0x14))));
        if (lw(pv + 0x164) == 2) break;
        sb(m + 0x20, 4);
        sw(pv + 0x1a8, static_cast<uint32_t>(float_to_int(rand_range_f(0.0f, static_cast<float>(lw(pv + 0x1b8))))));
        break;
    case 4:  // waiting (0x002e2440)
        if (tick32(pv + 0x1a8) != 0) chase_start();
        break;
    case 5: {  // the chase (0x002e24b4)
        const float dist = vec3_distance(V(m + 0x10), V(leader + 0x10));
        if (gap_to() < dist) approach_f(pv + 0xf4, fpu_mul(lwc1(pv + 0x1a4), dt()), fpu_mul(dt2(), 8.0f));
        else approach_f(pv + 0xf4, fpu_mul(fpu_mul(lwc1(pv + 0x1a4), dt()), u2f(0x3e99999au)), fpu_mul(dt2(), 20.0f));
        const uint32_t before = frame + 0x50;
        lq_sq(before, m + 0x10);
        bool settled = false;
        if (sees && abs_f(fpu_sub(dist, gap_to())) < u2f(0x3eb33333u)) {  // at its place in the ring: it stops
            approach_f(pv + 0xf4, 0.0f, fpu_mul(dt2(), 30.0f));
            settled = true;
        }
        if (!settled && lw(pv + 0x1d8) != 0) approach_f(pv + 0xf4, 0.0f, fpu_mul(dt2(), 10.0f));
        const float to_leader = atan2_f(fpu_sub(lwc1(leader + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(leader + 0x14), lwc1(m + 0x14)));
        const float off = angle_dist(to_leader, lwc1(m + 0x48));
        if (u2f(0x3f5f66f3u) < off) {  // turned away more than 50 degrees: a quarter speed at most
            const float cap = fpu_mul(fpu_mul(lwc1(pv + 0x1a4), dt()), 0.25f);
            if (cap < lwc1(pv + 0xf4)) swc1(pv + 0xf4, cap);
        }
        if (fpu_mul(lwc1(L10_ADDR(0x00161e4c)), degree) < off || lw(pv + 0x1d8) == 0) turn(to_leader);
        const float heading = orx_2e4840_l10(m);
        const uint32_t place = frame + 0x60;
        const float c = cos_vu(heading);
        swc1(place, lh(leader + 0xa6) == 0x359 ? fpu_add(c, c) : fpu_mul(c, u2f(0x40647ae1u)));
        const float sn = sin_vu(heading);
        swc1(place + 4, lh(leader + 0xa6) == 0x359 ? fpu_add(sn, sn) : fpu_mul(sn, u2f(0x40647ae1u)));
        sw(place + 8, 0);
        vec3_add(V(place), V(place), V(leader + 0x10));
        if (lbu(HERO_FORM) != 1) {
            const float yaw_before = lwc1(m + 0x48);
            const float yaw = atan2_f(fpu_sub(lwc1(place), lwc1(m + 0x10)), fpu_sub(lwc1(place + 4), lwc1(m + 0x14)));
            swc1(m + 0x48, yaw);
            const uint32_t dir = frame + 0x30, out = frame + 0x40, a = frame + 0x70, b = frame + 0x80;
            const float cy = cos_vu(yaw);
            swc1(dir, fpu_add(cy, cy));
            const float sy = sin_vu(lwc1(m + 0x48));
            sw(dir + 8, 0);
            swc1(dir + 4, fpu_add(sy, sy));
            vec_clear(V(out));
            lq_sq(a, m + 0x10);
            swc1(a + 8, fpu_add(lwc1(a + 8), 1.0f));
            lq_sq(b, leader + 0x10);
            swc1(b + 8, fpu_add(lwc1(b + 8), 1.0f));
            if (segment_collide_v(V(a), V(b), 2, m, nullptr) != 0) {  // something between it and its leader: it shoots
                const uint32_t hm = coll_hit_moby();
                if ((hm == 0 || lh(hm + 0xa6) != 0x31a) && vec3_distance(V(a), coll_hit_point()) < lwc1(L10_ADDR(0x00161e34))) shoot_start();
            }
            const uint32_t step = walker_record_step(1.0f, m, pv + 0xd0, dir, out);
            swc1(m + 0x48, yaw_before);
            if ((step & 3u) == 0) {
                sw(pv + 0x1d8, 0);
            } else {
                sw(pv + 0x1d8, vec2_distance(V(m + 0x10), V(before)) < fpu_mul(lwc1(pv + 0xf4), 0.25f) ? 1u : 0u);  // (stuck)
                if (ee_rem(lw(OV(0x0015f5cc)), 7) == 0 && lw(pv + 0x1a8) == 0) {
                    if (vec2_distance(V(pv + 0x1c0), V(m + 0x10)) < lwc1(pv + 0xf4) && off <= fpu_mul(lwc1(L10_ADDR(0x00161e4c)), degree)) {
                        shoot_start();
                    } else if (lbu(m + 0x53) == 1) {
                        moby_anim_set(m, 3, 0, frame_scale_i(7));
                    }
                    lq_sq(pv + 0x1c0, m + 0x10);
                }
            }
            if (lbu(m + 0x20) == 5 && tick32(pv + 0x1a8) != 0 && 0.0f < lwc1(pv + 0xf4) && lw(pv + 0x1d8) == 0 && lbu(m + 0x53) == 1)
                moby_anim_set(m, 3, 0, frame_scale_i(7));
        } else if (fpu_mul(dt(), u2f(0x3db2b8c2u)) < abs_f(lwc1(pv + 0x190))) {  // (Clank: turning on the spot)
            anim_if(3, 7);
        } else {
            anim_if(1, 7);
        }
        swc1(m + 0x58, u2f(0x3f99999au));
        if ((lbu(HERO_FORM) == 1 ? 1.0f : 30.0f) < vec3_distance(V(m + 0x10), V(pv + 0x170)) || lw(pv + 0x164) == 2) {  // home
            sb(m + 0x20, 8);
            moby_anim_set(m, 4, 0, frame_scale_i(0xc));
            break;
        }
        const float near = vec2_distance(V(m + 0x10), V(leader + 0x10));
        if (!(near < (lh(leader + 0xa6) == 0x359 ? u2f(0x401ccccdu) : u2f(0x4080a3d7u)))) break;
        if (!(lw(HERO + 0x1c0) < frame_scale_i(10))) break;
        if (!(off < u2f(0x3eb2b8c2u))) break;
        if (!sees) break;
        const uint32_t swing = (rand_mod(100) & 1) != 0 ? 6 : 7;  // a swing (7) with one of two animations
        sb(m + 0x20, 7);
        moby_anim_set(m, swing, 0, frame_scale_i(7));
        break;
    }
    case 6:  // the shot (0x002e2c74), then the swing's part (state 7's)
    case 7: {  // the swing (0x002e2f4c)
        if (lbu(m + 0x20) == 6) {
            const float off = angle_dist(atan2_f(fpu_sub(lwc1(leader + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(leader + 0x14), lwc1(m + 0x14))), lwc1(m + 0x48));
            if (u2f(0x3f5f66f3u) < off) {
                const float cap = fpu_mul(fpu_mul(lwc1(pv + 0x1a4), dt()), 0.25f);
                if (cap < lwc1(pv + 0xf4)) swc1(pv + 0xf4, cap);
            }
            if (tick32(pv + 0x1a8) != 0 || fpu_mul(lwc1(L10_ADDR(0x00161e4c)), degree) < off) {  // the way to its leader clear again: the chase
                const uint32_t a = frame + 0x30, b = frame + 0x40;
                bool clear = true;
                lq_sq(a, m + 0x10);
                swc1(a + 8, fpu_add(lwc1(a + 8), 1.0f));
                lq_sq(b, leader + 0x10);
                swc1(b + 8, fpu_add(lwc1(b + 8), 1.0f));
                if (segment_collide_v(V(a), V(b), 2, m, nullptr) != 0) {
                    const uint32_t hm = coll_hit_moby();
                    if ((hm == 0 || lh(hm + 0xa6) != 0x31a) && vec3_distance(V(a), coll_hit_point()) < lwc1(L10_ADDR(0x00161e34))) clear = false;
                }
                if (clear) {
                    sb(m + 0x20, 5);
                    sw(pv + 0x1d8, 0);
                    lq_sq(pv + 0x1c0, m + 0x10);
                    sw(pv + 0xf4, 0);
                    sw(pv + 0x1a8, static_cast<uint32_t>(frame_scale_i(0xf)));
                }
            }
            if ((lbu(HERO_FORM) == 1 ? 1.0f : 30.0f) < vec3_distance(V(m + 0x10), V(pv + 0x170)) || lw(pv + 0x164) == 2) {
                sb(m + 0x20, 8);
                moby_anim_set(m, 4, 0, frame_scale_i(0xc));
            } else {
                const float near = vec2_distance(V(m + 0x10), V(leader + 0x10));
                if (near < (lh(leader + 0xa6) == 0x359 ? u2f(0x401ccccdu) : u2f(0x4080a3d7u)) && lw(HERO + 0x1c0) < frame_scale_i(10) &&
                    off < fpu_mul(lwc1(L10_ADDR(0x00161e4c)), degree) && sees) {
                    const uint32_t swing = (rand_mod(100) & 1) != 0 ? 6 : 7;
                    sb(m + 0x20, 7);
                    moby_anim_set(m, swing, 0, frame_scale_i(7));
                }
            }
        }
        const float t = moby_anim_time(m);
        yaw_turn_damped(m, pv + 0x190, atan2_f(fpu_sub(lwc1(leader + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(leader + 0x14), lwc1(m + 0x14))),
                        u2f(0x3cf5c28fu), u2f(0x3e99999au), fpu_mul(dt(), two_pi));
        if (lbu(m + 0x53) - 6u >= 2u) break;
        if (4.0f <= t && t <= 7.0f) {  // the swing's blow: a hit record (+0x30 of the frame) at its hand (joint 0 / 1) and along the arm
            const uint32_t rec = frame + 0x30, hand = frame + 0x60, from = frame + 0x70;
            lq_sq(from, m + 0x10);
            swc1(from + 8, fpu_add(lwc1(from + 8), 0.25f));
            sw(rec + 0x14, 0x10001);
            swc1(rec + 0x1c, u2f(0x40466666u));
            sw(rec + 0x10, m);
            sw(rec + 0x20, 1);
            swc1(rec, cos_vu(lwc1(m + 0x48)));
            swc1(rec + 4, sin_vu(lwc1(m + 0x48)));
            swc1(rec + 8, 1.0f);
            swc1(rec + 0xc, u2f(0x45afdf66u));
            sb(rec + 0x19, 1);
            sh(rec + 0x1a, lhu(m + 0xa6));
            sb(rec + 0x18, 0);
            moby_chain_point_v(m, lbu(m + 0x53) == 6 ? 0 : 1, V(hand));
            mobys_in_sphere(hand, 0.75f, 0, m, rec);
            swc1(hand + 8, fpu_add(lwc1(hand + 8), 0.5f));
            mobys_in_sphere(hand, 0.75f, 0, m, rec);
            segment_collide_v(V(from), V(hand), 9, m, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        if ((lbu(HERO_FORM) == 1 ? 1.0f : 30.0f) < vec3_distance(V(m + 0x10), V(pv + 0x170)) || lw(pv + 0x164) == 2) {
            sb(m + 0x20, 8);
            moby_anim_set(m, 4, 0, frame_scale_i(0xc));
            break;
        }
        sb(m + 0x20, 5);
        sw(pv + 0x1d8, 0);
        lq_sq(pv + 0x1c0, m + 0x10);
        sw(pv + 0xf4, 0);
        anim_if(3, 7);
        break;
    }
    case 8: {  // home (0x002e3350)
        approach_f(pv + 0xf4, fpu_mul(lwc1(pv + 0x1a4), dt()), fpu_mul(dt2(), 5.0f));
        const float home = atan2_f(fpu_sub(lwc1(pv + 0x170), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x174), lwc1(m + 0x14)));
        turn(home);
        if (u2f(0x3f5f66f3u) < angle_dist(home, lwc1(m + 0x48))) {
            const float cap = fpu_mul(fpu_mul(lwc1(pv + 0x1a4), dt()), 0.25f);
            if (cap < lwc1(pv + 0xf4)) swc1(pv + 0xf4, cap);
        }
        const uint32_t dir = frame + 0x30, out = frame + 0x40;
        const float c = cos_vu(lwc1(m + 0x48));
        swc1(dir, fpu_add(c, c));
        const float sn = sin_vu(lwc1(m + 0x48));
        sw(dir + 8, 0);
        swc1(dir + 4, fpu_add(sn, sn));
        walker_record_step(1.0f, m, pv + 0xd0, dir, out);
        if (vec3_distance(V(m + 0x10), V(pv + 0x170)) < 1.0f) {
            sb(m + 0x20, 3);
            anim_if(1, 0x14);
        } else if (lw(pv + 0x164) != 2) {
            sb(m + 0x20, 5);
            sw(pv + 0x1d8, 0);
            lq_sq(pv + 0x1c0, m + 0x10);
            sw(pv + 0xf4, 0);
            anim_if(3, 7);
        }
        break;
    }
    case 9: {  // backing off from its leader (0x002e3204)
        approach_f(pv + 0xf4, fpu_mul(fpu_mul(lwc1(pv + 0x1a4), 3.0f), dt()), fpu_mul(dt2(), 20.0f));
        turn(atan2_f(fpu_sub(lwc1(leader + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(leader + 0x14), lwc1(m + 0x14))));
        const float yaw_before = lwc1(m + 0x48);
        const float away = angle_add(atan2_f(fpu_sub(lwc1(leader + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(leader + 0x14), lwc1(m + 0x14))), u2f(0x40490fd0u));
        swc1(m + 0x48, away);
        const uint32_t dir = frame + 0x30, out = frame + 0x40;
        const float c = cos_vu(away);
        swc1(dir, fpu_add(c, c));
        const float sn = sin_vu(lwc1(m + 0x48));
        sw(dir + 8, 0);
        swc1(dir + 4, fpu_add(sn, sn));
        walker_record_step(1.0f, m, pv + 0xd0, dir, out);
        swc1(m + 0x48, yaw_before);
        if (!sees) break;
        if (tick32(pv + 0x1a8) == 0 && !(lwc1(L10_ADDR(0x00161e44)) < vec3_distance(V(m + 0x10), V(leader + 0x10)))) break;
        swing_start(7);
        break;
    }
    case 10:  // knocked away (0x002e350c)
        if ((enemy_physics(m, pv + 0x70) & 0x41u) != 0) {
            swing_start(7);
        } else if (lwc1(m + 0x18) < fpu_sub(lwc1(pv + 0x178), u2f(0x3e4ccccdu))) {  // off a ledge: falling
            if (lbu(m + 0x53) != 14) moby_anim_set(m, 14, 0, frame_scale_i(6));
            sb(m + 0x20, 12);
            swc1(pv + 0x78, fpu_div(lwc1(pv + 0x78), 3.0f));
        }
        break;
    case 12: {  // falling (0x002e35f0)
        swc1(pv + 0x78, fpu_sub(lwc1(pv + 0x78), lwc1(pv + 0x80)));
        vec3_add_inplace(V(m + 0x10), V(pv + 0x70));
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        if (!(lwc1(m + 0x18) < floor)) {
            dies();
            tail = false;
            break;
        }
        const int32_t ground = sub_001f35d8();
        if (static_cast<uint32_t>(ground) < 2u || ground == 3) {  // into the pit: gone with its bolts
            moby_drop_bolts(m, 0x200, -1);
            moby_retire(m);
            tail = false;
            break;
        }
        sb(m + 0x20, 11);  // landed: knocked down
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        const uint32_t v = static_cast<uint32_t>(float_to_int(768.0f));
        swc1(pv + 0x80, fpu_mul(lwc1(L10_ADDR(0x00161e28)), dt2()));
        sb(pv + 0xad, 0);
        sw(pv + 0x90, v);
        swc1(pv + 0x98, 0.75f);
        sw(pv + 0x94, 9);
        jump_arc_set(pv + 0x70, 2.0f, 1.5f);
        swc1(pv + 0xc4, 13.0f);
        swc1(pv + 0xc0, 7.0f);
        move_state_start(m, pv + 0x70, 0xd, 1, 0, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
        sb(pv + 0x67, 0xfa);
        color_pulse_start(m, pv + 0x60);
        moby_drop_bolts(m, 0, -1);
        break;
    }
    case 11:  // dying (0x002e3748)
        dies();
        tail = false;
        break;
    default:
        break;
    }
    ee_stack_free(0xa0);
    if (!tail) return;
    const uint32_t st = lbu(m + 0x20);
    if (st == 3 || st == 5 || st == 7 || st == 8) {  // on the floor, or falling off a ledge
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        if (floor < lwc1(m + 0x18)) {
            const float lim = fpu_mul(dt(), -7.0f);
            const float vz = fpu_sub(lwc1(pv + 0x78), lwc1(pv + 0x80));
            swc1(pv + 0x78, vz < lim ? lim : vz);
            const float z = fpu_add(lwc1(m + 0x18), lwc1(pv + 0x78));
            swc1(m + 0x18, z < floor ? floor : z);
            if (floor < fpu_sub(lwc1(pv + 0x178), 2.0f)) {
                if (lbu(m + 0x53) != 14) moby_anim_set(m, 14, 0, frame_scale_i(6));
                sb(m + 0x20, 12);
            }
        }
    }
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 18.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0xe);
    }
}

}  // namespace rac1
