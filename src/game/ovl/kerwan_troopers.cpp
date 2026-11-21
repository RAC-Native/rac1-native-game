// Kerwan's own moby classes: class 574, the Blarg trooper of Metropolis, and its helpers. Header kerwan_mobys.h, registered in
// kerwan_mobys.cpp. Addresses are Kerwan's (`orig: L3:`); data only Kerwan has is L3_ADDR, the engine's words shared with Veldin
// are the level's own (OV), the base ELF's (the hero 0x13f350, the frame times, the checkpoint table 0x14c050) are bare.
//
// The trooper fires bolts (class 833) from its rifle, strikes at close range, patrols its path, jumps down from heights, rides
// the train's carriages (class 1210, or class 821) and is the leader the class 573 walkers follow (kerwan_walkers.cpp). Its
// pvars: +0x20 its health, +0x40 the walk's out record, +0x60 the colour pulse, +0x70 the move record (knock-back, jumps), +0xd0
// the walker record, +0x120 its path follower (+0x120 the point, +0x124 the direction, +0x130 its path), +0x134 its path's
// index, +0x150 / +0x1d0 its head's and body's joint springs (+0x1b8 / +0x238 their angles), +0x250 the wait before it fires,
// +0x252 the alert's time, +0x254 the shots fired, +0x255 the frames knocked out, +0x256 set: walk its path, +0x258 the turn's
// speed, +0x262 its mode (0 the level's, 1 a checkpoint's spare, 2 ...), +0x263 the wait before it turns again, +0x264 the train
// it belongs to (-1: none; it comes back after a death), +0x26c / +0x270 its sight (+5 while alerted), +0x274 set: walk its path
// point by point, +0x278 its fire range, +0x27c its approach range, +0x280 the moby it stands on (-1: none), +0x284 a moby it waits
// for, +0x288 / +0x28a its draw distance and its eighth of the frames, +0x28c the heading of its turn, +0x290 / +0x294 / +0x298 /
// +0x29c a second path, its reach, its point and the box it guards. Its state is the moby's +0x20; +0xbc the state a turn returns
// to.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rocket.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position)

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
float ratio() { return lwc1(0x0015ed64); }  // (the frame's rate against 60 Hz)
const float kPi = u2f(0x40490fdbu);
const float kQuarterPi = u2f(0x3f490fdbu);

// the moby of index `i` (the moby list in 0x100-byte steps)
uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the camera's position (the level's view, Veldin's 0x166dc0)
uint32_t camera_pos() { return OV(0x00166dc0); }

// the heading from the moby to (x, y)
float heading_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// The checkpoint table's mark of the moby's spawn (+0xb0) on this level: 0xff when it was used up.
uint32_t spawn_mark(uint32_t m) { return lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)); }

// The level's jump numbers (Kerwan's own words): the rise of a knock-back, its height and its gravity's share.
float jump_height() { return lwc1(L3_ADDR(0x001618e0)); }
float jump_rise() { return lwc1(L3_ADDR(0x001618e4)); }
float jump_gravity() { return lwc1(L3_ADDR(0x001618e8)); }

// A knock-back's arc in the move record +0x70 (pvar +0x80 its gravity `g` x the frame time squared, +0x84 the level's share of it
// when `store84`, +0x88 its rise speed, +0x8c its speed): the time to the top from sqrt(k v), the rise `rise` over it, the level's
// gravity's share over that time; halved when `halve`.
void knock_arc(uint32_t pv, float g, bool store84, float k, bool k_first, float rise, bool halve) {
    const float v = fpu_mul(dt_sq(), g);
    swc1(pv + 0x80, v);
    if (store84) swc1(pv + 0x84, fpu_mul(jump_gravity(), dt_sq()));
    const float arg = k_first ? fpu_mul(k, v) : fpu_mul(v, k);
    const float a = sqrt_f(arg);
    const float b = sqrt_f(arg);
    const float ta = fpu_div(fpu_add(a, a), v);
    const float tb = fpu_div(fpu_add(b, b), v);
    float up = fpu_add(fpu_div(rise, ta), fpu_mul(fpu_mul(fpu_mul(jump_gravity(), dt_sq()), 0.5f), tb));
    if (halve) up = fpu_mul(up, 0.5f);
    swc1(pv + 0x88, up);
    swc1(pv + 0x8c, sqrt_f(arg));
}
}  // namespace

// The rotation that turns the moby's up axis onto the floor's normal `normal` (an EE vector) combined with a turn of `yaw` about
// it (I + sin K + (1 - cos) K^2, K the normal's cross-product matrix), taken to Euler angles in a copy of `angles`: the result
// is left on the stack and lost (the original's caller ignores it; the function has no other effect).
// orig: L3:0x0024fba8
void trooper_floor_tilt_l3(uint32_t normal, uint32_t angles, float yaw) {
    const uint32_t sp = ee_stack_alloc(0x1c0);
    const uint32_t n = sp, out = sp + 0x10, k = sp + 0x20, k2 = sp + 0x60, s = sp + 0xa0, c = sp + 0xe0, r = sp + 0x120;
    lq_sq(n, normal);
    lq_sq(out, angles);
    const float nx = lwc1(n), ny = lwc1(n + 4), nz = lwc1(n + 8);
    sw(k, 0);
    swc1(k + 4, nz);
    swc1(k + 8, -ny);
    sw(k + 0xc, 0);
    swc1(k + 0x10, -nz);
    sw(k + 0x14, 0);
    swc1(k + 0x18, nx);
    sw(k + 0x1c, 0);
    swc1(k + 0x20, ny);
    swc1(k + 0x24, -nx);
    sw(k + 0x28, 0);
    sw(k + 0x2c, 0);
    sw(k + 0x30, 0);
    sw(k + 0x34, 0);
    sw(k + 0x38, 0);
    sw(k + 0x3c, 0);
    mat4_mul(M(k2), M(k), M(k));
    rocket_1ffd48(c, k2, fpu_sub(1.0f, cos_vu(yaw)));
    rocket_1ffd48(s, k, sin_vu(yaw));
    rocket_1ffe20(s, s, c);
    mat4_identity(M(c));
    rocket_1ffe20(r, s, c);
    mat_to_euler(r, out);
    ee_stack_free(0x1c0);
}

// The trooper's turn toward `heading` (the target record `target`, its moby +0x40, must exist; a trooper of a train turns only
// while the train rides, its state 4): beyond 45 degrees (unless it turned a moment ago, +0x263, or is in its animation 1) or 90,
// within 20 of the target, it starts its turn (state 12, the state before kept in +0xbc): the animation 1 beyond 67.5 degrees,
// else 9. 1 when it turned.
// orig: L3:0x002c6e18
uint32_t trooper_turn_to_l3(uint32_t m, uint32_t target, float heading) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(target + 0x40) == 0) return 0;
    const int32_t train = lw(pv + 0x264);
    if (train != -1) {
        const uint32_t t = moby_at(train);
        if (lh(t + 0xa6) == 0x336 && lbu(t + 0xbc) != 4) return 0;
    }
    const float diff = angle_sub(heading, lwc1(m + 0x48));
    bool turn;
    if (kQuarterPi <= abs_f(diff) && lbu(pv + 0x263) == 0 && lbu(m + 0x53) != 1) turn = true;
    else turn = u2f(0x3fc90fdbu) < abs_f(diff);
    if (!turn) return 0;
    if (!(vec2_distance(V(m + 0x10), V(target)) < 20.0f)) return 0;
    if (u2f(0x3f96cbe4u) < abs_f(diff)) {
        moby_anim_set(m, 1, 0, frame_scale_i(10));
        swc1(m + 0x58, 1.0f);
    } else {
        moby_anim_set(m, 9, 0, frame_scale_i(10));
    }
    swc1(pv + 0x28c, heading);
    const uint32_t before = lbu(m + 0x20);
    sb(m + 0x20, 0xc);
    sb(m + 0xbc, before);
    return 1;
}

// The trooper's alert and hits: alerted (+0x38), its sight grows by 5 for 240 frames; a hit (kinds 0x330000) the hit reaction
// does not shrug off costs its damage, and by the reaction's kind (or 1 when its health is gone): 1 it is thrown far (state 13,
// the move record's state 7, its sound 3), 3 / 7 / 8 and 4 / 5 knocked back (state 10, the move states 5 and 6), 6 knocked back
// facing away (the animation 5), 12 / 13 a longer colour pulse; each away from the hit's source with its colour pulse.
// orig: L3:0x002c95a8 (the jump table L3:0x1e31e0 on the reaction's kind - 1)
void trooper_hits_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t from = sp + 0x18;
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x38, 0);
        sh(pv + 0x252, static_cast<uint32_t>(frame_scale_i(0xf0)));
    }
    const float sight = lwc1(pv + 0x26c);
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x252))) == 0) swc1(pv + 0x270, fpu_add(sight, 5.0f));
    else swc1(pv + 0x270, sight);
    int32_t state = 0;
    float damage = 0.0f;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    int32_t kind = moby_hit_react_r(m, hit, pv + 0x20, 0, state, &damage, nullptr, 4);
    if (hit != 0 && state != 1 && lbu(m + 0x20) != 0xd) {
        const uint32_t src = lwu(hit + 0x20);
        if (src == 0) swc1(from, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84))));
        else if (src == lwu(0x001403e0)) swc1(from, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(0x0013f3d4))));
        else swc1(from, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14))));
        const float health = fpu_sub(lwc1(pv + 0x20), damage);
        swc1(pv + 0x20, health);
        if (health <= 0.0f) kind = 1;
        auto knock = [&](uint32_t move_set, uint32_t frame) {  // the knock-back's direction and its move state
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(from)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, move_set, 5, frame, lwc1(from));
        };
        auto knocked = [&](float c0, float c4) {  // state 10, its sound
            swc1(pv + 0xc0, c0);
            swc1(pv + 0xc4, c4);
            sb(m + 0x20, 0xa);
            moby_sound_start(1, 0, m);
        };
        switch (kind) {
        case 1: {  // (0x002c9aec)
            knock_arc(pv, 40.0f, true, 4.0f, false, 3.0f, false);
            sw(pv + 0x94, 9);
            sb(pv + 0xad, 0);
            knock(7, 2);
            swc1(pv + 0xc0, 10.0f);
            swc1(pv + 0xc4, 20.0f);
            swc1(pv + 0xbc, fpu_add(dt(), dt()));
            sb(pv + 0x255, 0);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(pv + 0x67, 0xfa);
            sb(m + 0x20, 0xd);
            moby_sound_start(3, 0, m);
            if (lbu(m + 0x53) != 7) moby_anim_set(m, 7, 0, frame_scale_i(6));
            break;
        }
        case 3: case 7: case 8:  // (0x002c98ac)
            sb(pv + 0x67, 0x78);
            knock_arc(pv, 80.0f, true, fpu_add(jump_height(), jump_height()), true, jump_rise(), false);
            sw(pv + 0x94, 9);
            sb(pv + 0xad, 0);
            knock(5, 0);
            knocked(6.0f, 13.0f);
            break;
        case 4: case 5:  // (0x002c99bc)
            sb(pv + 0x67, 0x78);
            knock_arc(pv, 80.0f, true, fpu_add(jump_height(), jump_height()), true, jump_rise(), false);
            sw(pv + 0x94, 9);
            sb(pv + 0xad, 0);
            knock(6, 2);
            knocked(5.0f, 10.0f);
            break;
        case 6: {  // (0x002c9750)
            sb(pv + 0x67, 0x78);
            knock_arc(pv, 80.0f, false, fpu_add(jump_height(), jump_height()), true, jump_rise(), true);
            sw(pv + 0x94, 9);
            sb(pv + 0xad, 0);
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(from)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            if (angle_dist(lwc1(m + 0x48), lwc1(from)) < u2f(0x3f860a92u)) swc1(m + 0x48, angle_add(lwc1(from), kPi));
            move_state_start(m, pv + 0x70, 5, 5, 0, lwc1(from));
            moby_set_anim(m, 5, 5);
            knocked(14.0f, 28.0f);
            break;
        }
        case 12: case 13:  // (0x002c9744)
            sb(pv + 0x67, 0x78);
            break;
        default:  // 2, 9 .. 11 (0x002c9c54) and beyond the table
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0x90);
}

// The trooper's next patrol point: the points of its path (but the nearest to it) seen from it within 90 degrees of the target
// (the target record within its sight +0x270), the one most turned from it; else the nearest beyond 16 of the target (or any
// nearer than 16, the farthest). Its follower's point gets the pick.
// orig: L3:0x002c9ca0
void trooper_pick_point_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t target = sp, point = sp + 0x60;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t fol = pv + 0x120;
    target_select_any(m, target, lwc1(pv + 0x270));
    int32_t nearest = lw(pv + 0x120);
    float dist = 0.0f;
    path_closest_point_v(lwu(fol + 0x10), V(m + 0x10), V(sp + 0x50), &nearest, &dist, 0, 999.0f, 5.0f, 0.0f);
    sw(pv + 0x120, 0);
    int32_t pick = 0;
    float best_d = 0.0f, best_turn = 0.0f;
    for (;;) {
        const int32_t i = lw(fol);
        if (i != nearest) {
            lq_sq(point, lwu(fol + 0x10) + 0x10 + (static_cast<uint32_t>(i) << 4));
            const float turn = angle_dist(heading_to(m, lwc1(point), lwc1(point + 4)), heading_to(m, lwc1(target), lwc1(target + 4)));
            const float d = vec3_distance(V(target), V(point));
            bool take;
            if (turn < u2f(0x3fc90fdbu)) take = best_turn < turn;
            else if (16.0f <= d) take = d < best_d || best_d < 16.0f;
            else take = best_d < d;
            if (take) {
                pick = i;
                best_d = d;
                best_turn = turn;
            }
        }
        const uint32_t path = lwu(fol + 0x10);
        const int32_t next = ee_rem(i + lw(path) + static_cast<int8_t>(lbu(fol + 4)), lw(path));
        sw(fol, static_cast<uint32_t>(next));
        if (next == 0) break;
    }
    sw(fol, static_cast<uint32_t>(pick));
    ee_stack_free(0x100);
}

// A trooper's bolt (class 833) from `at` along `dir` (EE vectors), fired by `owner`, for `life` (60 Hz) frames, its turn `spin`,
// speed `speed` and size `size`: always drawn, heading along `dir`; when the line from the trooper's middle (at the bolt's height)
// to `at` meets the world (flags 2), it starts where it hit, spent (no life, no speed). The bolt, or 0.
// orig: L3:0x002d4288
uint32_t trooper_bolt_spawn_l3(uint32_t dir, uint32_t at, uint32_t owner, int32_t life, float spin, float speed, float size) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t bolt = moby_spawn(0x341);
    if (bolt != 0) {
        sb(bolt + 0x30, 0xff);
        sh(bolt + 0x32, 0x7e);
        sb(bolt + 0x20, 1);
        const uint32_t bpv = lwu(bolt + 0x78);
        sb(bolt + 0x31, 1);
        lq_sq(bolt + 0x10, at);
        lq_sq(bpv, dir);
        swc1(bolt + 0x48, atan2_f(lwc1(bpv), lwc1(bpv + 4)));
        sw(bpv + 0x14, static_cast<uint32_t>(frame_scale_i(life)));
        swc1(bpv + 0x18, size);
        swc1(bpv + 0x1c, spin);
        swc1(bpv + 0x24, speed);
        sw(bpv + 0x10, owner);
        sw(bpv + 0x28, 0);
        lq_sq(sp, owner + 0x10);
        swc1(sp + 8, lwc1(at + 8));
        if (segment_collide_v(V(sp), V(at), 2, owner, nullptr) != 0) {
            sw(bpv + 0x14, 0);
            V(bolt + 0x10) = coll_hit_point();
            sw(bpv + 0x24, 0);
        }
        moby_update_transform(bolt);
    }
    ee_stack_free(0xa0);
    return bolt;
}

// Class 574, the Blarg trooper. Its mode 1 (+0x262) is a spare of a checkpoint: hidden while its spawn's checkpoint mark is
// used up, nothing else. Else it is hidden in the scenes (game mode 2), removed outside 2 .. 1021 or without a path (but thrown,
// 13). Each frame: its hits, its head and body turned toward the target (on screen or within 24), its state, standing on a
// moby (classes 821 / 1210: its path and its place carried with it), gravity and its fall, and on the level (mode 0) a trooper
// within 70 of the camera that fell 10 below its path (or below z 5) blows up. The state (+0x20):
// 0 its set-up (walker record, speeds, health 3, its path's start, its legs' lengths in the points' w) -> 1, or 2 waiting;
// 1 stands: on a second path (+0x290) it sets off along it (-> 9) when its group is busy, the target is in its box or its spawn's
//   mark used up, or the target within its reach and level; else it turns to the target, walks its path toward a target in
//   its approach range (-> 8), strikes a target within 3.5 (-> 6), and aims at one in sight after its wait (-> 4);
// 2 waits for its moby (+0x284) with its call (the animation 10) while a target is in sight; -> 1 when the moby is gone;
// 4 aims: -> 5 at the animation's end; a target within 3.5 is struck, one within its fire range makes it move on (-> 7);
// 5 fires at the animation's frame 8 (its bolt within 30 degrees of its facing, a spark at the muzzle); at the end, after a
//   shot, it walks on, turns, strikes or aims again;
// 6 strikes at frames 25 .. 31 (a hit record on the target ahead within 3.5); at the end it moves on;
// 7 walks to its patrol point (doomlet_259028), then moves on or goes back to 1;
// 8 walks its path point by point; 9 walks its second path, at its end -> 1 (or 2);
// 10 knocked back: on landing it moves on, falling below z 5 (or into the world) it blows up, a long fall -> 11;
// 11 falls (its move record), stopped by the world, below z 5 it blows up; then it turns as 12;
// 12 turns to +0x28c (the animation 1 or 9) and returns to its state +0xbc (7: a new patrol point; others -> 1);
// 13 thrown: lands with its sound, blows up on the ground; 14 waits hidden at its path's start; 15 comes back (-> 1).
// Blowing up: an explosion, three pieces of debris (0x6d0 .. 0x6d2) and its bolts; a trooper of a train comes back hidden at its
// path's start (-> 14), others are removed.
// orig: L3:0x002c6fd0 (the jump table L3:0x1e31a0 on the state, in order)
void kerwan_class574_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x190);
    const uint32_t T = sp + 0x20;  // the target record: its position, +0x20 its aim point, +0x40 its moby
    const uint32_t pv = lwu(m + 0x78);
    target_select_any(m, T, lwc1(pv + 0x270));
    const float dist = vec2_distance(V(m + 0x10), V(T));
    moby_set_70(pv + 0x150, 2.5f);
    auto done = [&] { ee_stack_free(0x190); };
    auto hide = [&] {
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    };
    if (lbu(pv + 0x262) == 1) {
        if (spawn_mark(m) == 0xff) hide();
        done();
        return;
    }
    if (lw(OV(0x0015f5c4)) == 2) sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    else if (lbu(m + 0x20) != 0xe) sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(m + 0x10 + k * 4);
        if (c < 2.0f || 1021.0f < c) {
            moby_retire(m);
            done();
            return;
        }
    }
    if (lw(pv + 0x134) == -1 && lbu(m + 0x20) != 0xd) {
        moby_retire(m);
        done();
        return;
    }
    trooper_hits_l3(m);
    countdown_u8(pv + 0x263);
    // the head (+0x1b8) and body (+0x238) turned toward the target, within 45 degrees
    if (lbu(m + 0x31) != 0 || dist < 24.0f) {
        const uint32_t st = lbu(m + 0x20);
        if (st == 0xc) {
            swc1(pv + 0x1b8, fpu_mul(angle_sub(lwc1(pv + 0x28c), lwc1(m + 0x48)), 0.25f));
            swc1(pv + 0x238, fpu_mul(angle_sub(lwc1(pv + 0x28c), lwc1(m + 0x48)), 0.75f));
        } else if (st < 7 && dist < lwc1(pv + 0x270)) {
            swc1(pv + 0x238, angle_sub(heading_to(m, lwc1(T), lwc1(T + 4)), lwc1(m + 0x48)));
        }
        for (uint32_t a : {0x1b8u, 0x238u}) {
            const float v = lwc1(pv + a);
            if (kQuarterPi < v) swc1(pv + a, kQuarterPi);
            else if (v < -kQuarterPi) swc1(pv + a, -kQuarterPi);
        }
        joint_node_spring(m, pv + 0x150, 2, fpu_mul(ratio(), u2f(0x3cf5c28fu)), fpu_mul(ratio(), u2f(0x3e99999au)));
        joint_node_spring(m, pv + 0x1d0, 3, fpu_mul(ratio(), u2f(0x3cf5c28fu)), fpu_mul(ratio(), u2f(0x3e99999au)));
    }

    auto dz = [&] { return abs_f(fpu_sub(lwc1(T + 8), lwc1(m + 0x18))); };
    auto anim_to = [&](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    auto turn_check = [&] { return trooper_turn_to_l3(m, T, heading_to(m, lwc1(T), lwc1(T + 4))) != 0; };
    auto strike_if_close = [&] {  // a target within 3.5 and level: the strike (state 6)
        if (!(dist < 3.5f) || !(dz() < 3.0f)) return false;
        sb(m + 0x20, 6);
        anim_to(2, frame_scale_i(10));
        return true;
    };
    auto to_path_walk = [&] {  // walk to the next point of its path (state 7) when there is one; false at its end
        const int32_t i = lw(pv + 0x120);
        if (!(i < lw(lwu(pv + 0x130)) - 1)) return false;
        sw(pv + 0x120, static_cast<uint32_t>(i + 1));
        sb(m + 0x20, 7);
        anim_to(1, frame_scale_i(10));
        return true;
    };
    auto to_patrol = [&] {  // a new patrol point (state 7)
        trooper_pick_point_l3(m);
        sb(m + 0x20, 7);
        anim_to(1, frame_scale_i(10));
    };
    auto to_idle = [&] {  // back to 1 (the animation 0), turning to the target
        sb(m + 0x20, 1);
        anim_to(0, frame_scale_i(10));
        turn_check();
    };
    auto move_on = [&](bool idle_at_end) {  // after a shot or a strike: along its path, or a new patrol point
        if (lw(pv + 0x274) != 0) {
            if (!to_path_walk() && idle_at_end) to_idle();
        } else {
            to_patrol();
        }
    };
    // Blowing up: the explosion at its place (1 up), its debris, its bolts (`bolt_flags`); a trooper of a train waits hidden at
    // its path's start, others are removed. True: the frame ends here.
    auto blow_up = [&](uint32_t bolt_flags, bool explode) {
        if (explode) {
            const uint32_t at = sp + 0x70;
            lq_sq(at, m + 0x10);
            swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
            explosion_effect(m, pv + 0x40, at, 5, 2, 4, 6, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
            const uint32_t zero = OV(0x0015f580);
            debris_spawn(m, 0x6d0, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            debris_spawn(m, 0x6d1, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            debris_spawn(m, 0x6d2, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        }
        moby_drop_bolts(m, bolt_flags, -1);
        if (lw(pv + 0x264) == -1) {
            moby_retire(m);
            return;
        }
        lq_sq(m + 0x10, lwu(pv + 0x130) + 0x10);
        vec_clear(V(pv + 0x40));
        sw(pv + 0xe8, 0);
        sb(m + 0x20, 0xe);
        hide();
    };

    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002c7324)
        sb(m + 0x7f, 0x1c);
        sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));
        walker_record_init_l1(pv + 0xd0);
        swc1(pv + 0xdc, 3.0f);
        sw(pv + 0xe8, 0);
        swc1(pv + 0x114, fpu_mul(dt(), kPi));
        swc1(pv + 0xf4, fpu_mul(dt(), 6.0f));
        swc1(pv + 0xd8, 2.0f);
        swc1(pv + 0xf8, kPi);
        swc1(pv + 0x110, fpu_mul(dt_sq(), u2f(0x41490fdbu)));
        swc1(pv + 0x10c, fpu_mul(dt_sq(), u2f(0x40c90fdbu)));
        const float fifteen = frame_scale_f(15.0f);
        swc1(pv + 0x20, 3.0f);
        swc1(pv + 0xfc, fpu_mul(lwc1(pv + 0xf4), fifteen));
        sw(pv + 0x108, 9);
        sb(pv + 0x29, 1);
        sb(pv + 0x28, 2);
        swc1(pv + 0x30, 1.75f);
        swc1(pv + 0x98, u2f(0x3f333333u));
        sh(pv + 0x24, static_cast<uint32_t>(float_to_int(3.0f)));
        sw(pv + 0xbc, 0);
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(u2f(0x44333333u))));
        sb(pv + 0x58, 10);
        sb(pv + 0x5a, 12);
        vec_clear(V(pv + 0x40));
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sh(pv + 0x250, 0);
        const uint32_t path = lvd::path(lw(pv + 0x134));
        sw(pv + 0x120, 0);
        sw(pv + 0x130, path);
        lq_sq(m + 0x10, path + 0x10);
        sb(m + 0x20, 1);
        sh(pv + 0x288, lhu(m + 0x32));
        sh(pv + 0x28a, lhu(L3_ADDR(0x001618f4)) & 7u);
        sw(L3_ADDR(0x001618f4), lwu(L3_ADDR(0x001618f4)) + 1);
        sb(pv + 0x263, 0);
        if (path != 0) {  // each leg's length in its first point's w
            for (int32_t i = 0; i < lw(path) - 2; ++i)
                swc1(path + 0x1c + static_cast<uint32_t>(i) * 16,
                     vec3_distance(V(path + 0x10 + static_cast<uint32_t>(i) * 16), V(path + 0x20 + static_cast<uint32_t>(i) * 16)));
        }
        if (lw(pv + 0x284) != -1 && lw(pv + 0x290) == -1) {
            moby_anim_set(m, 0, 0, frame_scale_i(10));
            sb(m + 0x20, 2);
        }
        break;
    }
    case 1: {  // (0x002c752c)
        if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x52) != 0) moby_anim_set(m, 0, 0, frame_scale_i(10));
        if (lw(pv + 0x290) != -1) {
            const uint32_t path = lvd::path(lw(pv + 0x290));
            lq_sq(m + 0x10, path + 0x10);
            swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
            bool go = walker_group_busy_l3(lbu(m + 0x21)) != 0 || point_in_box(T, lw(pv + 0x29c)) != 0 || spawn_mark(m) == 0xff;
            if (!go) go = dist < lwc1(pv + 0x294) && dz() < 4.0f;
            if (!go) break;
            sw(pv + 0x298, 0);
            sb(m + 0x20, 9);
            anim_to(1, frame_scale_i(10));
            break;
        }
        if (lw(pv + 0x284) != -1) {
            moby_anim_set(m, 0, 0, frame_scale_i(10));
            sb(m + 0x20, 2);
            break;
        }
        if (dist < lwc1(pv + 0x270) || dist < lwc1(pv + 0x27c)) {
            if (turn_check()) break;
            if (dist < lwc1(pv + 0x27c) && dz() < 4.0f) {
                sw(pv + 0x27c, 0);
                sh(pv + 0x256, 1);
                sb(m + 0x20, 8);
                anim_to(1, frame_scale_i(10));
            }
        }
        if (strike_if_close()) break;
        if (!(dist < lwc1(pv + 0x270)) || !(abs_f(fpu_sub(lwc1(T + 8), lwc1(m + 0x18))) < 10.0f)) break;
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x250))) == 0) break;
        if (!(angle_dist(lwc1(m + 0x48), heading_to(m, lwc1(T), lwc1(T + 4))) < kQuarterPi)) break;
        if (lbu(m + 0x31) == 0) break;  // (the original skips the aim when it is not always drawn)
        sb(m + 0x20, 4);
        anim_to(3, frame_scale_i(10));
        break;
    }
    case 2: {  // (0x002c7868)
        if (dist < lwc1(pv + 0x270) && dz() < 4.0f && lbu(m + 0x53) != 10) {
            moby_anim_set(m, 10, 0, frame_scale_i(10));
            break;
        }
        const uint32_t w = moby_at(lw(pv + 0x284));
        bool gone = w == 0 || lbu(w + 0x20) == 0xfe || lbu(w + 0x20) == 0xfd;
        if (!gone && lbu(m + 0x52) == 10 && (lbu(m + 0x70) & 2u) != 0) gone = true;
        if (!gone) {
            turn_check();
            break;
        }
        sb(m + 0x20, 1);
        moby_anim_set(m, 0, 0, frame_scale_i(10));
        sh(pv + 0x250, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sw(pv + 0x284, 0xffffffffu);
        break;
    }
    case 4: {  // (0x002c7950)
        if ((lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 5);
            sb(pv + 0x254, 0);
            if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, 0);
            break;
        }
        if (strike_if_close()) break;
        if (!(dist < lwc1(pv + 0x278)) || !(abs_f(fpu_sub(lwc1(T + 8), lwc1(m + 0x18))) < 10.0f)) break;
        move_on(false);
        break;
    }
    case 5: {  // (0x002c7aac)
        if (anim_crossed(m, 8.0f) != 0) {
            const uint32_t muzzle = sp + 0x70, dir = sp + 0x80, flash = sp + 0x90;
            sb(pv + 0x254, lbu(pv + 0x254) + 1);
            moby_chain_point_v(m, 0, V(muzzle));
            vec3_sub(V(dir), V(T + 0x20), V(muzzle));
            vec3_set_length(V(dir), V(dir), fpu_mul(dt(), 6.0f));
            swc1(dir + 8, fpu_mul(lwc1(dir + 8), 0.5f));
            float heading = atan2_f(lwc1(dir), lwc1(dir + 4));
            const float off = angle_sub(heading, lwc1(m + 0x48));
            const float limit = u2f(0x3f060a92u);
            if (limit < off || off < -limit) {  // kept within 30 degrees of its facing
                heading = angle_add(lwc1(m + 0x48), limit < off ? limit : -limit);
                swc1(dir, fpu_mul(cos_vu(heading), fpu_mul(dt(), 6.0f)));
                sw(dir + 8, 0);
                swc1(dir + 4, fpu_mul(sin_vu(heading), fpu_mul(dt(), 6.0f)));
            }
            vec3_scale(V(flash), V(dir), frame_scale_f(10.0f));
            vec3_add_inplace(V(flash), V(muzzle));
            spark_burst(m, flash, 0);
            trooper_bolt_spawn_l3(dir, muzzle, m, frame_scale_i(0x82), heading, fpu_mul(dt(), 6.0f), 0.75f);
        }
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lbu(pv + 0x254) == 0) break;
            if (dist < lwc1(pv + 0x27c) && dz() < 4.0f) {
                sw(pv + 0x27c, 0);
                sb(m + 0x20, 8);
                anim_to(1, frame_scale_i(10));
            }
            if (turn_check()) break;
            if (dist < lwc1(pv + 0x270) && abs_f(fpu_sub(lwc1(T + 8), lwc1(m + 0x18))) < 10.0f &&
                timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x250))) != 0 &&
                angle_dist(lwc1(m + 0x48), heading_to(m, lwc1(T), lwc1(T + 4))) < u2f(0x3db2b8c2u) && lbu(m + 0x31) != 0)
                break;  // (aimed and ready: the original goes straight to the frame's end)
            strike_if_close();
            break;
        }
        if (dist < 3.5f && dz() < 3.0f) {
            if (turn_check()) break;
            sb(m + 0x20, 6);
            anim_to(2, frame_scale_i(10));
            break;
        }
        if (!(dist < lwc1(pv + 0x278)) || !(abs_f(fpu_sub(lwc1(T + 8), lwc1(m + 0x18))) < 10.0f)) break;
        move_on(false);
        break;
    }
    case 6: {  // (0x002c7f68)
        const float t = moby_anim_time(m);
        yaw_turn_damped(m, pv + 0x258, heading_to(m, lwc1(T), lwc1(T + 4)), u2f(0x3cf5c28fu), u2f(0x3e99999au), u2f(0x3e99999au));
        moby_chain_point_v(m, 0, V(sp + 0x70));
        if (25.0f <= t && t <= 31.0f && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(T + 8))) < 2.0f &&
            vec2_distance(V(m + 0x10), V(T)) < 3.5f &&
            angle_dist(heading_to(m, lwc1(T), lwc1(T + 4)), lwc1(m + 0x48)) < u2f(0x3e860a92u)) {
            // the blow: a hit record ahead of it (kind 0x10001, the attacker, its class)
            const uint32_t rec = sp + 0x80;
            sw(rec + 0x14, 0x10001);
            sw(rec + 0x10, m);
            swc1(rec + 0x1c, 1.0f);
            swc1(rec, fpu_mul(cos_vu(lwc1(m + 0x48)), u2f(0x3e4ccccdu)));
            swc1(rec + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), u2f(0x3e4ccccdu)));
            sw(rec + 8, 0);
            vec2_set_length(V(rec), V(rec), 1.0f);
            swc1(rec + 8, 1.0f);
            swc1(rec + 0xc, u2f(0x45afdf66u));
            sb(rec + 0x19, 1);
            sh(rec + 0x1a, lhu(m + 0xa6));
            sb(rec + 0x18, 0);
            if (lwu(T + 0x40) != 0) moby_hit_record(lwu(T + 0x40), rec);
            break;
        }
        if ((lbu(m + 0x70) & 2u) == 0) break;
        move_on(true);
        break;
    }
    case 7: {  // (0x002c81e8)
        if (lh(pv + 0x256) != 0) {
            sb(m + 0x20, 8);
            break;
        }
        const uint32_t point = lwu(pv + 0x130) + (static_cast<uint32_t>(lw(pv + 0x120)) << 4);
        if (trooper_turn_to_l3(m, T, atan2_f(fpu_sub(lwc1(point + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(point + 0x14), lwc1(m + 0x14)))) != 0) break;
        const uint32_t goal = lwu(pv + 0x130) + 0x10 + (static_cast<uint32_t>(lw(pv + 0x120)) << 4);
        if ((static_cast<uint32_t>(doomlet_259028(m, pv + 0xd0, goal, pv + 0x40)) & 0x14u) == 0) break;
        if (dist < lwc1(pv + 0x278) && abs_f(fpu_sub(lwc1(T + 8), lwc1(m + 0x18))) < 10.0f) move_on(true);
        else to_idle();
        break;
    }
    case 8: {  // (0x002c838c)
        int32_t nearest = 0;
        float d = 0.0f;
        path_closest_point_v(lwu(pv + 0x130), V(m + 0x10), V(sp + 0x70), &nearest, &d, 0, 999.0f, 5.0f, 0.0f);
        if (lw(pv + 0x120) < nearest) sw(pv + 0x120, static_cast<uint32_t>(nearest));
        const uint32_t goal = lwu(pv + 0x130) + 0x10 + (static_cast<uint32_t>(lw(pv + 0x120)) << 4);
        if ((static_cast<uint32_t>(doomlet_259028(m, pv + 0xd0, goal, pv + 0x40)) & 0x14u) == 0) break;
        if (lw(pv + 0x120) < lw(lwu(pv + 0x130)) - 1) sw(pv + 0x120, lwu(pv + 0x120) + 1);
        else to_idle();
        break;
    }
    case 9: {  // (0x002c8460)
        const uint32_t path = lvd::path(lw(pv + 0x290));
        int32_t nearest = 0;
        float d = 0.0f;
        path_closest_point_v(path, V(m + 0x10), V(sp + 0x70), &nearest, &d, 0, 999.0f, 5.0f, 0.0f);
        if (lw(pv + 0x298) < nearest) sw(pv + 0x298, static_cast<uint32_t>(nearest));
        const uint32_t goal = path + 0x10 + (static_cast<uint32_t>(lw(pv + 0x298)) << 4);
        if ((static_cast<uint32_t>(doomlet_259028(m, pv + 0xd0, goal, pv + 0x40)) & 0x14u) == 0) break;
        if (lw(pv + 0x298) < lw(path) - 1) {
            sw(pv + 0x298, lwu(pv + 0x298) + 1);
            break;
        }
        sw(pv + 0x290, 0xffffffffu);
        sb(m + 0x20, 1);
        anim_to(0, frame_scale_i(10));
        if (lw(pv + 0x284) == -1) {
            turn_check();
            break;
        }
        moby_anim_set(m, 0, 0, frame_scale_i(10));
        sb(m + 0x20, 2);
        break;
    }
    case 10: {  // (0x002c85ac)
        uint32_t r;
        if (lh(pv + 0xae) == 2) {
            swc1(m + 0x58, 1.0f);
            r = (static_cast<uint32_t>(lbu(m + 0x70)) << 5) & 0x40u;
        } else {
            r = enemy_physics(m, pv + 0x70);
        }
        sw(pv + 0x290, 0xffffffffu);
        if ((r & 0x40u) != 0) {  // landed: it moves on
            if (lw(pv + 0x274) != 0) {
                if (!to_path_walk()) to_idle();
            } else {
                to_patrol();
            }
        } else if (lwc1(m + 0x18) < 5.0f || (r & 0x20u) != 0) {
            blow_up(0, true);
            done();
            return;
        }
        // (the landing's height test: a long way above the floor, it falls)
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 2.0f));
        const float floor = floor_probe_20(m + 0x10);
        const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
        swc1(m + 0x18, z);
        if (fpu_add(floor, 5.0f) < z) {
            moby_sound_start(10, 0x21, m);
            moby_anim_set(m, 0xb, 0, frame_scale_i(10));
            sb(m + 0x20, 0xb);
        }
        break;
    }
    case 11: case 12: {
        if (lbu(m + 0x20) == 11) {  // (0x002c88cc) the fall
            const uint32_t mv = pv + 0x70, from = sp + 0x70, to = sp + 0x80, speed = sp + 0xc0;
            swc1(mv + 8, fpu_sub(lwc1(mv + 8), lwc1(mv + 0x10)));
            swc1(speed, vec2_length(V(mv)));
            approach_f(speed, lh(mv + 0x3e) == 2 ? 0.0f : lwc1(mv + 0x4c), lwc1(mv + 0x14));
            vec2_set_length(V(mv), V(mv), lwc1(speed));
            lq_sq(from, m + 0x10);
            swc1(from + 8, fpu_add(lwc1(from + 8), lwc1(mv + 0x28)));
            vec3_add(V(m + 0x10), V(m + 0x10), V(mv));
            lq_sq(to, m + 0x10);
            swc1(to + 8, fpu_add(lwc1(to + 8), lwc1(mv + 0x28)));
            if (static_cast<float>(static_cast<int32_t>(lwu(mv + 0x20))) < vec3_length(V(mv))) {
                segment_collide_v(V(from), V(to), 0x24, m, nullptr);
                V(to) = coll_hit_point();
            }
            if (sphere_collide(to, fpu_mul(int_to_float(lw(mv + 0x20)), u2f(0x3a800000u)), 0x24, m) != 0) {
                V(m + 0x10) = coll_hit_pushed();
                swc1(m + 0x18, fpu_sub(fpu_add(lwc1(from + 8), lwc1(mv + 8)), lwc1(mv + 0x28)));
            }
            if (lwc1(m + 0x18) < 5.0f) {
                blow_up(0x200, true);
                done();
                return;
            }
        }
        // (0x002c8b64) the turn
        const uint32_t anim = lbu(m + 0x53);
        if (anim != 1 && anim != 9) break;
        if (anim == 1) yaw_turn_damped(m, pv + 0x258, lwc1(pv + 0x28c), u2f(0x3db851ecu), u2f(0x3e8a3d71u), u2f(0x3e19999au));
        else yaw_turn_damped(m, pv + 0x258, lwc1(pv + 0x28c), u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3e4ccccdu));
        if (!(angle_dist(lwc1(m + 0x48), lwc1(pv + 0x28c)) < u2f(0x3d0efa35u))) break;
        sb(pv + 0x263, static_cast<uint32_t>(frame_scale_i(0x3c)));
        const uint32_t back = lbu(m + 0xbc);
        if (back == 7 || back == 8) {
            if (back == 7) trooper_pick_point_l3(m);
            sb(m + 0x20, lbu(m + 0xbc));
            anim_to(1, frame_scale_i(10));
            if (anim == 1) swc1(m + 0x58, 1.0f);
        } else {
            sb(m + 0x20, 1);
            anim_to(0, frame_scale_i(10));
            sh(pv + 0x250, static_cast<uint32_t>(frame_scale_i(10)));
        }
        break;
    }
    case 13: {  // (0x002c8d48)
        const uint32_t r = enemy_physics(m, pv + 0x70);
        sb(pv + 0x255, lbu(pv + 0x255) + 1);
        const float floor = floor_probe_20(m + 0x10);
        if ((r & 0x60u) != 0 && fpu_add(floor, 5.0f) < lwc1(m + 0x18) && lbu(m + 0xbc) != 0xb) {
            moby_sound_start(10, 0, m);
            sb(m + 0xbc, 0xb);
        }
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        if ((r & 1u) != 0) sb(pv + 0x255, 0);
        if ((r & 0x60u) != 0) {
            blow_up(0, true);
            done();
            return;
        }
        if (lwc1(m + 0x18) < 5.0f) {
            blow_up(0, false);
            done();
            return;
        }
        break;
    }
    case 14:  // (0x002c8f74)
        anim_to(0, frame_scale_i(10));
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        lq_sq(m + 0x10, lwu(pv + 0x130) + 0x10);
        sw(m + 0x94, 0);
        break;
    case 15:  // (0x002c8fc4)
        if (spawn_mark(m) == 0xff) {
            moby_retire(m);
            done();
            return;
        }
        sh(m + 0x34, (lhu(m + 0x34) & 0xffbeu) | 0x1000u);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        swc1(pv + 0x20, int_to_float(lh(pv + 0x24)));
        lq_sq(m + 0x10, lwu(pv + 0x130) + 0x10);
        sb(m + 0x20, 1);
        break;
    default:  // 3 (0x002c9038) and beyond the table
        break;
    }

    // ---- the frame's end (0x002c9038)
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(camera_pos())) < 36.0f) moby_floor_band(m);
    // the moby it stands on: gone, it stands on nothing; a carriage (1210) or class 821 carries its path's points and itself
    {
        const int32_t on = lw(pv + 0x280);
        if (on != -1) {
            const uint32_t o = moby_at(on);
            if (o == 0 || lbu(o + 0x20) == 0xfe || lbu(o + 0x20) == 0xfd) sw(pv + 0x280, 0xffffffffu);
        } else {
            sw(pv + 0x280, 0xffffffffu);
        }
    }
    if (lw(pv + 0x280) != -1) {
        const uint32_t moved_ang = sp + 0x70;
        sd(moved_ang, 0);
        sd(moved_ang + 8, 0);
        const uint32_t o = moby_at(lw(pv + 0x280));
        const int16_t cls = lh(o + 0xa6);
        if (cls == 0x335 || cls == 0x4ba) {
            const uint32_t path = lwu(pv + 0x130);
            for (int32_t i = 0; i < lw(path); ++i) {
                const uint32_t ang = sp + 0x80;
                sd(ang, 0);
                sd(ang + 8, 0);
                const uint32_t pt = lwu(pv + 0x130) + 0x10 + static_cast<uint32_t>(i) * 16;
                sub_00260400(m, o, pt, ang, pt, ang);
            }
        }
        sub_00260400(m, o, m + 0x10, m + 0x40, m + 0x10, moved_ang);
        swc1(m + 0x48, lwc1(moved_ang + 8));
    }
    // gravity (but in states 11 .. 14) and the floor: landing on a moby of classes 821 / 1210 makes it the one it stands on
    const uint32_t st = lbu(m + 0x20);
    if (!(st >= 0xb && st <= 0xe)) {
        if (st == 0xa) sw(pv + 0xe8, 0);
        else swc1(pv + 0xe8, fpu_add(lwc1(pv + 0xe8), fpu_mul(dt_sq(), u2f(0x411ccccdu))));
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_sub(lwc1(pv + 0xe8), 2.0f)));
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
        swc1(m + 0x18, z);
        if (z < floor) {
            const uint32_t hit = coll_hit_moby();
            if (hit != 0 && moby_pvar_word2(hit) != 0) {
                sw(pv + 0x280, static_cast<uint32_t>(static_cast<int32_t>(hit - lwu(OV(0x0015ffd8))) >> 8));
            } else if (lw(pv + 0x280) != -1) {
                const int16_t cls = lh(moby_at(lw(pv + 0x280)) + 0xa6);
                if (cls != 0x335 && cls != 0x4ba) sw(pv + 0x280, 0xffffffffu);
            }
            swc1(m + 0x18, floor);
            sw(pv + 0xe8, 0);
            if (lw(pv + 0x280) != -1) {
                const uint32_t n = sp + 0x70, a = sp + 0x80;
                lq_sq(a, m + 0x40);
                V(n) = coll_hit_normal();
                trooper_floor_tilt_l3(n, a, lwc1(m + 0x48));
            }
        } else if (fpu_add(floor, 5.0f) < z) {
            moby_anim_set(m, 0xb, 0, frame_scale_i(10));
            sb(m + 0x20, 0xb);
            vec_clear(V(pv + 0x70));
            swc1(pv + 0x78, -lwc1(pv + 0xe8));
            swc1(pv + 0x80, fpu_mul(dt_sq(), u2f(0x419ccccdu)));
        }
    }
    // on the level, near the camera, on its eighth of the frames: fallen 10 below its path (or below z 5), it blows up
    if (lbu(pv + 0x262) != 0) {
        done();
        return;
    }
    if (!(vec2_distance(V(m + 0x10), V(camera_pos())) < 70.0f)) {
        done();
        return;
    }
    if (static_cast<int32_t>(lwu(OV(0x0015f5cc)) & 7u) != lh(pv + 0x28a)) {
        done();
        return;
    }
    int32_t nearest = 0;
    float d = 0.0f;
    path_closest_point_v(lwu(pv + 0x130), V(m + 0x10), V(sp + 0x70), &nearest, &d, 0, 999.0f, 5.0f, 0.0f);
    if (lwc1(m + 0x18) < 5.0f || lwc1(m + 0x18) < fpu_sub(lwc1(sp + 0x78), 10.0f)) {
        const uint32_t at = sp + 0x80;
        lq_sq(at, m + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
        explosion_effect(m, pv + 0x40, at, 5, 2, 4, 6, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        const uint32_t zero = OV(0x0015f580);
        debris_spawn(m, 0x6d0, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        debris_spawn(m, 0x6d1, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        debris_spawn(m, 0x6d2, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        blow_up(0x200, false);
    }
    done();
}

// Class 833, the trooper's bolt (its pvars: +0 its step, +0x10 the trooper, +0x14 its life, +0x18 its size, +0x1c its heading, +0x20
// its pitch's rate, +0x24 its speed). State 1 flies: its step plus the hero's drift (0x13f490, its z scaled by the level's word),
// spinning, a trail of two particle 4s behind it, its pitch turned toward a target within twice its life's range; meeting the world
// or a moby (not its trooper) it hits that moby (a hit record of kind 0x10001 along its heading) and bursts (-> 2), as at its life's
// end. State 2: a small burst and gone.
// orig: L3:0x002d43c8
void kerwan_class833_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 2) {
        explosion_burst(m, m + 0x10, 0, 0.25f, 13.0f);
        moby_retire(m);
        ee_stack_free(0x160);
        return;
    }
    if (st != 1) {
        ee_stack_free(0x160);
        return;
    }
    const uint32_t T = sp + 0x40, from = sp, trail = sp + 0x10, behind = sp + 0x20, drift = sp + 0x30, sum = sp + 0x90;
    target_select_any(m, T, fpu_mul(int_to_float(lw(pv + 0x14) << 1), lwc1(pv + 0x24)));
    lq_sq(from, m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    lq_sq(drift, 0x0013f490);
    swc1(drift + 8, fpu_mul(lwc1(drift + 8), lwc1(L3_ADDR(0x00161b28))));
    vec3_add_inplace(V(m + 0x10), V(drift));
    swc1(m + 0x48, lwc1(pv + 0x1c));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(dt(), u2f(0x40c90fdbu))));
    swc1(trail, fpu_mul(fpu_mul(cos_vu(lwc1(pv + 0x1c)), lwc1(pv + 0x24)), -0.5f));
    swc1(trail + 4, fpu_mul(fpu_mul(sin_vu(lwc1(pv + 0x1c)), lwc1(pv + 0x24)), -0.5f));
    sw(trail + 8, 0);
    vec3_add(V(sum), V(trail), V(pv));
    lq_sq(trail, sum);
    vec3_add(V(sum), V(trail), V(drift));
    lq_sq(trail, sum);
    vec3_set_length(V(behind), V(pv), -0.2f);
    vec3_add_inplace(V(behind), V(m + 0x10));
    {
        const int32_t life = frame_scale_i(rand_range(0xf, 0x16));
        const int32_t size = rand_range(0x14, 0x23);
        spawn_particle_4(behind, trail, 0x6f00afffu, 0xff, static_cast<uint32_t>(life), 0x28, static_cast<uint32_t>(size), 1);
    }
    {
        const int32_t life = frame_scale_i(rand_range(0x1e, 0x3c));
        const int32_t size = rand_range(0x32, 0x4b);
        spawn_particle_4(behind, trail, 0x1fffffffu, 0x4f4f4fu, static_cast<uint32_t>(life), 0x28, static_cast<uint32_t>(size), 0);
    }
    if (lwu(T + 0x40) != 0) {  // its pitch toward the target's aim point
        const float d = vec2_distance(V(m + 0x10), V(T + 0x20));
        const float to = -atan2_f(d, fpu_sub(lwc1(T + 0x28), lwc1(m + 0x18)));
        const float pitch = angle_seek_rate(pv + 0x20, lwc1(m + 0x44), to, fpu_mul(dt_sq(), kQuarterPi), fpu_mul(dt_sq(), kPi),
                                            fpu_mul(dt(), u2f(0x3f060a92u)));
        swc1(m + 0x44, pitch);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(sin_vu(-pitch), lwc1(pv + 0x24))));
    }
    if (segment_collide_v(V(m + 0x10), V(from), 0, lwu(pv + 0x10), nullptr) != 0 || sphere_collide(m + 0x10, 0.2f, 0, lwu(pv + 0x10)) != 0) {
        const uint32_t victim = coll_hit_moby();
        if (victim != 0) {
            const uint32_t rec = sp + 0xa0;
            sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, pv);
            swc1(rec, cos_vu(lwc1(pv + 0x1c)));
            swc1(rec + 4, sin_vu(lwc1(pv + 0x1c)));
            swc1(rec + 8, 1.0f);
            swc1(rec + 0xc, u2f(0x45afdf66u));
            sb(rec + 0x19, st);
            sh(rec + 0x1a, lhu(m + 0xa6));
            sb(rec + 0x18, st);
            moby_hit_record(coll_hit_moby(), rec);
        }
        sb(m + 0x20, 2);
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0) sb(m + 0x20, 2);
    ee_stack_free(0x160);
}

}  // namespace rac1
