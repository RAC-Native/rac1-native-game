// Batalia (level 8) own moby classes, hand-ported: class 452, the Bolt Thief (three on the level). It waits by its path
// until Ratchet comes near, then runs along it scooping up the "bolt critters" it was given (class 633, a cut creature
// with no model: none are on the shipped level, so its list stays empty), and runs away along a second path, keeping out
// of Ratchet's reach; hit once it is thrown, drops its bolts and bursts on landing. With its helper that picks its
// critters. Registered in batalia_mobys.cpp. Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR (its speed
// 0x161c50, the camera's position 0x1675c0), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/devastator.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the hero's moby
constexpr int32_t kCritters = 50;           // the slots of the critter list (pvars +0x158)

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }

// a path's point `i` (the points from +0x10, 0x10 each)
uint32_t path_pt(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }

// the follower record `rec`'s next point (+0 the point's index, +4 the direction, a signed byte, +0x10 the path)
int32_t next_point(uint32_t rec) {
    const uint32_t path = lwu(rec + 0x10);
    const uint32_t dir = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(rec + 4))));
    return ee_rem(static_cast<int32_t>(lwu(rec) + lwu(path) + dir), lw(path));
}

// the animation set `set` started over `frames` frames, unless it plays
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
}

// the thief turning towards the point `p` (its rate pvar +0x224)
void turn_to(uint32_t m, uint32_t pv, uint32_t p) {
    yaw_turn_damped(m, pv + 0x224, heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(p), lwc1(p + 4)), 0.01f, 0.3f, 0.1f);
}

// the thief stepped towards `goal` (EE vectors; `step` the step) by its speed 0x161c50 a second; true within 0.5 of it
bool step_to(uint32_t m, uint32_t goal, uint32_t step) {
    vec3_sub(V(step), V(goal), V(m + 0x10));
    vec3_set_length(V(step), V(step), fpu_mul(lwc1(L8_ADDR(0x00161c50)), dt()));
    vec3_add(V(m + 0x10), V(m + 0x10), V(step));
    return vec3_distance(V(m + 0x10), V(goal)) < 0.5f;
}
}  // namespace

// The thief `m`'s critter list (pvars +0x158) refilled from its ten moby indices +0x130: those of class 633, in order
// from the list's start (the slots after them keep what they held).
// orig: L8:0x002e2da0
void bolt_thief_gather_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t table = lwu(L8_ADDR(0x0015ffd8));
    uint32_t out = pv + 0x158;
    for (uint32_t i = 0; i < 10; ++i) {
        const uint32_t o = table + (lwu(pv + 0x130 + i * 4) << 8);
        if (lh(o + 0xa6) == 0x279) {
            sw(out, o);
            out += 4;
        }
    }
}

// Class 452, the Bolt Thief. pvars: +0x29 0, +0x40 cleared, +0x60 its colour pulse (+0x67 its length), +0x70 its throw
// (a move record), +0xd0 its run (a follower: +0xd0 the point, +0xd4 the direction, +0xe0 the path, its number +0xe4),
// +0x100 its escape (the same: +0x100, +0x104, +0x110, the path number +0x114, negative: none), +0x130 ten moby indices
// (its critters), +0x158 its critter list (50 slots, 0: empty), +0x220 the path whose first point is its lookout,
// +0x224 its turn rate, +0x22c the critters it caught, +0x260 its joint 1's node (scaled 2.5). States (the table 0x1fbfb0,
// 0 .. 8; 9 and above: only the colour pulse): 0 set up on its run's first point (its escape on point 1 if it has one,
// animation 1), state 1; 1 waits until Ratchet is within 24 of its lookout: its critters picked, state 5 (animation 3);
// 2 runs along its run to a point whose w is 0: state 3 (animation 2); 3 plays its animation at speed 3 to the end: its
// critters picked and all of them (state not negative) told to come (their state 3), state 4 (animation 6); 4 pulls its
// critters (state not negative) to its point 0 at 4 a second; each that arrives is caught (its state 4, the slot
// emptied, +0x22c up); none left: state 5 (animation 3); 5 runs along its run: each critter within 6 flies to it (a bolt's
// flight; animation 6), with none the animation 3; the run's last point next: state 6, or with no escape path gone;
// 6 runs along its escape: Ratchet farther than 19, state 7; 7 faces Ratchet: Ratchet within 17, or a Devastator rocket
// after it, back to state 6, its escape reversed when Ratchet is nearer to its next point than to it; 8 thrown: landed (or
// a wall), a burst and gone; below a height of 5, gone. Hit (mask 0x210000, not in state 8): its critters coming (state
// 3) sent back (state 1), thrown away from Ratchet (the move 4), its bolts dropped, state 8. Near the camera (within 29,
// with +0x31 set) its floor band.
// orig: L8:0x002e2df0
void bat_class452_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(208);
    const uint32_t goal = sp, step = sp + 0x10;
    joint_node_scale(m, 1, pv + 0x260, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L8_ADDR(0x001675c0))) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }

    // a hit
    if (sub_0025a420(m, NOT_ADDR(0x210000), 0) != 0 && lbu(m + 0x20) != 8) {
        for (int32_t i = 0; i < kCritters; ++i) {
            const uint32_t c = lwu(pv + 0x158 + static_cast<uint32_t>(i) * 4);
            if (c != 0 && lbu(c + 0x20) == 3) sb(c + 0x20, 1);
        }
        swc1(pv + 0x80, 0.008f);
        sw(pv + 0x94, 1);
        swc1(pv + 0x8c, fpu_mul(dt(), 10.0f));
        swc1(pv + 0x84, 0.0005f);
        swc1(pv + 0x88, fpu_mul(dt(), 8.0f));
        sb(pv + 0xad, 0);
        move_state_start(m, pv + 0x70, 4, 1, 0, heading(lwc1(HERO_POS), lwc1(HERO_POS + 4), lwc1(m + 0x10), lwc1(m + 0x14)));
        sb(pv + 0x67, 0x78);
        moby_drop_bolts(m, 0, -1);
        sb(m + 0x20, 8);
    }

    sb(m + 0xa4, 0xff);
    bool gone = false;
    switch (lbu(m + 0x20)) {  // (the table 0x1fbfb0: states 0 .. 8 each its own case; 9 and above: none)
    case 0: {  // (0x002e2fc0) set up
        const uint32_t run = lvd::path(lw(pv + 0xe4));
        sw(pv + 0xe0, run);
        lq_sq(m + 0x10, run + 0x10);
        sw(pv + 0xd0, 1);
        if (lw(pv + 0x114) >= 0) {
            sw(pv + 0x100, 1);
            sw(pv + 0x110, lvd::path(lw(pv + 0x114)));
        }
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sb(pv + 0x29, 0);
        sd(m + 0x38, ld(lwu(HERO_MOBY) + 0x38));
        vec_clear(V(pv + 0x40));
        sb(m + 0x20, 1);
        anim_to(m, 1, 0);
        break;
    }
    case 1:  // (0x002e3060) waiting for Ratchet
        lq_sq(goal, lvd::path(lw(pv + 0x220)) + 0x10);
        if (vec3_distance(V(HERO_POS), V(goal)) < 24.0f) {
            bolt_thief_gather_l8(m);
            sb(m + 0x20, 5);
            anim_to(m, 3, 6);
        }
        break;
    case 2: {  // (0x002e30c0) running to a stop
        const int32_t next = next_point(pv + 0xd0);
        const uint32_t p = path_pt(lwu(pv + 0xe0), next);
        turn_to(m, pv, p);
        lq_sq(goal, p);
        if (lwc1(p + 0xc) == 0.0f) {
            sb(m + 0x20, 3);
            anim_to(m, 2, 6);
            break;
        }
        if (step_to(m, goal, step)) sw(pv + 0xd0, static_cast<uint32_t>(next));
        break;
    }
    case 3:  // (0x002e3204) stopped: calling its critters
        swc1(m + 0x58, 3.0f);
        if ((lbu(m + 0x70) & 2u) == 0) break;
        bolt_thief_gather_l8(m);
        for (int32_t i = 0; i < kCritters; ++i) {
            const uint32_t c = lwu(pv + 0x158 + static_cast<uint32_t>(i) * 4);
            if (c != 0 && static_cast<int8_t>(lbu(c + 0x20)) >= 0) sb(c + 0x20, 3);
        }
        sb(m + 0x20, 4);
        anim_to(m, 6, 6);
        swc1(m + 0x58, 1.0f);
        break;
    case 4: {  // (0x002e328c) pulling its critters in
        const uint32_t hand = sp + 0x10, pull = sp;
        int32_t pulled = 0;
        for (int32_t i = 0; i < kCritters; ++i) {
            const uint32_t slot = pv + 0x158 + static_cast<uint32_t>(i) * 4;
            const uint32_t c = lwu(slot);
            if (c == 0 || static_cast<int8_t>(lbu(c + 0x20)) < 0) continue;
            moby_chain_point_v(m, 0, V(hand));
            ++pulled;
            vec3_sub(V(pull), V(hand), V(c + 0x10));
            vec3_clamp_length(pull, fpu_mul(dt(), 4.0f));
            vec3_add(V(c + 0x10), V(c + 0x10), V(pull));
            if (vec3_length(V(pull)) < fpu_add(dt(), dt())) {
                sb(c + 0x20, 4);
                sw(slot, 0);
                sw(pv + 0x22c, lwu(pv + 0x22c) + 1);
            }
        }
        if (pulled != 0) break;
        sb(m + 0x20, 5);
        anim_to(m, 3, 6);
        break;
    }
    case 5: {  // (0x002e3384) running along its run, catching its critters
        const uint32_t rec = pv + 0xd0;
        const int32_t next = next_point(rec);
        turn_to(m, pv, path_pt(lwu(rec + 0x10), next));
        lq_sq(goal, path_pt(lwu(rec + 0x10), next));
        bool any = false;
        for (int32_t i = 0; i < kCritters; ++i) {
            const uint32_t slot = pv + 0x158 + static_cast<uint32_t>(i) * 4;
            const uint32_t c = lwu(slot);
            if (c == 0 || static_cast<int8_t>(lbu(c + 0x20)) < 0) continue;
            any = true;
            if (vec2_distance(V(m + 0x10), V(c + 0x10)) < 6.0f) {
                anim_to(m, 6, frame_scale_i(0x14));
                bolt_fly_to(lwu(slot), m, 0.0f, 0.0f);
                sw(slot, 0);
            }
        }
        if (!any) anim_to(m, 3, frame_scale_i(0x14));
        if (lwu(lwu(rec + 0x10)) - 1u == static_cast<uint32_t>(next)) {
            if (lw(pv + 0x114) < 0) {
                moby_retire(m);
                gone = true;
            } else {
                sb(m + 0x20, 6);
            }
            break;
        }
        if (step_to(m, goal, step)) sw(rec, static_cast<uint32_t>(next));
        break;
    }
    case 6: {  // (0x002e3584) running along its escape
        const uint32_t rec = pv + 0x100;
        const int32_t next = next_point(rec);
        turn_to(m, pv, path_pt(lwu(rec + 0x10), next));
        lq_sq(goal, path_pt(lwu(rec + 0x10), next));
        if (step_to(m, goal, step)) sw(rec, static_cast<uint32_t>(next));
        if (19.0f < vec3_distance(V(HERO_POS), V(m + 0x10))) sb(m + 0x20, 7);
        break;
    }
    case 7: {  // (0x002e36bc) out of reach, facing Ratchet
        yaw_turn_damped(m, pv + 0x224, heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(HERO_POS), lwc1(HERO_POS + 4)), 0.01f,
                        0.3f, 0.1f);
        const float dist = vec3_distance(V(HERO_POS), V(m + 0x10));
        if (!(dist < 17.0f) && dev_25f878(m) == 0) break;
        const uint32_t rec = pv + 0x100;
        sb(m + 0x20, 6);
        const int32_t next = next_point(rec);
        if (vec3_distance(V(HERO_POS), V(path_pt(lwu(rec + 0x10), next))) < dist)
            sb(rec + 4, static_cast<uint32_t>(-static_cast<int32_t>(static_cast<int8_t>(lbu(rec + 4)))));
        break;
    }
    case 8:  // (0x002e37bc) thrown
        if ((enemy_physics(m, pv + 0x70) & 3u) != 0) {
            explosion_burst(m, m + 0x10, -1, 1.0f, 13.0f);
            moby_retire(m);
            gone = true;
        } else if (lwc1(m + 0x18) < 5.0f) {  // (out of the level)
            moby_retire(m);
            gone = true;
        }
        break;
    default:  // (9 and above, outside the table)
        break;
    }
    if (!gone) color_pulse_update(m, pv + 0x60);
    ee_stack_free(208);
}

}  // namespace rac1
