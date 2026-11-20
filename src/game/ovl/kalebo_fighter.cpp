// Kalebo III's (level 16) class 541: the fighters of the lock-in fights (class 654 wakes them, state 2). A fighter walks
// along its level path (+0xc4) between two of its points (+0xdc the point it is at), turns, and attacks from there with what
// it carries at its joint (+0x104, spawned at set-up by its kind +0xc0): kind 0 a launcher (class 0x35f) that throws class
// 281 bombs, kind 1 a launcher (0x374) it walks back and forth with, kind 2 a flame thrower (0x36c, the flame spray at
// +0x120). It presses the floor plate (class 650) nearest its place, reels when hit (state 9), blows up at no health
// (state 10) and comes back at its place (+0xe0) as many times as its count +0xc8 says. Its group (+0x21) turns in step
// (+0x11c, drawn by L16:0x2cef60). Level 16 only: `orig: L16:`, level data L16_ADDR / lvd::*, base ELF bare. Registered in
// kalebo_mobys.cpp; checked by batch 250 (class541, class541_cases).
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_flame_spray.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
using kalebo::Frame;
using kalebo::group_entry_of_class;
using kalebo::tick32;

constexpr uint32_t HERO = 0x0013f350;     // the hero (+0x80 his position)
constexpr int16_t kFighterClass = 0x21d;  // class 541
constexpr int16_t kPlateClass = 0x28a;    // class 650

float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // (its square)

// the heading from the moby to the point (x, y) at `p`, as the original works it out
float heading_to(uint32_t m, uint32_t p) {
    return atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14)));
}

// the animation set `set` from frame 0 over `frames` 60 Hz frames, unless it is playing
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the point `i` of the level path at `path` (its points from +0x10, 16 bytes each)
uint32_t path_point(uint32_t path, uint32_t i) { return path + 0x10 + (i << 4); }

}  // namespace

// The bomb (class 281) a fighter throws from `pos` with the velocity at `vel`: its fuse `fuse` frames, its fall a frame `fall`,
// the thrower `thrower` (its flight does not meet him), turned at random about z and along the velocity. Returns it, or 0.
// orig: L16:0x002c6268
uint32_t kal_fighter_bomb_throw_l16(uint32_t thrower, uint32_t pos, uint32_t vel, uint32_t fuse, float fall) {
    const uint32_t b = moby_spawn(0x119);
    if (b == 0) return 0;
    const uint32_t pv = lwu(b + 0x78);
    sh(b + 0x32, 0xff);
    sb(b + 0x20, 1);
    sb(b + 0x31, 1);
    sb(b + 0x30, 0xff);
    sw(pv + 0x20, thrower);
    swc1(pv + 0x24, fall);
    sw(pv + 0x28, fuse);
    sh(pv + 0xc, static_cast<uint32_t>(frame_scale_i(5)));
    sh(pv + 0xe, static_cast<uint32_t>(frame_scale_i(0xf)));
    V(b + 0x10) = V(pos);
    V(pv + 0x10) = V(vel);
    swc1(b + 0x40, fpu_mul(rand_range_f(-180.0f, 180.0f), u2f(0x3c8efa35u)));  // (degrees to radians)
    swc1(b + 0x48, fpu_add(atan2_f(lwc1(vel), lwc1(vel + 4)), u2f(0x3fc90fdbu)));  // (+ pi / 2)
    moby_update_transform(b);
    return b;
}

// The spin of the fighter's group (+0x21; 0xff: none): each fighter of it turned on by the level's rate (L16 0x1619dc, degrees a
// second) and marked done this frame (+0x118), then the group's draw callback queued. (The original reads the group's list
// without a null test, and reads an entry whose moby is not a fighter again, without end: kalebo::group_entry_of_class.)
// orig: L16:0x002cee70
void kal_fighter_group_spin_l16(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) return;
    const float turn = fpu_mul(fpu_mul(lwc1(L16_ADDR(0x001619dc)), u2f(0x3c8efa35u)), dt());
    for (uint32_t e = lvd::group(static_cast<int32_t>(group));; e += 2) {
        const uint32_t f = group_entry_of_class(e, kFighterClass);
        const uint32_t fpv = lwu(f + 0x78);
        sw(fpv + 0x118, lwu(L16_ADDR(0x0015f5cc)));
        swc1(fpv + 0x11c, angle_add(lwc1(fpv + 0x11c), turn));
        if (lh(e) < 0) break;
    }
    sub_001f93e0(L16_ADDR(0x002cef60), m);
}

// The fighter's every-frame part (except in state 1, asleep): a fighter out of health (its hit record +0x2e at 2) drops its
// bolts and comes back at its place in state 0xb while its count +0xc8 lasts, else it goes with what it carries; otherwise its
// group spins (once a frame), its hits (mask 0x330000) take its health +0x20 (at 0: state 0xa) or make it reel (state 9: the
// state and animation it was in kept at +0x110 / +0x114, its heading to the hero +0x10c, a red pulse); its colour pulse
// (+0x60) and the carried moby put at its joint (L16 0x1619c8) with the joint's turn.
// orig: L16:0x002ce9f0
void kal_fighter_frame_l16(uint32_t m) {
    if (lbu(m + 0x20) == 1) return;
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xb0);
    const uint32_t sp = f.sp;
    if (lbu(pv + 0x2e) == 2) {
        sb(pv + 0x2e, 1);
        swc1(pv + 0x20, 2.0f);
        sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x3c)));
        moby_drop_bolts(m, 0, -1);
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        const int32_t left = lw(pv + 0xc8) - 1;
        sw(pv + 0xc8, static_cast<uint32_t>(left));
        if (left != -1) {
            sb(m + 0x20, 0xb);
            V(m + 0x10) = V(pv + 0xe0);
            sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x3c)));
            return;
        }
        if (lwu(pv + 0x104) != 0) moby_retire(lwu(pv + 0x104));
        moby_retire(m);
        return;
    }
    if (lwu(pv + 0x118) != lwu(L16_ADDR(0x0015f5cc))) kal_fighter_group_spin_l16(m);
    const uint32_t pulse = pv + 0x60;
    const uint32_t hit_state = sp + 0x40, hit_dmg = sp + 0x44;
    swc1(hit_dmg, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    {
        int32_t st = lw(hit_state);  // (a word of the frame the original never writes first)
        float dmg = lwc1(hit_dmg);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    const uint32_t state = lbu(m + 0x20);
    if (lw(hit_state) != 1 && state != 0xa && state != 0xb) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, health);
        if (health <= 0.0f) {
            sb(m + 0x20, 0xa);
        } else {
            if (state != 9) {  // (reeling again keeps what it was doing before the first hit)
                sw(pv + 0x110, state);
                sw(pv + 0x114, lbu(m + 0x53));
            }
            swc1(pv + 0x10c, heading_to(m, HERO + 0x80));
            sb(m + 0x20, 9);
            if (lbu(m + 0x53) != 8) moby_anim_set(m, 8, 2, 1);  // (a raw count: not frame-scaled)
            sb(pv + 0x67, 0x78);
            color_pulse_start(m, pulse);
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
    const uint32_t carried = lwu(pv + 0x104);
    if (carried != 0) {
        moby_chain_point_v(m, lwu(L16_ADDR(0x001619c8)), V(carried + 0x10));
        moby_chain_world_one(m, lwu(L16_ADDR(0x001619c8)), sp);
        mat3_copy(M(lwu(pv + 0x104) + 0xc0), M(sp));
        for (uint32_t row = 0xc0; row <= 0xe0; row += 0x10) {
            const uint32_t r = lwu(pv + 0x104) + row;
            vec3_set_length(V(r), V(r), 1.0f);
        }
        moby_update_transform(lwu(pv + 0x104));
    }
    color_pulse_update(m, pulse);  // (the original updates the pulse a second time)
}

// A step of the fighter toward the point at `target` with its heading turned toward `yaw` (its turn rate +0x100): its speed
// +0xfc eased toward the distance (at most 6 a second), the move along the line to the point. Returns 1 when it stood within
// 0.05 of the point and its speed is below 0.005, else 0.
// orig: L16:0x002ced20
uint32_t kal_fighter_step_l16(uint32_t m, uint32_t target, float yaw) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x80);
    const uint32_t sp = f.sp;
    V(sp) = V(target);
    const float turn = fpu_mul(dt2(), u2f(0x41490fdbu));  // (4 pi)
    yaw_seek(m + 0x48, pv + 0x100, yaw, turn, turn, fpu_mul(dt(), u2f(0x41c90fdbu)));  // (8 pi)
    const uint32_t dir = sp + 0x10, at = sp + 0x20;
    sw(at, 0);
    vec3_sub(V(dir), V(sp), V(m + 0x10));
    const float dist = vec3_length(V(dir));
    const float accel = fpu_mul(dt2(), 12.0f);
    seek_1d(at, pv + 0xfc, dist, accel, accel, fpu_mul(dt(), 6.0f));
    vec3_set_length(V(dir), V(dir), lwc1(pv + 0xfc));
    vec3_add(V(m + 0x10), V(m + 0x10), V(dir));
    if (!(dist < 0.05f)) return 0;
    return lwc1(pv + 0xfc) < u2f(0x3ba3d70au) ? 1u : 0u;  // (0.005)
}

// The floor plate (class 650) of group `group` nearest the point at `p`, within 37; 0 when none (or the group has no list).
// (An entry whose moby is not a plate is read again, without end: kalebo::group_entry_of_class.)
// orig: L16:0x002d5db0
uint32_t kal_fighter_plate_find_l16(int32_t group, uint32_t p) {
    uint32_t e = lvd::group(group);
    if (e == 0) return 0;
    float best = 37.0f;
    uint32_t found = 0;
    for (;; e += 2) {
        const uint32_t plate = group_entry_of_class(e, kPlateClass);
        const float d = vec3_distance(V(p), V(plate + 0x10));
        if (d < best) {
            best = d;
            found = plate;
        }
        if (lh(e) < 0) break;
    }
    return found;
}

// The fighter's weight on the plate `plate`: its timer +0x8 set to 5 frames; when it had run out, the clunk (sound 0), at most
// once every 4 frames for all plates (L16 0x161a88 the frame of the last one). (The plate is not tested for 0.)
// orig: L16:0x002d5e80
void kal_fighter_plate_press_l16(uint32_t plate) {
    const uint32_t ppv = lwu(plate + 0x78);
    if (lw(ppv + 8) == 0 && !(abs_i(lw(L16_ADDR(0x00161a88)) - lw(L16_ADDR(0x0015f5cc))) < 4)) {
        sw(L16_ADDR(0x00161a88), lwu(L16_ADDR(0x0015f5cc)));
        moby_sound_start(0, 0, plate);
    }
    sw(ppv + 8, static_cast<uint32_t>(frame_scale_i(5)));
}

// Class 541. The states (the jump table L16 0x1e8230; 1 and >= 12 do nothing):
//  0 set-up: its count of lives (the moby's +0xb4 halfword divided by it), awake (state 1: the fight wakes it), its plate, its
//    place, what it carries by its kind; 2 to its path's first point (pressing its plate), then by kind: 0 to 3, 1 to 5,
//    else 7; 3 / 4 the thrower: to the point it is at, aiming, then throwing a bomb at the target (+0x70, or its box +0xd0 /
//    +0xd4 when +0xb4 is 2) as its animation crosses 15, and back (3); 5 / 6 the walker: to the next point, a second's pause
//    and the turn to the one after (the path's ends turn it back: +0x108); 7 / 8 the flame: to its point, then the flame
//    from its weapon's joint while the animation plays, then (7) to the other point; 9 reeling from a hit (back to the state
//    it was in); 10 blowing up (debris 0x655 .. 0x657, its bolts; back at its place in state 0xb while it has lives left);
//    11 rising back into place, then 2.
// orig: L16:0x002cddb8
void kal_class541_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xf0);
    const uint32_t sp = f.sp;
    kal_fighter_frame_l16(m);
    const uint32_t path = lvd::path(lw(pv + 0xc4));
    joint_node_scale(m, 2, pv + 0x140, 2.5f);
    const uint32_t aim = sp + 0x10, at = sp + 0x20;

    // the target's choice (states 3 and 7): its box (+0xb4 = 2) or, half the time, a point of the target path (+0xd8)
    auto pick_target = [m, pv]() {
        if (rand_mod(2) == 0) {
            sw(pv + 0xb4, 2);
            return;
        }
        const uint32_t tp = lvd::path(lw(pv + 0xd8));
        target_select(m, pv + 0x70, 0, 0, tp + 0x10, lw(tp), 12.0f);
    };

    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002cde48)
        const int32_t lives = lw(pv + 0xc8);
        sb(pv + 0x2e, 1);
        if (lives > 0) sh(m + 0xb4, static_cast<uint32_t>(ee_div(lh(m + 0xb4), lives)));
        sb(m + 0x20, 1);
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        if (rand_mod(2) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        sb(pv + 0x28, 1);
        sh(pv + 0x24, 2);
        swc1(pv + 0x20, 2.0f);
        swc1(pv + 0x11c, rand_angle());
        sw(pv + 0xf8, kal_fighter_plate_find_l16(lw(pv + 0xcc), m + 0x10));
        V(pv + 0xe0) = V(m + 0x10);
        switch (lw(pv + 0xc0)) {
        case 0:
            sw(pv + 0x104, moby_spawn(0x35f));
            break;
        case 1:
            sw(pv + 0x104, moby_spawn(0x374));
            break;
        case 2:
            sw(pv + 0x104, moby_spawn(0x36c));
            flame_spray_clear(pv + 0x120);
            break;
        default:
            break;
        }
        const uint32_t carried = lwu(pv + 0x104);
        if (carried == 0) break;
        sh(carried + 0x34, lhu(carried + 0x34) | 0x100u);
        sh(lwu(pv + 0x104) + 0x32, 0x40);
        sb(lwu(pv + 0x104) + 0x30, 0);
        sb(lwu(pv + 0x104) + 0x31, 1);
        break;
    }
    case 2: {  // (0x002cdf78)
        kal_fighter_plate_press_l16(lwu(pv + 0xf8));
        V(aim) = V(path_point(path, 0));
        if (kal_fighter_step_l16(m, aim, lwc1(m + 0x48)) == 0) break;
        sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sw(pv + 0xdc, 1);
        const int32_t kind = lw(pv + 0xc0);
        sw(pv + 0x108, 1);
        sb(m + 0x20, kind == 0 ? 3u : (kind == 1 ? 5u : 7u));
        break;
    }
    case 3: {  // (0x002cdfdc)
        if (tick32(pv + 0xf0) == 0) break;
        const uint32_t pt = path_point(path, lwu(pv + 0xdc));
        V(aim) = V(pt);
        if (kal_fighter_step_l16(m, aim, heading_to(m, pt)) == 0) break;
        sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sb(m + 0x20, 4);
        anim_to(m, 0x15, 0x1e);
        pick_target();
        break;
    }
    case 4: {  // (0x002ce098)
        if (lw(pv + 0xb4) == 2)
            V(aim) = V(lvd::box(lvd::BOXES, lw(pv + 0xd0 + (lwu(pv + 0xdc) << 2))) + 0x30);
        else
            V(aim) = V(pv + 0x70);
        if (anim_crossed(m, 15.0f) != 0 && lwu(pv + 0x104) != 0) {
            const float speed = fpu_mul(lwc1(L16_ADDR(0x001619cc)), dt());
            const float fall = fpu_mul(dt2(), 10.0f);
            swc1(aim + 8, floor_probe_v(0.5f, V(aim), 0));
            moby_chain_point_v(lwu(pv + 0x104), 0, V(at));
            const uint32_t vel = sp + 0x30;
            vec3_sub(V(vel), V(aim), V(at));
            sw(vel + 8, 0);
            vec3_set_length(V(vel), V(vel), speed);
            swc1(vel + 8, sub_0025abf0(at, aim, nullptr, speed, -fall));
            kal_fighter_bomb_throw_l16(m, at, vel, static_cast<uint32_t>(frame_scale_i(0x12c)), fall);
        } else if ((lbu(m + 0x70) & 2) != 0) {
            sw(pv + 0xdc, (lwu(pv + 0xdc) + 1) & 1u);
            sb(m + 0x20, 3);
            anim_to(m, 0x17, 0x1e);
        }
        uint32_t toward;
        if (moby_anim_time(m) < 15.0f) {
            toward = aim;  // (facing the target while it aims)
        } else {
            toward = path_point(path, (lwu(pv + 0xdc) + 1) & 1u);  // (then the other point of its two)
        }
        const float yaw = heading_to(m, toward);
        V(at) = V(m + 0x10);
        kal_fighter_step_l16(m, at, yaw);
        break;
    }
    case 5: {  // (0x002ce2b4)
        if (lbu(m + 0x53) == 0xa) {
            if ((lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 0xb, 0, 1);  // (a raw count: not frame-scaled)
            break;
        }
        vec3_sub(V(aim), V(path_point(path, lwu(pv + 0xdc))), V(m + 0x10));
        vec3_clamp_length(aim, fpu_mul(lwc1(L16_ADDR(0x001619d0)), dt()));
        vec3_add(V(m + 0x10), V(m + 0x10), V(aim));
        moby_chain_point_v(m, lwu(L16_ADDR(0x001619c8)), V(at));
        moby_sphere_contact(m, at, 1, 0, 1, 0, u2f(0x3eaa7efau), 1.0f, 1.0f);  // (0.333)
        if (coll_hit_moby() == lwu(0x001413d0) && lw(0x0013f510) == 0) moby_sound_start(6, 0, m);  // (it ran into the hero)
        if (!(vec3_length(V(aim)) < u2f(0x38d1b717u))) break;  // (1e-4: still on its way)
        anim_to(m, 9, 0x14);
        sb(m + 0x20, 6);
        const uint32_t idx = lwu(pv + 0xdc);
        sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x3c)));
        if (idx == 0) {
            sw(pv + 0x108, 1);
        } else if (idx == lwu(path) - 1) {
            sw(pv + 0x108, 0xffffffffu);
        }
        break;
    }
    case 6: {  // (0x002ce440)
        const uint32_t next = lwu(pv + 0xdc) + lwu(pv + 0x108);
        const float yaw = heading_to(m, path_point(path, next));
        const float turn = fpu_mul(dt2(), u2f(0x41490fdbu));  // (4 pi)
        yaw_seek(m + 0x48, pv + 0xfc, yaw, turn, turn, fpu_mul(dt(), u2f(0x41c90fdbu)));  // (8 pi)
        if (tick32(pv + 0xf0) == 0) break;
        sw(pv + 0xdc, next);
        anim_to(m, 0xa, 0xa);
        sb(m + 0x20, 5);
        break;
    }
    case 7: {  // (0x002ce500)
        if (tick32(pv + 0xf0) == 0) break;
        V(aim) = V(path_point(path, lwu(pv + 0xdc)));
        if (kal_fighter_step_l16(m, aim, lwc1(m + 0x48)) == 0) break;
        sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sb(m + 0x20, 8);
        anim_to(m, 1, 0x1e);
        pick_target();
        break;
    }
    case 8: {  // (0x002ce5d0)
        if (lw(pv + 0xb4) != 2) {
            V(aim) = V(m + 0x10);
            kal_fighter_step_l16(m, aim, heading_to(m, pv + 0x70));
        }
        if (lwu(pv + 0x104) != 0) {  // the flame from the weapon's joint 0, along its heading, burning what it meets
            const uint32_t spray = pv + 0x120, dir = sp + 0x20, burn = sp + 0x40;
            moby_chain_point_v(lwu(pv + 0x104), 0, V(aim));
            swc1(dir, cos_vu(heading_to(m, aim)));
            swc1(dir + 4, sin_vu(heading_to(m, aim)));  // (the original works the same heading out a second time for it)
            sw(dir + 8, 0);
            flame_spray(spray, aim, dir, lwc1(L16_ADDR(0x001619d4)));
            swc1(dir + 0xc, u2f(0x45afdf66u));
            swc1(dir + 8, 1.0f);
            sub_00259888(1.0f, burn, static_cast<int32_t>(m), 0x10001, dir);
            sb(burn + 0x18, 5);
            sh(burn + 0x1a, lhu(m + 0xa6));
            sb(burn + 0x19, 1);
            flame_spray_burn(spray, m, burn);
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        sw(pv + 0xdc, (lwu(pv + 0xdc) + 1) & 1u);
        sb(m + 0x20, 7);
        anim_to(m, 3, 0x1e);
        break;
    }
    case 9: {  // (0x002ce748)
        V(aim) = V(path_point(path, lwu(pv + 0xdc)));
        kal_fighter_step_l16(m, aim, lwc1(pv + 0x10c));
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, lbu(pv + 0x110));
        if (lbu(m + 0x53) == lwu(pv + 0x114)) break;
        moby_anim_set(m, lwu(pv + 0x114), 0, frame_scale_i(0x14));
        break;
    }
    case 10: {  // (0x002ce7b8)
        swc1(pv + 0x20, 2.0f);
        sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x3c)));
        const float grav = fpu_mul(dt2(), 12.0f);
        const uint32_t zero = L16_ADDR(0x0015f580);
        for (uint32_t cls = 0x655; cls <= 0x657; ++cls) debris_spawn(m, cls, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, grav);
        V(aim) = V(m + 0x10);
        swc1(aim + 8, fpu_add(lwc1(aim + 8), u2f(0x3f99999au)));  // (1.2 above it)
        explosion_burst(m, aim, -1, 0.5f, 10.0f);
        moby_drop_bolts(m, 0, -1);
        moby_sound_start(7, 0, m);
        const int32_t left = lw(pv + 0xc8) - 1;
        sw(pv + 0xc8, static_cast<uint32_t>(left));
        if (left != -1) {
            sb(m + 0x20, 0xb);
            V(m + 0x10) = V(pv + 0xe0);
            sw(pv + 0xf0, static_cast<uint32_t>(frame_scale_i(0x3c)));
            break;
        }
        if (lwu(pv + 0x104) != 0) moby_retire(lwu(pv + 0x104));
        moby_retire(m);
        break;
    }
    case 11: {  // (0x002ce934)
        if (tick32(pv + 0xf0) != 0) {
            sw(pv + 0xfc, 0);
            sb(m + 0x20, 2);
            anim_to(m, 9, 0xa);
            swc1(pv + 0x20, 2.0f);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        }
        const float left = static_cast<float>(lw(pv + 0xf0));
        swc1(m + 0x18, fpu_add(lwc1(pv + 0xe8), fpu_mul(fpu_mul(dt(), 3.0f), left)));
        break;
    }
    default:  // (1 and >= 12: 0x002ce9c0)
        break;
    }
}

}  // namespace rac1
