// Class 1068 of Blarg (L6) and Orxon (L10), a walking robot that throws an electric beam (its draw and sparks are in
// shared_beam.cpp): one port of the two levels' copies, which are the same code relinked (the normalised texts compared,
// the jump tables' entries the same offsets). Its addresses are the engine's (OV: the camera 0x166dc0, the path table
// 0x1b04b0, the collision results 0x173e80, the zero vector 0x15f580) or the base ELF's (bare: the hero 0x13f3d0 and his
// moby 0x1413d0, the hero's mode 0x1413f4, the frame time 0x15ed6c and its square 0x15ed70, the planet 0x15ed84, the hint
// flags 0x13d408..); the level's own data is one block of the class's tunings (per level below) and the beam's draw
// callback. The jump tables (the states, the hit reactions) are switches on the index. Header shared_class1068.h.
//
// The pvars: the health +0x20 (its start, with +0x24 / +0x28 / +0x29 / +0x30 / +0x58 / +0x5a the hit record's words), the
// timer flag +0x38, the knock-back record +0x60 (+0x78 / +0x7c its speeds, +0xb0 / +0xb4), the walker record +0xc0, the
// colour pulse +0x110, the target's record +0x120 (+0x128 its height), the target +0x160 and its kind +0x164 (2: none
// in reach), the home +0x170, the path +0x180, the timer +0x188, the turn's speed +0x18c, the range +0x198, the beam's timer
// +0x19c, the walls +0x1a4, the path back +0x1a8, the guard box +0x1ac, the arm's last point +0x200, the beam +0x210..
// (its origin +0x210, the ground's normal +0x220, the 16 points +0x230, each w its strength; its life +0x21c), the joint's
// node +0x420.
#include <game/ovl/shared_class1068.h>

#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;   // the hero's position
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the hero's moby
constexpr int32_t kBeamPoints = 16;

float frame_time() { return lwc1(0x0015ed6c); }

// a timer word ticked (timer_tick_i32 on EE memory)
int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }

// The level's tunings of the class (its own data, one block): +0 the size, +4 the sight range, +8 / +0xc the knock-back's
// speeds, +0x10 / +0x14 the death's, +0x18 the beam's frames, +0x1c the reach of its arm, +0x20 the walk's speed, +0x24 the
// arm's animation speed, +0x28 the beam's spreading speed, +0x2c its spread (degrees), +0x30 its life (frames). (The beam's
// draw words follow, +0x34: shared_beam.cpp.)
uint32_t tunings(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x00161cb8) + o;
    return L6_ADDR(0x00161fa8) + o;
}
// the beam's draw callback (the level's copy of beam_draw)
uint32_t beam_draw_callback() { return on_level(ORXON) ? L10_ADDR(0x002dc698) : L6_ADDR(0x002ff680); }

// the yaw from the moby to the point (x, y)
float yaw_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// the state set, and the animation `set` started over `frames` (scaled) unless it is the one playing
void go(uint32_t m, uint32_t state, uint32_t set, int32_t frames) {
    sb(m + 0x20, state);
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the yaw turned toward the target at its full rate (a turn a second squared, half a turn a second at most)
void face(uint32_t m, uint32_t pv, uint32_t target) {
    const float accel = fpu_mul(lwc1(0x0015ed70), u2f(0x40c90fdbu));
    yaw_seek(m + 0x48, pv + 0x18c, yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14)), accel, accel,
             fpu_mul(frame_time(), u2f(0x40490fdbu)));
}

// a step two long along the moby's yaw (the yaw's cosine and sine each added to itself; z 0)
void set_step_twice(uint32_t m, uint32_t step) {
    const float c = cos_vu(lwc1(m + 0x48));
    swc1(step, fpu_add(c, c));
    const float s = sin_vu(lwc1(m + 0x48));
    swc1(step + 4, fpu_add(s, s));
    sw(step + 8, 0);
}
// a step of the walker record forward along its yaw, two long
void walk_twice(uint32_t m, uint32_t pv, uint32_t step) {
    set_step_twice(m, step);
    walker_record_step(1.0f, m, pv + 0xc0, step, step + 0x10);
}

// The arm's swing (the attack's frames 9 .. 15): from its last point (+0x200) to the chain point now, against the moby's
// position `up` above, swept in 5 steps with the hit record of a strike along its yaw (kind 1, the class's); then the
// chain point kept.
void arm_swing(uint32_t m, uint32_t pv, float up, bool hero_slows) {
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t point = sp, from = sp + 0x10, dir = sp + 0x20, rec = sp + 0x30;
    moby_chain_point_v(m, 0, V(point));
    lq_sq(from, m + 0x10);
    swc1(from + 8, fpu_add(lwc1(from + 8), up));
    const float t = moby_anim_time(m);
    if (hero_slows && lbu(0x001413f4) == 1 && t < 9.0f) swc1(m + 0x58, lwc1(tunings(0x24)));
    if (9.0f <= t && t <= 15.0f) {
        swc1(dir, cos_vu(lwc1(m + 0x48)));
        swc1(dir + 4, sin_vu(lwc1(m + 0x48)));
        swc1(dir + 0xc, u2f(0x45afdf66u));
        swc1(dir + 8, 1.0f);
        sub_00259888(1.0f, rec, static_cast<int32_t>(m), 1, dir);
        sb(rec + 0x19, 1);
        sh(rec + 0x1a, lhu(m + 0xa6));
        sb(rec + 0x18, 0);
        uint32_t record[9];
        for (uint32_t i = 0; i < 9; ++i) record[i] = lwu(rec + 4 * i);
        swept_collide(V(point), V(from), V(pv + 0x200), V(from), m, record, 5);
    }
    lq_sq(pv + 0x200, point);
    ee_stack_free(0x60);
}
}  // namespace

// The beam thrown: its origin at the moby's chain point 0 (z the moby's), the ground's normal under it, its 16 points on an
// arc 0.2 out from the origin and 0.15 up, spread over the level's angle about the moby's yaw, the two ends' strength 0 and
// the others' 1; its life the level's frames (scaled, as a float).
// orig: L6:0x002ff100 L10:0x002dc118
void class1068_beam_start(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x10);
    const uint32_t out = sp;
    floor_probe_v(0.5f, V(m + 0x10), 0);
    vec3_set_length(V(pv + 0x220), V(OV(0x00173e80)), 1.0f);
    moby_chain_point_v(m, 0, V(pv + 0x210));
    swc1(pv + 0x218, lwc1(m + 0x18));
    for (int32_t i = 0; i < kBeamPoints; ++i) {
        const float spread = fpu_mul(lwc1(tunings(0x2c)), u2f(0x3c8efa35u));  // (in radians)
        const float a = fpu_mul(fpu_sub(fpu_mul(int_to_float(i), u2f(0x3d800000u)), 0.5f), spread);  // (i / 16 - 0.5)
        swc1(out, fpu_mul(cos_vu(angle_add(a, lwc1(m + 0x48))), u2f(0x3e4ccccdu)));
        swc1(out + 4, fpu_mul(sin_vu(angle_add(a, lwc1(m + 0x48))), u2f(0x3e4ccccdu)));
        swc1(out + 8, u2f(0x3e19999au));
        const uint32_t p = pv + 0x230 + static_cast<uint32_t>(i) * 0x10;
        vec3_add(V(p), V(pv + 0x210), V(out));
        swc1(p + 0xc, i == 0 || i == kBeamPoints - 1 ? 0.0f : 1.0f);
    }
    swc1(pv + 0x21c, static_cast<float>(frame_scale_i(lw(tunings(0x30)))));
    ee_stack_free(0x10);
}

// The beam's frame while its life (+0x21c, a frame count kept as a float) lasts: at 20 frames left its full-strength points
// drop to 0.98; each point spreads out from the origin (the level's speed, half as fast while the hero is in mode 1), sinks
// while it is above the ground's plane by 0.4 and rises while below 0.3, and a full-strength point stopped by a wall drops
// to 0.98, a weak one fading out (two frames' time a frame); between two points of 0.6 or more a strike (kind 0x10000, or
// 0x10001 between two full ones) is swept along the segment. Then drawn (beam_draw) and sparking (beam_sparks).
// orig: L6:0x002ff2c8 L10:0x002dc2e0
void class1068_beam_run(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t next = sp, rel = sp + 0x10, rec = sp + 0x20, left = sp + 0x50;
    sw(left, static_cast<uint32_t>(float_to_int(lwc1(pv + 0x21c))));
    tick32(left);
    swc1(pv + 0x21c, static_cast<float>(lw(left)));
    if (lw(left) == 0) {
        ee_stack_free(0x60);
        return;
    }
    const float weak = u2f(0x3f7ae148u);  // (0.98)
    if (lw(left) == 20)
        for (int32_t i = 0; i < kBeamPoints; ++i) {
            const uint32_t w = pv + 0x23c + static_cast<uint32_t>(i) * 0x10;
            if (1.0f <= lwc1(w)) swc1(w, weak);
        }
    const uint32_t origin = pv + 0x210, normal = pv + 0x220;
    for (int32_t i = 0; i < kBeamPoints; ++i) {
        const uint32_t p = pv + 0x230 + static_cast<uint32_t>(i) * 0x10;
        const float speed = lbu(0x001413f4) == 1 ? fpu_mul(fpu_mul(lwc1(tunings(0x28)), frame_time()), 0.5f)
                                                  : fpu_mul(lwc1(tunings(0x28)), frame_time());
        vec3_sub(V(next), V(p), V(origin));
        sw(next + 8, 0);
        vec3_set_length(V(next), V(next), speed);
        vec3_add(V(next), V(p), V(next));
        vec3_sub(V(rel), V(next), V(m + 0x10));
        const float height = vec3_dot(V(rel), V(normal));
        if (u2f(0x3ecccccdu) < height) swc1(next + 8, fpu_sub(lwc1(next + 8), fpu_add(frame_time(), frame_time())));
        else if (height < u2f(0x3e99999au)) swc1(next + 8, fpu_add(lwc1(next + 8), fpu_add(frame_time(), frame_time())));
        bool stopped = false;
        if (1.0f <= lwc1(p + 0xc) && segment_collide_v(V(p), V(next), 2, 0, nullptr) != 0) {
            swc1(next + 0xc, weak);
            stopped = true;
        }
        if (!stopped && lwc1(next + 0xc) < 1.0f) approach_f(next + 0xc, 0.0f, fpu_add(frame_time(), frame_time()));
        lq_sq(p, next);
    }
    const float strong = u2f(0x3f19999au);  // (0.6)
    for (int32_t i = 0; i < kBeamPoints - 1; ++i) {
        const uint32_t p = pv + 0x230 + static_cast<uint32_t>(i) * 0x10, q = p + 0x10;
        if (!(strong <= lwc1(p + 0xc)) || !(strong <= lwc1(q + 0xc))) continue;
        swc1(next, cos_vu(lwc1(m + 0x48)));
        swc1(next + 4, sin_vu(lwc1(m + 0x48)));
        swc1(next + 8, 1.0f);
        swc1(next + 0xc, u2f(0x45afdf66u));
        const uint32_t kind = 1.0f <= lwc1(p + 0xc) && 1.0f <= lwc1(q + 0xc) ? 0x10001u : 0x10000u;
        sub_00259888(1.0f, rec, static_cast<int32_t>(m), static_cast<int32_t>(kind), next);
        sh(rec + 0x1a, lhu(m + 0xa6));
        sb(rec + 0x18, 0);
        sb(rec + 0x19, 1);
        uint32_t record[9];
        for (uint32_t k = 0; k < 9; ++k) record[k] = lwu(rec + 4 * k);
        segment_collide_v(V(p), V(q), 0, m, record);
    }
    sub_001f93e0(beam_draw_callback(), m);
    beam_sparks(m);
    ee_stack_free(0x60);
}

// The frame's common part: the size, the beam (class1068_beam_run), the timer (+0x188, dealt 180 .. 240 frames when +0x38
// is set), a hit (mask 0x330000) taken off the health, then the hit's reaction (the table's index: 1 / 2 the death, 3 .. 10
// knocked back, the others nothing but the colour pulse), the range (6 more while the timer runs), the target picked and
// whether it is out of reach (+0x164 = 2), the hero the target when there is none. On Orxon a hit from a class-794 moby
// shows a hint once.
// orig: L6:0x002feb40 L10:0x002dbb58
void class1068_think(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t pos = sp, vel = sp + 0x10, hit_state = sp + 0x20, hit_dmg = sp + 0x24;
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(tunings(0))));
    class1068_beam_run(m);
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x188, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    tick32(pv + 0x188);
    sw(hit_dmg, 0);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t reaction = 0;
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    bool pulse_start = false;
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0x10) {
        if (lw(0x0015ed84) == 10 && hit != 0 && lwu(hit + 0x20) != 0 && lh(lwu(hit + 0x20) + 0xa6) == 0x31a &&
            lbu(0x0013d419) == 0) {
            sb(0x0013d419, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, health);
        if (health <= 0.0f) reaction = 1;
        sw(pv + 0x80, 0x1f4);
        swc1(pv + 0x88, 0.5f);
        pulse_start = true;
        // the hit reactions' table (L6 0x201830): 1, 2 (0x2feda8) the death; 3 .. 10 (0x2fed18) knocked back; 0, 11 and
        // past the table (0x2fefd8) nothing more
        if (reaction >= 3 && reaction <= 10) {
            sb(m + 0x20, 0xf);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(60)));
            swc1(pv + 0x78, fpu_mul(lwc1(tunings(8)), frame_time()));
            swc1(pv + 0xb0, 8.0f);
            swc1(pv + 0xb4, 15.0f);
            swc1(pv + 0x7c, fpu_mul(lwc1(tunings(0xc)), frame_time()));
            move_state_start(m, pv + 0x60, 7, 1, 0, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
            sb(pv + 0x117, 0xfa);
        } else if (reaction == 1 || reaction == 2) {
            sb(m + 0x20, 0x10);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            swc1(pv + 0xb0, 9.0f);
            swc1(pv + 0xb4, 18.0f);
            swc1(pv + 0x7c, fpu_mul(lwc1(tunings(0x14)), frame_time()));
            swc1(pv + 0x78, fpu_mul(lwc1(tunings(0x10)), frame_time()));
            move_state_start(m, pv + 0x60, 9, 1, 0, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
            // ten shards, 2.5 up, thrown out and away from the hitter, at 0.4 of their size
            const float jitter = u2f(0x3e4ccccdu);  // (0.2)
            for (int32_t k = 0; k < 10; ++k) {
                lq_sq(pos, m + 0x10);
                swc1(pos + 8, fpu_add(lwc1(pos + 8), 2.5f));
                swc1(pos, fpu_add(lwc1(pos), rand_range_signed_f(0.0f, jitter)));
                swc1(pos + 4, fpu_add(lwc1(pos + 4), rand_range_signed_f(0.0f, jitter)));
                swc1(pos + 8, fpu_add(lwc1(pos + 8), rand_range_signed_f(0.0f, jitter)));
                const float a = rand_angle();
                const float speed = fpu_mul(rand_range_f(3.0f, u2f(0x40b66666u)), frame_time());  // (3 .. 5.7)
                swc1(vel, fpu_mul(cos_vu(a), speed));
                swc1(vel + 4, fpu_mul(sin_vu(a), speed));
                swc1(vel + 8, 0.0f);
                const float away = atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14));
                swc1(vel, fpu_add(lwc1(vel), fpu_mul(cos_vu(away), fpu_mul(frame_time(), 7.0f))));
                swc1(vel + 4, fpu_add(lwc1(vel + 4), fpu_mul(sin_vu(away), fpu_mul(frame_time(), 7.0f))));
                swc1(vel + 8, fpu_mul(rand_range_f(3.0f, 8.5f), frame_time()));
                const uint32_t shard = beam_shard_spawn(pos, vel);
                if (shard != 0) swc1(shard + 0x2c, fpu_mul(lwc1(shard + 0x2c), u2f(0x3ecccccdu)));
            }
            sb(pv + 0x117, 0xfa);
            moby_drop_bolts(m, 0, -1);
        }
    }
    if (pulse_start) color_pulse_start(m, pv + 0x110);
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x110);
    swc1(pv + 0x198, lw(pv + 0x188) != 0 ? fpu_add(lwc1(tunings(4)), 6.0f) : lwc1(tunings(4)));
    const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x180) << 2));
    if (target_select(m, pv + 0x120, 0, 0, path + 0x10, lw(path), lwc1(pv + 0x198)) != 2) {
        if (lwc1(pv + 0x198) < vec2_distance(V(pv + 0x170), V(pv + 0x120))) sw(pv + 0x164, 2);
        else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128)))) sw(pv + 0x164, 2);
    }
    if (lw(pv + 0x160) == 0) sw(pv + 0x160, lwu(HERO_MOBY));
    ee_stack_free(0x30);
}

// The update: the frame's common part (class1068_think), the joint scaled, the floor band near the camera, then the state.
// 0 sets up the hit record, the walker and the home; 1 guards (turned to the target: the beam when it is far and in front,
// the arm when it is in reach, the chase while the timer runs; a guard box of its own sends it back along its path, 14,
// when the hero enters it); 2 .. 4 throw the beam; 5 / 6 and 11 / 12 swing the arm, standing or chasing; 13 walks home;
// 14 walks back to its path's end; 15 knocked back; 16 dying (the explosion and three pieces).
// orig: L6:0x002fdbd0 L10:0x002dabe8
void class1068_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class1068_think(m);
    const uint32_t target = lwu(pv + 0x160);
    joint_node_scale(m, 1, pv + 0x420, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 27.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    swc1(m + 0x58, 1.0f);
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t goal = sp, step = sp + 0x10, cross = sp + 0x30;
    const float yaw_rate = u2f(0x40b2b8c2u);  // (320 degrees a second)
    // the states' table (L6 0x2017e0); 7 .. 10 (0x2feb10) do nothing
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x2fdc9c)
        sb(pv + 0x28, 1);
        swc1(pv + 0x20, 2.0f);
        swc1(pv + 0x30, 2.0f);
        sb(pv + 0x29, 1);
        sh(pv + 0x24, 2);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(12.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(u2f(0x4159999au))));  // (13.6)
        walker_record_init_l1(pv + 0xc0);
        swc1(pv + 0xc8, 2.0f);
        swc1(pv + 0xcc, 0.5f);
        sw(pv + 0xc0, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(tunings(0)), u2f(0x4399999au)))));  // (x 307.2)
        swc1(pv + 0xe4, fpu_mul(lwc1(tunings(0x20)), frame_time()));
        swc1(pv + 0xc4, fpu_mul(lwc1(tunings(0)), u2f(0x3e99999au)));
        sw(pv + 0xf8, lwu(pv + 0xf8) | 8u);
        lq_sq(pv + 0x170, m + 0x10);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x2fdd68)
        if (lw(pv + 0x1ac) != -1 && lw(pv + 0x1a8) != -1) {
            if (point_in_box(HERO_POS, lw(pv + 0x1ac)) != 0) {
                sb(m + 0x20, 0xe);
                break;
            }
            if (7.0f < vec2_distance(V(m + 0x10), V(target + 0x10))) break;
        }
        face(m, pv, target);
        if (lw(pv + 0x164) == 2) {
            if (lw(pv + 0x188) == 0) break;
            go(m, 0xb, 1, 20);
        } else if (lwc1(tunings(0x1c)) < vec2_distance(V(m + 0x10), V(target + 0x10))) {
            const float toward = yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14));
            if (!(angle_dist(lwc1(m + 0x48), toward) < u2f(0x3e32b8c2u))) break;  // (10 degrees)
            go(m, 2, 3, 10);
        } else {
            go(m, 5, 6, 10);
        }
        break;
    }
    case 2:  // (0x2fe11c)
        face(m, pv, target);
        if ((lbu(m + 0x70) & 2) == 0) break;
        class1068_beam_start(m);
        go(m, 3, 5, 10);
        break;
    case 3:  // (0x2fe1c4)
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (lw(pv + 0x164) == 2) {
            go(m, 1, 0, 20);
            break;
        }
        go(m, 4, 4, 20);
        sw(pv + 0x19c, static_cast<uint32_t>(frame_scale_i(lw(tunings(0x18)))));
        break;
    case 4: {  // (0x2fe234)
        face(m, pv, target);
        if (tick32(pv + 0x19c) != 0) {
            class1068_beam_start(m);
            go(m, 3, 5, 5);
        } else if (vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(tunings(0x1c))) {
            go(m, 5, 6, 10);
        } else if (lw(pv + 0x188) != 0) {
            go(m, 0xb, 1, 20);
        }
        break;
    }
    case 5:  // (0x2fe354)
        face(m, pv, target);
        arm_swing(m, pv, u2f(0x3e99999au), true);  // (0.3 up)
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 6);
        sb(m + 0xbc, 1);
        if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(10));
        break;
    case 6:  // (0x2fe538)
        if ((lbu(m + 0x70) & 2) == 0) break;
        go(m, lbu(m + 0xbc), 0, 20);
        break;
    case 11: {  // (0x2fe560)
        yaw_turn_damped(m, pv + 0x18c, yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14)), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                        fpu_mul(frame_time(), u2f(0x40c90fdbu)));
        walk_twice(m, pv, step);
        path_push_out_2d(lwu(pv + 0x1a4), m + 0x10, m + 0x10, 0.75f);
        bool at_wall = false;
        if (path_crossing(lw(pv + 0x1a4), m + 0x10, target + 0x10, cross) != 0 &&
            vec3_distance(V(m + 0x10), V(cross)) < u2f(0x3f4ccccdu))  // (0.8)
            at_wall = true;
        if (lw(pv + 0x188) == 0) {
            sb(m + 0x20, 0xd);
            break;
        }
        if (at_wall || vec2_distance(V(m + 0x10), V(target + 0x10)) < lwc1(tunings(0x1c))) go(m, 0xc, 6, 10);
        break;
    }
    case 12:  // (0x2fe6fc)
        arm_swing(m, pv, 1.5f, false);
        if ((lbu(m + 0x70) & 2) == 0) break;
        go(m, 0xb, 1, 20);
        break;
    case 13: {  // (0x2fe840)
        yaw_turn_damped(m, pv + 0x18c, yaw_to(m, lwc1(pv + 0x170), lwc1(pv + 0x174)), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                        fpu_mul(frame_time(), u2f(0x40c90fdbu)));
        set_step_twice(m, step);
        if (angle_dist(lwc1(m + 0x48), yaw_to(m, lwc1(pv + 0x170), lwc1(pv + 0x174))) < u2f(0x3f490fdbu)) {  // (45 degrees)
            walker_record_step(1.0f, m, pv + 0xc0, step, step + 0x10);
            path_push_out_2d(lwu(pv + 0x1a4), m + 0x10, m + 0x10, 0.75f);
        }
        if (vec3_distance(V(pv + 0x170), V(m + 0x10)) < 1.0f) go(m, 1, 0, 20);
        break;
    }
    case 14: {  // (0x2fdf1c)
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(12));
        const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x1a8) << 2));
        lq_sq(goal, path + (lwu(path) << 4));  // (its last point)
        float dist = vec2_distance(V(m + 0x10), V(goal));
        if (dist < 0.25f) {
            const float toward = yaw_to(m, lwc1(target + 0x10), lwc1(target + 0x14));
            yaw_turn_damped(m, pv + 0x18c, toward, u2f(0x3d4ccccdu), u2f(0x3e99999au), fpu_mul(frame_time(), yaw_rate));
            if (!(angle_dist(toward, lwc1(m + 0x48)) < u2f(0x3e860a92u))) break;  // (15 degrees)
            sw(pv + 0x18c, 0);
            sw(pv + 0x1ac, 0xffffffffu);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(18));
            sb(m + 0x20, 1);
            break;
        }
        yaw_turn_damped(m, pv + 0x18c, yaw_to(m, lwc1(goal), lwc1(goal + 4)), u2f(0x3ccccccdu), u2f(0x3e99999au),
                        fpu_mul(frame_time(), yaw_rate));
        if (2.0f < dist) dist = 2.0f;
        swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), dist));
        swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), dist));
        sw(step + 8, 0);
        walker_record_step(1.0f, m, pv + 0xc0, step, step + 0x10);
        break;
    }
    case 15: {  // (0x2fe978)
        const uint32_t r = enemy_physics(m, pv + 0x60);
        if ((r & 0x40u) != 0) go(m, 1, 0, 10);
        else if ((r & 0x120u) != 0) moby_retire(m);
        break;
    }
    case 16: {  // (0x2fe9e4)
        if ((enemy_physics(m, pv + 0x60) & 0x160u) == 0) break;
        lq_sq(goal, m + 0x10);
        swc1(goal + 8, fpu_add(lwc1(goal + 8), 1.0f));
        explosion_effect(m, pv + 0x40, goal, 5, 2, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        const uint32_t z = OV(0x0015f580);
        debris_spawn(m, 0x6a6, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
        debris_spawn(m, 0x6a7, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
        debris_spawn(m, 0x6a8, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
        moby_retire(m);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x40);
}

}  // namespace rac1
