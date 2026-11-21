// Batalia (level 8) own moby classes, hand-ported: class 435, the Blarg missile. Two kinds share the class: the ones the
// Blarg gunships fire (classes 444 / 462 / 463 through bat_class435_spawn_l8: state 1 homing on a target moby, or state 5
// flying straight on), and the ones placed on the level, which wait for the hero to come near, then fly a path into a
// part of the fort and blow it apart (its group's wall pieces thrown, its breakable part blown up, rubble thrown out), a
// story byte remembering it. Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR (the class's tuning at
// 0x161a00 .. 0x161a30, the camera's place 0x1675c0), the base ELF's are bare (the hero's place 0x13f3d0, the frame time
// 0x15ed6c and its square 0x15ed70).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
const float kTwoPi = u2f(0x40c90fdbu);
const float kFiveDegrees = u2f(0x3db2b8c2u);
const float kSixthPi = u2f(0x3f060a92u);    // 30 degrees
const float kQuarterPi = u2f(0x3f490fdbu);

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of the level's moby table at index `i`
uint32_t table_moby(uint32_t i) { return lwu(L8_ADDR(0x0015ffd8)) + (i << 8); }

// the story byte of the placed missile `pv` (0x13d3b4 + its index +0x90): set once it has struck
uint32_t struck_flag(uint32_t pv) { return 0x0013d388u + lwu(pv + 0x90) + 0x2c; }

// a quadword of the EE stack cleared
void quad_clear(uint32_t a) {
    sd(a, 0);
    sd(a + 8, 0);
}
}  // namespace

// A class 435 missile (0x1b3) spawned at the EE vector `at` facing the camera (0x1675c0), in state 1, scaled by
// 0x161a40 and coloured 0x161a34 .. 0x161a3c; its pvars +0x20 = the EE vector `vel`, +0x40 = `f40`, +0x44 = `f44`,
// +0x48 = 70, +0x4c = `target`. Returns the moby (0: none spawned).
// orig: L8:0x002de3e0
uint32_t bat_class435_spawn_l8(uint32_t at, uint32_t target, uint32_t vel, float f40, float f44) {
    const uint32_t m = moby_spawn(0x1b3);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    lq_sq(pos, at);
    const float dist = vec2_distance(V(L8_ADDR(0x001675c0)), V(pos));
    swc1(m + 0x44, -atan2_f(dist, fpu_sub(lwc1(L8_ADDR(0x001675c8)), lwc1(m + 0x18))));
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(L8_ADDR(0x001675c0)), lwc1(m + 0x10)),
                           fpu_sub(lwc1(L8_ADDR(0x001675c4)), lwc1(m + 0x14))));
    sb(m + 0x30, 0xff);
    sb(m + 0x20, 1);
    const float size = fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L8_ADDR(0x00161a40)));
    sb(m + 0x31, 1);
    sh(m + 0x32, 0xff);
    swc1(m + 0x2c, size);
    moby_set_color_hi(m, lw(L8_ADDR(0x00161a34)), lw(L8_ADDR(0x00161a38)), lw(L8_ADDR(0x00161a3c)));
    swc1(pv + 0x40, f40);
    swc1(pv + 0x48, 70.0f);
    swc1(pv + 0x44, f44);
    sw(pv + 0x4c, target);
    lq_sq(pv + 0x20, vel);
    moby_update_transform(m);
    return m;
}

// The rubble of the missile `m`'s strike: an explosion at its place (its velocity, pvars +0x20, handed on), smaller when
// the struck part was blown up (`small`), then `count` pieces (classes 0x30e / 0x30f, or 0x310 / 0x311 when not `small`,
// at random) from random points of the level's box `box`, thrown out from the missile across at 12 .. 15 a second and up
// at 6 .. 18, for 60 .. 90 frames. (No test of the spawn, as the original: it relies on free mobys.)
// orig: L8:0x002de528
void missile_rubble_l8(uint32_t m, int32_t box, int32_t count, bool small) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t pos = m + 0x10, at = sp + 0x20, offset = sp + 0x30, vel = sp + 0x40;
    const uint32_t mvel = lwu(m + 0x78) + 0x20;
    if (small)
        explosion_effect(m, mvel, pos, 0x1e, 0x14, 0x14, -1, 1, 1, -1, 0, 0.0f, 0.0f, 6.0f, 4.0f, 9.0f, 1.0f, 15.0f);
    else
        explosion_effect(m, mvel, pos, 0x2d, 0x1e, 0x1e, -1, 1, 1, -1, 0, 0.0f, 0.0f, 10.0f, 6.0f, 9.0f, 1.0f, 15.0f);
    for (int32_t i = 0; i < count; ++i) {
        uint32_t cls;
        if (small)
            cls = (rand_mod(0xff) & 1) != 0 ? 0x30e : 0x30f;
        else
            cls = (rand_mod(0xff) & 1) != 0 ? 0x310 : 0x311;
        const uint32_t piece = moby_spawn(cls);
        sh(piece + 0x32, 0x40);
        sb(piece + 0x31, 1);
        sb(piece + 0x30, 0x40);
        sd(piece + 0x38, ld(m + 0x38));
        swc1(piece + 0x40, rand_angle());
        swc1(piece + 0x44, rand_angle());
        swc1(piece + 0x48, rand_angle());
        for (uint32_t lane = 0; lane < 12; lane += 4) swc1(at + lane, rand_range_f(-1.0f, 1.0f));
        swc1(at + 0xc, 1.0f);
        mat4_transform(V(at), V(at), M(lvd::box(lvd::BOXES, box)));
        lq_sq(piece + 0x10, at);
        vec_clear(V(offset));
        vec3_sub(V(vel), V(at), V(pos));
        vec2_set_length(V(vel), V(vel), rand_range_f(fpu_mul(dt(), 12.0f), fpu_mul(dt(), 15.0f)));
        swc1(vel + 8, rand_range_f(fpu_mul(dt(), 6.0f), fpu_mul(dt(), 18.0f)));
        class344_throw(piece, m, vel, offset, float_to_int(rand_range_f(60.0f, 90.0f)));
    }
    ee_stack_free(0x110);
}

// Class 435, the Blarg missile. pvars: +0x20 its velocity; fired: +0x40 / +0x44 the elevation and heading (seen from the
// target) it homes in from, at +0x48 (70) from the target moby +0x4c (whose pvars +0x98 cap the missile's speed and
// +0x58 lose 60 a hit), in state 5 the pitch (negated) and yaw it turns to; placed: +0x50 its point on its path, +0x54 (a
// signed byte) its step along it, +0x60 the path, +0x64 the path's index, +0x80 its yaw rate, +0x88 the level box its
// rubble comes from, +0x8c how near the hero must come to the path's end, +0x90 its story index (the byte 0x13d3b4 + it),
// +0x94 its start timer. Its +0x21 is the group of the part it strikes. States: 0 placed: struck before (its story byte),
// its group's mobys retired; else at its path's start, hidden, waiting (3; a 10-frame timer for the story index 0);
// 1 homing: steered (0x161a04 a second squared) to its point near the target, rolling, trailing sparks; 2 once its
// heading and elevation from the target are within 5 degrees of +0x44 / +0x40: steered at the target 5 above it (4 times
// as hard), growing to 3 times its class's size over 180 frames, within 0x161a0c the target's +0x58 down by 60, the sound
// 0 and the end (6); 3 waits (no movie, 0x15f5c4 0) for the hero within +0x8c of its path's end and its timer, then shown,
// facing its path's second point, the sound 1 (4); 4 flies its path (0x161a08 a second, smoking; no movie): at the last
// point its group struck (wall pieces 0x223 .. 0x228 thrown, those whose +0xc is over 1500 retired, the part 0x1a8 blown
// up), the story byte set, 30 pieces of rubble (smaller when the part was blown up), the sound 2 (6), or nothing with no
// group; 5 flies straight at 2 x 0x161a00 a second, turning (30 degrees a second) to +0x44 / -+0x40, until out of the box
// 5 .. 500 or within 16 of the hero (6); 6 the end: one spawned (above 0x15ffdc) blows up; retired. A hit (mask 0x210000)
// in any state but 4: the sound 0, a burst and retired.
// orig: L8:0x002dd3c0
void bat_class435_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t pos = m + 0x10, vel = pv + 0x20;
    if (sub_0025a420(m, NOT_ADDR(0x210000), 0) != 0 && lbu(m + 0x20) != 4) {
        moby_sound_start(0, 0, m);
        explosion_burst(m, pos, -1, 0.5f, 13.0f);
        moby_retire(m);
        ee_stack_free(0x120);
        return;
    }

    // its roll, a turn a second
    const auto roll = [&]() { swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(dt(), kTwoPi))); };
    // the goal sp+0x20: the target's place, 5 higher
    const uint32_t goal = sp + 0x20;
    const auto goal_at_target = [&]() {
        lq_sq(goal, lwu(pv + 0x4c) + 0x10);
        swc1(goal + 8, fpu_add(lwc1(goal + 8), 5.0f));
    };
    // steered to the goal: the wanted velocity (capped by the target's pvars `tp` +0x98 a second), the velocity turned
    // towards it by `turn` at most, the move, and its pitch and yaw from the velocity
    const auto steer = [&](uint32_t tp, float turn) {
        const uint32_t want = sp + 0x30, change = sp + 0x40;
        vec3_sub(V(want), V(goal), V(pos));
        vec3_clamp_length(want, fpu_mul(lwc1(tp + 0x98), dt()));
        vec3_sub(V(change), V(want), V(vel));
        vec3_clamp_length(change, turn);
        vec3_add(V(vel), V(vel), V(change));
        vec3_add(V(pos), V(pos), V(vel));
        swc1(m + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(vel + 8)));
        swc1(m + 0x48, atan2_f(lwc1(vel), lwc1(vel + 4)));
    };
    // the sparks it trails: across its body (its matrix's rows +0xd0 / +0xe0) up to 0x161a14 a second at a random angle
    const auto trail = [&]() {
        const float r = fpu_mul(rand_range_f(0.0f, lwc1(L8_ADDR(0x00161a14))), dt());
        const float a = rand_angle();
        const uint32_t out = sp + 0x50, side = sp + 0x60;
        vec3_set_length(V(out), V(m + 0xe0), fpu_mul(r, cos_vu(a)));
        vec3_set_length(V(side), V(m + 0xd0), fpu_mul(r, sin_vu(a)));
        vec3_add(V(out), V(out), V(side));
        // (the original copies sp+0x60 onto itself here)
        swc1(sp + 0x5c, lwc1(L8_ADDR(0x00161a24)));
        swc1(sp + 0x6c, lwc1(L8_ADDR(0x00161a28)));
        const int32_t life = frame_scale_i(lw(L8_ADDR(0x00161a20)));
        const int32_t c = float_to_int(lwc1(L8_ADDR(0x00161a24)));
        const int32_t d = float_to_int(lwc1(L8_ADDR(0x00161a28)));
        spawn_particle_4(pos, out, lwu(L8_ADDR(0x00161a2c)), lwu(L8_ADDR(0x00161a30)), static_cast<uint32_t>(life),
                         static_cast<uint32_t>(c), static_cast<uint32_t>(d), 1);
    };

    switch (lbu(m + 0x20)) {  // (the table 0x1fbf20: the states 0 .. 6 in order; 7 and up do nothing)
    case 0: {  // (0x002dd480)
        if (lbu(struck_flag(pv)) != 0) {
            // struck on an earlier visit: its group's mobys retired (no test of the list, as the original)
            for (uint32_t e = lvd::group(lbu(m + 0x21));; e += 2) {
                moby_retire(table_moby(lhu(e) & 0x7fffu));
                if (lh(e) < -1) break;  // (the list's last entry: bit 15 set, and not 0xffff)
            }
            break;
        }
        if (lw(pv + 0x90) == 0) sw(pv + 0x94, static_cast<uint32_t>(frame_scale_i(0xa)));
        const uint32_t path = lvd::path(lw(pv + 0x64));
        sw(pv + 0x60, path);
        lq_sq(pos, path + 0x10);
        sw(pv + 0x50, 1);
        sb(m + 0x20, 3);
        sh(m + 0x32, 0);
        sw(m + 0x94, 0);
        break;
    }
    case 1: {  // (0x002dd540)
        const uint32_t tp = lwu(lwu(pv + 0x4c) + 0x78);
        roll();
        goal_at_target();
        // its heading and elevation seen from the target
        const float heading = atan2_f(fpu_sub(lwc1(pos), lwc1(goal)), fpu_sub(lwc1(pos + 4), lwc1(goal + 4)));
        const float elevation = atan2_f(vec2_distance(V(goal), V(pos)), fpu_sub(lwc1(pos + 8), lwc1(goal + 8)));
        sub_00262cf0(goal, goal, lwc1(pv + 0x48), lwc1(pv + 0x44), lwc1(pv + 0x40));
        steer(tp, fpu_mul(lwc1(L8_ADDR(0x00161a04)), dt_sq()));
        if (angle_dist(heading, lwc1(pv + 0x44)) < kFiveDegrees && angle_dist(elevation, lwc1(pv + 0x40)) < kFiveDegrees)
            sb(m + 0x20, 2);
        trail();
        break;
    }
    case 2: {  // (0x002dd79c)
        const uint32_t tp = lwu(lwu(pv + 0x4c) + 0x78);
        roll();
        const float size = fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 3.0f);
        const float frames = static_cast<float>(frame_scale_i(0xb4));
        approach_f(m + 0x2c, size, fpu_div(lwc1(lwu(m + 0x24) + 0x24), frames));
        goal_at_target();
        steer(tp, fpu_mul(fpu_mul(lwc1(L8_ADDR(0x00161a04)), 4.0f), dt_sq()));
        if (vec3_distance(V(pos), V(lwu(pv + 0x4c) + 0x10)) < lwc1(L8_ADDR(0x00161a0c))) {
            sw(tp + 0x58, lwu(tp + 0x58) - 0x3cu);
            vec_clear(V(vel));
            moby_sound_start(0, 0, m);
            sb(m + 0x20, 6);
        }
        trail();
        break;
    }
    case 3: {  // (0x002dda08)
        if (lw(L8_ADDR(0x0015f5c4)) != 0) break;
        const uint32_t path = lwu(pv + 0x60);
        if (!(vec3_distance(V(0x0013f3d0), V(path + (lwu(path) << 4))) < lwc1(pv + 0x8c))) break;  // (its last point)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x94))) == 0) break;
        sh(m + 0x32, 0x40);
        sb(m + 0x20, 4);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(pos)), fpu_sub(lwc1(path + 0x24), lwc1(pos + 4))));
        moby_sound_start(1, 0, m);
        break;
    }
    case 4: {  // (0x002ddaac)
        if (lw(L8_ADDR(0x0015f5c4)) != 0) break;
        const uint32_t path = lwu(pv + 0x60);
        const uint32_t n = lwu(path);
        const int32_t dir = static_cast<int8_t>(lbu(pv + 0x54));
        const int32_t next = ee_rem(static_cast<int32_t>(lwu(pv + 0x50) + n + static_cast<uint32_t>(dir)), static_cast<int32_t>(n));
        const uint32_t point = path + 0x10 + (static_cast<uint32_t>(next) << 4);
        yaw_turn_damped(m, pv + 0x80, atan2_f(fpu_sub(lwc1(point), lwc1(pos)), fpu_sub(lwc1(point + 4), lwc1(pos + 4))), 0.1f,
                        1.3f, 1.0f);
        swc1(m + 0x44, atan2_f(vec2_distance(V(point), V(pos)), fpu_sub(lwc1(pos + 8), lwc1(point + 8))));
        lq_sq(goal, point);
        if (lwu(pv + 0x50) != lwu(path) - 1u) {
            // on along the path, the next point taken within 0.5 of it
            const uint32_t step = sp + 0x30;
            vec3_sub(V(step), V(goal), V(pos));
            vec3_set_length(V(step), V(step), fpu_mul(lwc1(L8_ADDR(0x00161a08)), dt()));
            vec3_add(V(pos), V(pos), V(step));
            if (vec3_distance(V(pos), V(goal)) < 0.5f) sw(pv + 0x50, static_cast<uint32_t>(next));
            // its smoke: a puff of fire, then one of grey smoke, drifting at up to 2 a second
            const uint32_t fire_dir = sp + 0x50, fire_vel = sp + 0x40;
            quad_clear(fire_dir);
            for (uint32_t lane = 0; lane < 12; lane += 4) swc1(fire_dir + lane, rand_range_f(-1.0f, 1.0f));
            lq_sq(fire_vel, fire_dir);
            vec3_clamp_length(fire_vel, fpu_add(dt(), dt()));
            const int32_t fire_life = frame_scale_i(rand_range(0x1e, 0x2d));
            const int32_t fire_size = rand_range(0x28, 0x46);
            spawn_particle_4(pos, fire_vel, 0x6f00afffu, 0xff, static_cast<uint32_t>(fire_life), 0x28,
                             static_cast<uint32_t>(fire_size), 1);
            const uint32_t smoke_dir = sp + 0x60, smoke_vel = sp + 0x50;
            quad_clear(smoke_dir);
            for (uint32_t lane = 0; lane < 12; lane += 4) swc1(smoke_dir + lane, rand_range_f(-1.0f, 1.0f));
            lq_sq(smoke_vel, smoke_dir);
            vec3_clamp_length(smoke_vel, fpu_add(dt(), dt()));
            const int32_t smoke_life = frame_scale_i(rand_range(0x3c, 0x78));
            const int32_t smoke_size = rand_range(0x64, 0x96);
            spawn_particle_4(pos, smoke_vel, 0x1fffffffu, 0x4f4f4fu, static_cast<uint32_t>(smoke_life), 0x28,
                             static_cast<uint32_t>(smoke_size), 0);
            break;
        }
        // the last point: the strike on its group
        const uint32_t list = lvd::group(lbu(m + 0x21));
        if (list == 0) break;
        bool part_blown = false;
        sb(struck_flag(pv), 1);
        const uint32_t throw_vel = sp + 0x40, offset = sp + 0x50, place = sp + 0x60;
        for (uint32_t e = list;; e += 2) {
            const uint32_t piece = table_moby(lhu(e) & 0x7fffu);
            if (lbu(piece + 0x20) < 0x7f && piece != m) {
                const uint32_t cls = lhu(piece + 0xa6);
                if (cls - 0x223u < 6u) {
                    // a wall piece: its place on the wall (the table 0x1fbf40: one case a class, 0x223 .. 0x228)
                    vec_clear(V(throw_vel));
                    quad_clear(place);
                    switch (cls - 0x223u) {
                    case 0:  // (0x002dde20)
                        swc1(place, -3.331f);
                        swc1(place + 4, 3.331f);
                        swc1(place + 8, 0.275f);
                        break;
                    case 1:  // (0x002dde64)
                        swc1(place, -6.0f);
                        swc1(place + 4, 6.0f);
                        swc1(place + 8, -1.772f);
                        break;
                    case 2:  // (0x002ddea0)
                        swc1(place, -2.0f);
                        swc1(place + 4, -1.152f);
                        swc1(place + 8, -1.772f);
                        break;
                    case 3:  // (0x002ddee0)
                        swc1(place + 8, 0.975f);
                        break;
                    case 4:  // (0x002ddf04)
                        swc1(place + 4, -1.665f);
                        swc1(place + 8, 0.642f);
                        break;
                    default:  // (0x002ddf38, 5) at its origin
                        break;
                    }
                    lq_sq(offset, place);
                    if (1500.0f < lwc1(piece + 0xc)) {
                        moby_retire(piece);
                    } else {
                        // thrown away from the missile at 12 a second, up at 8 .. 12; for 10 .. 20 frames when thrown
                        // towards the camera (within 45 degrees), else 60 .. 90
                        vec3_sub(V(throw_vel), V(piece + 0x10), V(pos));
                        sw(throw_vel + 8, 0);
                        vec3_set_length(V(throw_vel), V(throw_vel), fpu_mul(dt(), 12.0f));
                        swc1(throw_vel + 8, rand_range_f(fpu_mul(dt(), 8.0f), fpu_mul(dt(), 12.0f)));
                        sw(piece + 0x94, 0);
                        const float out = atan2_f(lwc1(throw_vel), lwc1(throw_vel + 4));
                        const float to_camera = atan2_f(fpu_sub(lwc1(L8_ADDR(0x001675c0)), lwc1(pos)),
                                                        fpu_sub(lwc1(L8_ADDR(0x001675c4)), lwc1(pos + 4)));
                        const float life =
                            angle_dist(out, to_camera) < kQuarterPi ? rand_range_f(10.0f, 20.0f) : rand_range_f(60.0f, 90.0f);
                        class344_throw(piece, m, throw_vel, offset, float_to_int(life));
                    }
                } else if (cls == 0x1a8u) {
                    // the breakable part: blown up
                    part_blown = true;
                    vec_clear(V(throw_vel));
                    explosion_effect(piece, throw_vel, piece + 0x10, 0x32, 0x3c, 0x3c, -1, 1, 1, -1, 0, 0.0f, 0.0f, 12.0f, 8.0f,
                                     9.0f, 1.0f, 15.0f);
                    moby_retire(piece);
                }
            }
            if (lh(e) < -1) break;  // (as above)
        }
        missile_rubble_l8(m, lw(pv + 0x88), 0x1e, part_blown);
        moby_sound_start(2, 0, m);
        sb(m + 0x20, 6);
        break;
    }
    case 5: {  // (0x002de19c)
        // straight on: its yaw and pitch turned to +0x44 / -+0x40, rolling, at twice 0x161a00 a second
        turn_towards(m + 0x48, 0, lwc1(pv + 0x44), fpu_mul(dt(), kSixthPi));
        turn_towards(m + 0x44, 0, -lwc1(pv + 0x40), fpu_mul(dt(), kSixthPi));
        roll();
        const float speed = fpu_mul(fpu_add(lwc1(L8_ADDR(0x00161a00)), lwc1(L8_ADDR(0x00161a00))), dt());
        vec_from_spherical_v(V(vel), speed, lwc1(m + 0x48), -lwc1(m + 0x44));
        vec3_add(V(pos), V(pos), V(vel));
        // (the probe: the level's stub, 0 always)
        if (zero_stub() != 0 || lwc1(pos) < 5.0f || 500.0f < lwc1(pos) || lwc1(pos + 4) < 5.0f || 500.0f < lwc1(pos + 4) ||
            lwc1(pos + 8) < 5.0f || 500.0f < lwc1(pos + 8) || vec3_distance(V(0x0013f3d0), V(pos)) < 16.0f)
            sb(m + 0x20, 6);
        break;
    }
    case 6:  // (0x002de320)
        if (lwu(L8_ADDR(0x0015ffdc)) < m)  // (a spawned one)
            explosion_effect(m, vel, pos, 0xf, 0xa, 0xa, -1, 1, 1, -1, 0, 0.0f, 0.0f, 3.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(m);
        break;
    default:
        break;
    }
    ee_stack_free(0x120);
}

}  // namespace rac1
