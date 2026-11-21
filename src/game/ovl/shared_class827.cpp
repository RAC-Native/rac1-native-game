// Class 827, a walker of Blarg (L6) and Orxon (L10): one port of the two levels' copies, which are the same code relinked
// (the normalised texts compared, the jump tables' entries the same offsets). Its addresses are the engine's (OV: the moby
// table 0x15ffd8, the path table 0x1b04b0, the camera 0x166dc0, the collision results 0x173e60.., the zero vector
// 0x15f580, the frame counter 0x15f5cc, the mode words 0x15f5c4 / 0x15fdf8, the class-857 list 0x1aba00) or the base
// ELF's (bare: the hero 0x13f3d0 and his moby 0x1413d0, the frame time 0x15ed6c and its square 0x15ed70); the levels' own
// data is one block of the class's tunings (per level below) and the candidates' list of the target pick. The jump tables
// (the states, the hit reactions) are switches on the index. Header shared_class827.h.
//
// The pvars: the health +0x20 (its start +0x24, a half), the glow +0x30, the colour pulse's spawn flag +0x38, the move
// record +0x120.. (+0x130 the gravity, +0x138 / +0x13c the jump's speed and time, +0x144 / +0x15d its kind), the death's
// glow flag +0x14e, the target's position +0x180, the target +0x1c0 and its kind +0x1c4 (1 fixed, 2 none), the turn
// +0x1d4 and its spread +0x1d8, the sight range +0x1dc / now +0x1e0, the speed +0x1e4, the chase timer +0x1e8, the hidden
// flag +0x1ea, the zone +0x1f4, the path +0x1f8 (-1 none), the wander timer +0x1fc, the rider class +0x200, the second
// mode +0x202, a fixed target's index +0x204 (-1 none), the path's point +0x208, the frame slot +0x20a, the rider +0x20c,
// the home +0x210, the next goal +0x230, the lives +0x240, the mode flag +0x242, the jump's reach +0x244, the hero's
// range +0x24c, the pulse's phase +0x248, the joint's node +0x250.
#include <game/ovl/shared_class827.h>

#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;   // the hero's position
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the hero's moby
constexpr uint32_t MATE_CLASS = 0x359;      // class 857
constexpr uint32_t RIDER_CLASS = 0x1f4;     // class 500

float frame_time() { return lwc1(0x0015ed6c); }

// The level's tunings of the class (its own data, one block): its address +0 is the moby's record pointer (pvar +0xd0);
// +0x10 the turn's blend, +0x14 the colour pulse's rate (degrees a second), +0x18 / +0x1c its two colours, +0x20 the
// class held, +0x24 the spawn counter, +0x28.. +0x74 the hit sparks' numbers, +0xf4 / +0xf8 the candidates' count and
// the frame they were listed on.
uint32_t tunings(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x00161ae0) + o;
    return L6_ADDR(0x00161bf0) + o;
}
// The candidates the target pick lists (class 857's mobys awake, a word each).
uint32_t candidates() {
    if (on_level(ORXON)) return L10_ADDR(0x001d9f20);
    return L6_ADDR(0x001dae60);
}

// a pvar timer word ticked (timer_tick_i16 / _i32 on EE memory)
int32_t tick16(uint32_t a) { return timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(a))); }
int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }

// the yaw from the moby to `to` (a moby)
float yaw_to(uint32_t m, uint32_t to) {
    return atan2_f(fpu_sub(lwc1(to + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(to + 0x14), lwc1(m + 0x14)));
}
}  // namespace

// The moby's ground along its up axis +0xe0: the segment from one unit above it to one below (collision flags 2).
// orig: L6:0x002e9aa8 L10:0x002cdce8
int32_t class827_ground(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t below = sp, above = sp + 0x10, axis = sp + 0x20;
    lq_sq(above, m + 0x10);
    vec3_set_length(V(axis), V(m + 0xe0), 1.0f);
    vec3_add(V(above), V(above), V(axis));
    lq_sq(below, m + 0x10);
    vec3_set_length(V(axis), V(m + 0xe0), -1.0f);
    vec3_add(V(below), V(below), V(axis));
    const int32_t hit = segment_collide_v(V(above), V(below), 2, 0, nullptr);
    ee_stack_free(0x80);
    return hit;
}

// The matrix +0xc0 blended (the tunings' +0x10) toward the one whose up is `up` and whose forward is `dir` made
// perpendicular to it.
// orig: L6:0x002e9998 L10:0x002cdbd8
void class827_turn(uint32_t m, uint32_t up, uint32_t dir) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t q = sp, goal_q = sp + 0x10, axis = sp + 0x20, fwd = sp + 0x30, along = sp + 0x40;
    const uint32_t goal = sp + 0x50;  // the goal's rows: forward +0x50, side +0x60, up +0x70
    vec3_set_length(V(axis), V(up), 1.0f);
    vec3_set_length(V(along), V(axis), vec3_dot(V(axis), V(dir)));
    vec3_sub(V(fwd), V(dir), V(along));
    vec3_set_length(V(fwd), V(fwd), 1.0f);
    lq_sq(goal + 0x20, axis);
    lq_sq(goal, fwd);
    vec3_cross_ba(V(goal + 0x10), V(goal), V(goal + 0x20));
    mat_to_quat_at(q, m + 0xc0);
    mat_to_quat_at(goal_q, goal);
    quat_nlerp(V(q), V(q), V(goal_q), lwc1(tunings(0x10)));
    quat_to_mat3(V(q), M(m + 0xc0));
    ee_stack_free(0xf0);
}

// The step `step` (a vector's address) plus gravity (`up` along the moby's up axis, less a frame's worth), the moby moved
// by it; a step longer than `radius` is cut at the first wall (from its centre `up` above), then the sphere of `radius`
// pushed out of the walls (six tries); with `on_path` kept inside its path +0x1ec (by half a unit).
// orig: L6:0x002e9b60 L10:0x002cdda0
void class827_move(uint32_t m, uint32_t step, uint32_t on_path, float up, float radius) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t from = sp, centre = sp + 0x10, lift = sp + 0x20, fall = sp + 0x30;
    const uint32_t pos = m + 0x10;
    vec3_set_length(V(lift), V(m + 0xe0), up);
    vec3_set_length(V(fall), V(lift), -frame_time());
    vec3_add(V(step), V(step), V(fall));
    lq_sq(from, pos);
    vec3_add(V(from), V(from), V(lift));
    vec3_add(V(pos), V(pos), V(step));
    vec3_add(V(centre), V(pos), V(lift));
    if (radius < vec3_length(V(step)) && segment_collide_v(V(from), V(centre), 0, m, nullptr) != 0) {
        lq_sq(centre, OV(0x00173e60));
        vec3_sub(V(pos), V(centre), V(lift));
    }
    for (int32_t i = 0; i < 6 && sphere_collide(centre, radius, 4, m) != 0; ++i) {
        lq_sq(centre, OV(0x00173e70));
        vec3_sub(V(pos), V(centre), V(lift));
    }
    if (on_path != 0) path_push_out_2d(lwu(lwu(m + 0x78) + 0x1ec), pos, pos, 0.5f);
    ee_stack_free(0xe0);
}

// The candidate's score: its distance plus five times the turn toward it, two less for the current target; it replaces the
// best when lower (and within `range`).
// orig: L6:0x002f33e8 L10:0x002d7420
void class827_target_score(uint32_t m, uint32_t current, uint32_t cand, uint32_t best, uint32_t best_score, float range) {
    const float dist = vec2_distance(V(m + 0x10), V(cand + 0x10));
    if (range < dist) return;
    float score = fpu_add(dist, fpu_mul(angle_dist(lwc1(m + 0x48), yaw_to(m, cand)), 5.0f));
    if (cand == current) score = fpu_sub(score, 2.0f);
    if (score < lwc1(best_score)) {
        sw(best, cand);
        swc1(best_score, score);
    }
}

// Once a frame (the frame counter changed) the candidates are listed again: the mobys of the class-857 list (*0x1aba00..,
// to its 0) of that class in a state from 2 but 9. Then the best of them and of the hero's moby becomes the target.
// orig: L6:0x002f34d8 L10:0x002d7510
void class827_target_pick(uint32_t m, uint32_t out, float range) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t best = sp, best_score = sp + 4;
    const uint32_t frame = lwu(OV(0x0015f5cc));
    if (lwu(tunings(0xf8)) != frame) {
        sw(tunings(0xf8), frame);
        sw(tunings(0xf4), 0);
        for (uint32_t e = OV(0x001aba00); lwu(e) != 0; e += 4) {
            const uint32_t o = lwu(e);
            const uint32_t state = lbu(o + 0x20);
            if (lh(o + 0xa6) != static_cast<int32_t>(MATE_CLASS) || state < 2 || state == 9) continue;
            const int32_t n = lw(tunings(0xf4));
            sw(tunings(0xf4), static_cast<uint32_t>(n + 1));
            sw(candidates() + static_cast<uint32_t>(n) * 4, o);
        }
    }
    swc1(best_score, u2f(0x497423f0u));  // 999999
    sw(best, 0);
    for (int32_t i = 0; i < lw(tunings(0xf4)); ++i)
        class827_target_score(m, lwu(out), lwu(candidates() + static_cast<uint32_t>(i) * 4), best, best_score, range);
    class827_target_score(m, lwu(out), lwu(HERO_MOBY), best, best_score, range);
    sw(out, lwu(best));
    ee_stack_free(0x90);
}

// Twenty sparks (particle kind 4) about the moby, flying out (1.8 a second) and up.
// orig: L6:0x002ea498 L10:0x002ce4d0
void class827_glow_burst(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t vel = sp, pos = sp + 0x10;
    for (int32_t i = 0; i < 20; ++i) {
        const float a = rand_angle();
        const float b = rand_angle();
        vec_from_spherical_v(V(vel), fpu_mul(frame_time(), u2f(0x3fe66666u)), a, b);
        swc1(vel + 8, fpu_add(lwc1(vel + 8), fpu_mul(frame_time(), u2f(0x3f666666u))));
        rand_sphere_point(pos, 0.0f, 0.5f);
        vec3_add(V(pos), V(pos), V(m + 0x10));
        swc1(pos + 8, fpu_add(lwc1(pos + 8), 0.5f));
        const int32_t life = frame_scale_i(rand_range(0x14, 0x23));
        const int32_t size = rand_range(0x28, 0x3c);
        spawn_particle_4(pos, vel, 0x7000a0ffu, 0xff, static_cast<uint32_t>(life), 0x1e, static_cast<uint32_t>(size), 1);
    }
    ee_stack_free(0x90);
}

// The rider (pvar +0x20c) breaks: its sound 4, four chunks of class 0x165 and eight of 0x166 / 0x167 from half a unit
// above the moby, each thrown in a random direction (1 to 4 a second), then the rider retired.
// orig: L6:0x002e9f30 L10:0x002cdf68
void class827_rider_debris(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t at = sp, vel = sp + 0x10, pos = sp + 0x20, dir = sp + 0x30;
    const uint32_t pv = lwu(m + 0x78);
    moby_sound_start(4, 0, m);
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), 0.5f));
    auto piece = [pv, at, vel, pos, dir](bool small) {
        const float speed = fpu_mul(rand_range_f(1.0f, 4.0f), frame_time());
        for (uint32_t k = 0; k < 0x10; k += 4) sw(dir + k, 0);
        swc1(dir, rand_range_f(-0.625f, 0.625f));
        swc1(dir + 4, rand_range_f(-0.625f, 0.625f));
        swc1(dir + 8, rand_range_f(0.0f, 1.25f));
        lq_sq(pos, dir);
        vec3_set_length(V(vel), V(pos), speed);
        vec3_add(V(pos), V(pos), V(at));
        if (!small) {
            chunk_spawn(lwu(pv + 0x20c), vel, pos, OV(0x0015f580), 0x165, 0, u2f(0x3e99999au), u2f(0x3f19999au));
        } else {
            const uint32_t cls = static_cast<uint32_t>(rand_mod(2)) + 0x166;
            chunk_spawn(lwu(pv + 0x20c), vel, pos, OV(0x0015f580), cls, 0, 0.5f, u2f(0x3f333333u));
        }
    };
    for (int32_t i = 0; i < 4; ++i) piece(false);
    for (int32_t i = 0; i < 8; ++i) piece(true);
    moby_retire(lwu(pv + 0x20c));
    sw(pv + 0x20c, 0);
    ee_stack_free(0xd0);
}

// The tunings' number (+0x28) of bursts, each from about the moby (+0.25 up, jittered): a velocity of the push `push`
// scaled at random plus a random horizontal and upward speed, its gravity, and the tunings' number (+0x2c) of streaks
// (particle kind 2) with random colours between the tunings' pairs.
// orig: L6:0x002ea198 L10:0x002ce1d0
void class827_sparks(uint32_t m, uint32_t push) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t at = sp + 0x10, vel = sp + 0x20, tail = sp + 0x30;
    for (int32_t i = 0; i < lw(tunings(0x28)); ++i) {
        lq_sq(at, m + 0x10);
        metal_25f8e0(at, lwc1(tunings(0x30)));
        swc1(at + 8, fpu_add(lwc1(at + 8), 0.25f));
        const float a = rand_angle();
        float out = rand_range_f(lwc1(tunings(0x3c)), lwc1(tunings(0x40)));
        float rise = rand_range_f(lwc1(tunings(0x44)), lwc1(tunings(0x48)));
        vec3_scale(V(vel), V(push), fpu_mul(rand_range_f(0.0f, lwc1(tunings(0x74))), frame_time()));
        swc1(vel, fpu_add(lwc1(vel), fpu_mul(cos_vu(a), fpu_mul(out, frame_time()))));
        out = fpu_mul(out, frame_time());
        rise = fpu_mul(rise, frame_time());
        swc1(vel + 8, fpu_add(lwc1(vel + 8), rise));
        swc1(vel + 4, fpu_add(lwc1(vel + 4), fpu_mul(sin_vu(a), out)));
        lq_sq(tail, vel);
        const int32_t frames = frame_scale_i(lw(tunings(0x6c)));
        swc1(tail + 8, fpu_sub(lwc1(tail + 8), fpu_mul(fpu_mul(lwc1(tunings(0x4c)), lwc1(0x0015ed70)), int_to_float(frames))));
        for (int32_t k = 0; k < lw(tunings(0x2c)); ++k) {
            const float w = rand_range_f(0.5f, 1.5f);
            swc1(vel + 0xc, fpu_mul(w, lwc1(tunings(0x50))));
            swc1(tail + 0xc, fpu_mul(w, lwc1(tunings(0x54))));
            metal_25f8e0(at, lwc1(tunings(0x34)));
            metal_25f8e0(tail, fpu_mul(lwc1(tunings(0x38)), frame_time()));
            const uint32_t c0 = rgba_lerp(lwu(tunings(0x58)), lwu(tunings(0x60)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(tunings(0x5c)), lwu(tunings(0x64)), rand_range_f(0.0f, 1.0f));
            const int32_t life = frame_scale_i(lw(tunings(0x68)));
            const int32_t fade = frame_scale_i(lw(tunings(0x6c)));
            const float len = int_to_float(lw(tunings(0x70)));
            const float hi = fpu_mul(len, 2.5f);
            const int32_t h = float_to_int(frame_scale_f(rand_range_f(fpu_mul(len, 0.5f), hi)));
            spawn_particle_02(at, vel, tail, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(fade), static_cast<uint32_t>(h), -1);
        }
    }
    ee_stack_free(0x110);
}

// Before the state: the tilt (+0x30) from its up axis; a hit on the rider (not by class 175) breaks it and the moby
// (sparks, debris, bolts, retired); a hit on the moby takes its damage from the health, and a lethal one (or the reaction
// table's 1 / 2) drops the bolts, sparks, the death glow once, sound 3, breaks the rider, and either retires it (no lives
// left) or brings it back home at full health (state 15). Then the colour pulse, the chase timer +0x1e8 (started at 3 to
// 4 seconds by the flag +0x38) and the sight range (six more while it runs), and the target: a fixed moby (+0x204), or
// (the hero's moby's flag 0x1413f4 set) every eighth frame the pick, kept while inside its zone; else the engine's target
// select in its zone, dropped when beyond the range of home or three units above / below; none: the hero's moby.
// orig: L6:0x002e94f0 L10:0x002cd730
void class827_react(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t hit_state = sp, hit_dmg = sp + 4;
    const uint32_t pv = lwu(m + 0x78);
    swc1(pv + 0x30, fpu_mul(lwc1(m + 0xe8), 0.25f));
    const uint32_t rider = lwu(pv + 0x20c);
    if (rider != 0) {
        const uint32_t rider_hit = sub_0025a420(rider, NOT_ADDR(0x330000u), 0);
        if (rider_hit != 0 && !(lwu(rider_hit + 0x20) != 0 && lh(lwu(rider_hit + 0x20) + 0xa6) == 0xaf)) {
            class827_sparks(m, OV(0x0015f580));
            class827_rider_debris(m);
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
            ee_stack_free(0x70);
            return;
        }
    }
    swc1(hit_dmg, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    int32_t reaction;
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0xe) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, health);
        if (health <= 0.0f) reaction = 1;
        // (the reaction table L6:0x2013f0 (L10:0x1dcec0), 12 entries: 1 and 2 (0x002e9634) the death, the rest (0x002e96f4) none)
        if (reaction == 1 || reaction == 2) {
            moby_drop_bolts(m, 0, -1);
            class827_sparks(m, hit + 0x10);
            if (lbu(pv + 0x14e) == 1) {
                class827_glow_burst(m);
                sb(pv + 0x14e, 0);
            }
            moby_sound_start(3, 0, m);
            if (lwu(pv + 0x20c) != 0) class827_rider_debris(m);
            if (lh(pv + 0x240) == 0) {
                moby_retire(m);
                ee_stack_free(0x70);
                return;
            }
            sh(pv + 0x240, lhu(pv + 0x240) - 1u);
            swc1(pv + 0x20, int_to_float(lh(pv + 0x24)));
            lq_sq(m + 0x10, pv + 0x210);
            sb(m + 0x20, 0xf);
            sw(m + 0x94, 0);
            sh(m + 0x34, (lhu(m + 0x34) & 0xefffu) | 0x41u);
        }
    }

    sb(m + 0xa4, 0xff);
    const float phase = angle_add(lwc1(pv + 0x248), fpu_mul(fpu_mul(lwc1(tunings(0x14)), u2f(0x3c8efa35u)), frame_time()));
    swc1(pv + 0x248, phase);
    sw(m + 0x90, rgba_lerp(lwu(tunings(0x18)), lwu(tunings(0x1c)), fpu_mul(fpu_add(sin_vu(phase), 1.0f), 0.5f)));
    if (lwu(pv + 0x38) != 0) {
        sh(pv + 0x1e8, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    tick16(pv + 0x1e8);
    float range = lwc1(pv + 0x1dc);
    if (lh(pv + 0x1e8) != 0) range = fpu_add(range, 6.0f);
    swc1(pv + 0x1e0, range);

    const int32_t fixed = lw(pv + 0x204);
    const uint32_t paths = OV(0x001b04b0);
    if (fixed != -1) {
        const uint32_t o = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(fixed) << 8);
        sw(pv + 0x1c4, 1);
        sw(pv + 0x1c0, o);
        lq_sq(pv + 0x180, o + 0x10);
    } else if (lbu(0x001413f4) == 1) {
        if (lw(OV(0x0015f5cc)) % 8 == lh(pv + 0x20a)) {
            class827_target_pick(m, pv + 0x1c0, lwc1(pv + 0x1e0));
            const uint32_t t = lwu(pv + 0x1c0);
            if (t == 0) {
                sw(pv + 0x1c4, 2);
            } else {
                const uint32_t zone = lwu(paths + (lwu(pv + 0x1f4) << 2));
                if (point_in_polygon(t + 0x10, zone + 0x10, lw(zone)) != 0 &&
                    (lh(t + 0xa6) != static_cast<int32_t>(MATE_CLASS) || !(2.0f < vec2_distance(V(m + 0x10), V(t + 0x10))))) {
                    sw(pv + 0x1c4, 0);
                } else {
                    sw(pv + 0x1c0, 0);
                    sw(pv + 0x1c4, 2);
                }
            }
        }
    } else {
        const uint32_t zone = lwu(paths + (lwu(pv + 0x1f4) << 2));
        if (target_select(m, pv + 0x180, 0, 0, zone + 0x10, lw(zone), lwc1(pv + 0x1e0)) != 2) {
            if (lwc1(pv + 0x1e0) < vec2_distance(V(pv + 0x210), V(pv + 0x180)))
                sw(pv + 0x1c4, 2);
            else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x188))))
                sw(pv + 0x1c4, 2);
        }
    }
    if (lwu(pv + 0x1c0) == 0) sw(pv + 0x1c0, lwu(HERO_MOBY));
    ee_stack_free(0x70);
}

// The states (the jump table L6:0x2013a0 (L10:0x1dce70), 17 entries, by its index): 0 the start (a rider spawned: 1;
// else 15, 3 or 8), 1 the rider's wait for its mate, 3 the watch, 4 along its path, 5 / 6 the jump to its end, 7 the
// landing, 8 the wander about home, 9 / 10 the chase, 11 the stop, 12 the attack, 13 the way home, 16 carried.
// orig: L6:0x002e8678 L10:0x002cc8b8
void class827_update(uint32_t m) {
    if (lw(tunings(0x20)) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    const int32_t mode = lw(OV(0x0015f5c4));
    if (mode == 2 && (lhu(m + 0x34) & 1u) == 0) {
        sh(pv + 0x1ea, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    } else if (mode == 0 && lh(pv + 0x1ea) != 0) {
        sh(pv + 0x1ea, 0);
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    }
    if (lbu(m + 0x20) != 16 && (lw(OV(0x0015fdf8)) != 0) != (lh(pv + 0x242) != 0)) return;
    joint_node_scale(m, 1, pv + 0x250, u2f(0x40066666u));
    class827_react(m);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 14.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0xb);
    }
    const uint32_t state = lbu(m + 0x20);
    if (state >= 17) return;
    const uint32_t target = lwu(pv + 0x1c0);
    const uint32_t sp = ee_stack_alloc(0xe0);

    // the state `next`, and the animation `set` (from `frame`, over `frames`) unless it plays already
    auto go = [m](uint32_t next, uint32_t set, int32_t frame, int32_t frames) {
        sb(m + 0x20, next);
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, frame, frames);
    };
    // a step toward the direction at `dir`: upright on the ground found below, turned toward it, moved forward at its
    // speed (the vector at `step`)
    auto walk = [m, pv](uint32_t dir, uint32_t step, uint32_t on_path) {
        class827_ground(m);
        class827_turn(m, OV(0x00173e80), dir);
        vec3_set_length(V(step), V(m + 0xc0), fpu_mul(lwc1(pv + 0x1e4), frame_time()));
        class827_move(m, step, on_path, u2f(0x3e99999au), u2f(0x3e99999au));
    };
    // the direction toward the target turned by +0x1d4, at `out`
    auto chase_dir = [m, pv, target](uint32_t out) {
        const float yaw = angle_add(yaw_to(m, target), lwc1(pv + 0x1d4));
        swc1(out, cos_vu(yaw));
        swc1(out + 4, sin_vu(yaw));
        sw(out + 8, 0);
    };
    // landed from the jump
    auto land = [m] {
        sb(m + 0x20, 7);
        if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, 1);
        swc1(m + 0x54, 1.0f);
        swc1(m + 0x18, floor_probe_v(0.5f, V(m + 0x10), 0));
    };

    switch (state) {
    case 0: {  // (0x002e87dc)
        const int32_t count = lw(tunings(0x24));
        sh(pv + 0x20a, static_cast<uint32_t>(count % 8));
        sw(tunings(0x24), static_cast<uint32_t>(count + 1));
        const int32_t parts = lh(pv + 0x240);
        if (parts != 0) sh(m + 0xb4, static_cast<uint32_t>(ee_div(lh(m + 0xb4), parts)));
        sh(m + 0x34, lhu(m + 0x34) | 0x100u);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(8.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(u2f(0x404ccccdu))));
        lq_sq(pv + 0x210, m + 0x10);
        lq_sq(pv + 0x230, m + 0x10);
        lq_sq(pv + 0x220, m + 0x40);
        sh(pv + 0x208, 0);
        const float reach = rand_range_f(0.5f, 3.0f);
        sw(pv + 0x148, 0);
        sw(pv + 0x134, 0);
        sw(pv + 0x140, 0x200);
        swc1(pv + 0x244, reach);
        swc1(pv + 0x130, fpu_mul(lwc1(0x0015ed70), 20.0f));
        swc1(pv + 0x248, rand_angle());
        sw(pv + 0xd0, tunings(0));
        sb(pv + 0x29, 1);
        if (lh(pv + 0x200) != 0) {
            sb(m + 0x20, 1);
            const uint32_t r = moby_spawn(RIDER_CLASS);
            sw(pv + 0x20c, r);
            if (r == 0) {
                moby_retire(m);
                break;
            }
            sd(r + 0x38, ld(m + 0x38));
            sh(r + 0x34, lhu(r + 0x34) | 0x4002u);
            lq_sq(r + 0x10, m + 0x10);
            lq_sq(r + 0x40, m + 0x40);
            sh(r + 0x32, 0x40);
            sb(r + 0x31, 1);
            moby_update_transform(r);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        } else if (lh(pv + 0x202) != 0) {
            sb(m + 0x20, 0xf);
            sw(m + 0x94, 0);
            sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
        } else if (lw(pv + 0x1f8) >= 0) {
            sb(m + 0x20, 3);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
        } else {
            sb(m + 0x20, 8);
            if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, rand_mod(lbu(lwu(lwu(m + 0x24) + 0x58) + 0x10)), 0);
        }
        break;
    }
    case 1:  // (0x002e89dc) the rider leaves when the mate is near
        if (vec2_distance(V(m + 0x10), V(target + 0x10)) < 4.0f) {
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            class827_rider_debris(m);
            go(8, 4, 0, frame_scale_i(5));
        }
        break;
    case 3:  // (0x002e8a30)
        vec3_sub(V(sp), V(target + 0x10), V(m + 0x10));
        class827_ground(m);
        class827_turn(m, OV(0x00173e80), sp);
        if (vec2_distance(V(HERO_POS), V(m + 0x10)) < lwc1(pv + 0x24c)) go(4, 4, 0, frame_scale_i(5));
        break;
    case 4: {  // (0x002e8a9c) along the path's points to its last, then a jump there
        const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x1f8) << 2));
        lq_sq(sp + 0x10, path + 0x10 + (static_cast<uint32_t>(lh(pv + 0x208)) << 4));
        vec3_sub(V(sp), V(sp + 0x10), V(m + 0x10));
        walk(sp, sp + 0x20, 0);
        mat3_transform(V(sp), V(sp), M(m + 0xc0));
        if (!(vec2_length(V(sp)) < 0.5f)) break;
        const int32_t n = lw(path);
        if (lh(pv + 0x208) < n - 2) {
            sh(pv + 0x208, lhu(pv + 0x208) + 1u);
            break;
        }
        lq_sq(pv + 0x210, path + (static_cast<uint32_t>(n) << 4));
        lq_sq(pv + 0x230, pv + 0x210);
        const float speed = fpu_div(vec2_distance(V(m + 0x10), V(pv + 0x210)), int_to_float(frame_scale_i(0x1e)));
        swc1(pv + 0x138, speed);
        swc1(pv + 0x13c, sub_0025abf0(m + 0x10, pv + 0x210, nullptr, speed, -lwc1(pv + 0x130)));
        sb(pv + 0x15d, 3);
        sw(pv + 0x144, 5);
        const float yaw = atan2_f(fpu_sub(lwc1(pv + 0x210), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x214), lwc1(m + 0x14)));
        move_state_start(m, pv + 0x120, 2, 1, 0, yaw);
        sb(m + 0x20, 5);
        break;
    }
    case 5:  // (0x002e8c68)
        vec_set_unit_z(V(sp));
        class827_turn(m, sp, pv + 0x120);
        if ((enemy_physics(m, pv + 0x120) & 1u) != 0) land();
        break;
    case 6: {  // (0x002e8ca4)
        const uint32_t r = enemy_physics(m, pv + 0x120);
        if ((r & 1u) != 0) {
            land();
        } else if ((r & 0x120u) != 0) {
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
        }
        break;
    }
    case 7:  // (0x002e8d34)
        if ((lbu(m + 0x70) & 2u) != 0) go(8, 4, 0, frame_scale_i(5));
        break;
    case 8: {  // (0x002e8d5c) about home: a new goal there when reached or when its timer runs out
        if (lbu(m + 0x31) != 0) {
            const uint32_t goal = pv + 0x230;
            if (vec3_distance(V(m + 0x10), V(goal)) < u2f(0x3d4ccccdu) || tick32(pv + 0x1fc) != 0) {
                const float a = rand_angle();
                const float r = rand_range_f(0.5f, 2.0f);
                swc1(sp, fpu_mul(cos_vu(a), r));
                swc1(sp + 4, fpu_mul(sin_vu(a), r));
                sw(sp + 8, 0);
                vec3_add(V(goal), V(pv + 0x210), V(sp));
                sw(pv + 0x1fc, static_cast<uint32_t>(frame_scale_i(float_to_int(rand_range_f(45.0f, 75.0f)))));
            }
            vec3_sub(V(sp + 0x20), V(goal), V(m + 0x10));
            walk(sp + 0x20, sp + 0x10, 1);
        }
        uint32_t next;
        if (lw(pv + 0x1c4) != 2 && rand_mod(9) == 0) next = 9;
        else if (lh(pv + 0x1e8) != 0) next = 0xa;
        else break;
        const float spread = lwc1(pv + 0x1d8);
        swc1(pv + 0x1d4, fpu_mul(rand_range_f(-spread, spread), u2f(0x3c8efa35u)));
        go(next, 4, 0, 0);
        break;
    }
    case 9:  // (0x002e8f38)
        chase_dir(sp + 0x10);
        walk(sp + 0x10, sp, 1);
        if (vec2_distance(V(m + 0x10), V(target + 0x10)) < 2.0f) go(0xc, 5, 4, frame_scale_i(10));
        else if (lw(pv + 0x1c4) == 2) sb(m + 0x20, 0xd);
        break;
    case 10: {  // (0x002e904c)
        chase_dir(sp + 0x10);
        walk(sp + 0x10, sp, 1);
        const bool crossed = path_crossing(lw(pv + 0x1ec), m + 0x10, target + 0x10, sp + 0x30) != 0 &&
                             vec3_distance(V(m + 0x10), V(sp + 0x30)) < lwc1(pv + 0x244);
        if (lh(pv + 0x1e8) == 0) sb(m + 0x20, 0xd);
        else if (crossed) go(0xb, 6, 0, frame_scale_i(10));
        else if (vec2_distance(V(m + 0x10), V(target + 0x10)) < 2.0f) go(0xc, 5, 0, frame_scale_i(10));
        break;
    }
    case 11:  // (0x002e91dc)
        if ((lbu(m + 0x70) & 2u) != 0) go(0xa, 4, 0, frame_scale_i(0x14));
        break;
    case 12: {  // (0x002e9204) the attack: a lunge (twice the speed) and the hit between frames 10 and 15 of its animation
        swc1(sp + 0x10, cos_vu(yaw_to(m, target)));
        swc1(sp + 0x14, sin_vu(yaw_to(m, target)));
        sw(sp + 0x18, 0);
        class827_turn(m, m + 0xe0, sp + 0x10);
        const float t = moby_anim_time(m);
        if (lbu(m + 0x52) == lbu(m + 0x53) && 10.0f <= t && t <= 15.0f && lw(0x001413d4) != 0x51) {
            vec3_set_length(V(sp), V(m + 0xc0), fpu_mul(fpu_add(lwc1(pv + 0x1e4), lwc1(pv + 0x1e4)), frame_time()));
            class827_move(m, sp, 1, u2f(0x3e99999au), u2f(0x3e99999au));
            moby_chain_contact(m, 0, 1, 0, 1, 0, u2f(0x3eaa7efau), 1.0f);
            break;
        }
        if ((lbu(m + 0x70) & 2u) != 0 && 2.0f < vec2_distance(V(m + 0x10), V(target + 0x10))) go(9, 4, 0, frame_scale_i(10));
        break;
    }
    case 13:  // (0x002e937c) home
        vec3_sub(V(sp + 0x10), V(pv + 0x210), V(m + 0x10));
        walk(sp + 0x10, sp, 1);
        if (vec2_distance(V(m + 0x10), V(pv + 0x210)) < 2.0f) go(8, 4, 0, frame_scale_i(10));
        else if (lw(pv + 0x1c4) != 2) sb(m + 0x20, 9);
        else if (lh(pv + 0x1e8) != 0) sb(m + 0x20, 0xa);
        break;
    case 16:  // (0x002e947c)
        if (carry_update(m, pv + 0x120) != 0) {
            sb(m + 0x20, 8);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(10));
            sh(pv + 0xc8, 0);
        }
        break;
    default:  // 2, 14, 15 (0x002e94bc)
        break;
    }
    ee_stack_free(0xe0);
}

namespace {
// a carry hook's end: let go (the hook returned 0) while carried (16): wander (8) and its rest animation
int32_t carry_release(uint32_t m, int32_t r) {
    if (r == 0 && lbu(m + 0x20) == 0x10) {
        sb(m + 0x20, 8);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(10));
    }
    return r;
}
// carried: state 16, the state it had kept at +0xbc
void carry_take(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0x10) return;
    sb(m + 0x20, 0x10);
    sb(m + 0xbc, state);
}
}  // namespace

// The carry hook 0: taken only from the wander or the chase (states 8, 9, 12, 13), held while carried; from any other
// state the hook's answer is 0.
// orig: L6:0x002ea5e0 L10:0x002ce618
int32_t class827_hook_carry0(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t r = carry_hook0(m, a1, a2, f12);
    if (r == 0) return carry_release(m, r);
    const uint32_t state = lbu(m + 0x20);
    if (state == 8 || state == 9 || state == 0xc || state == 0xd) {
        carry_take(m);
        return r;
    }
    return state == 0x10 ? r : 0;
}

// The carry hooks 4 and 8 (`which`): carried from any state.
// orig: L6:0x002ea6a8 L6:0x002ea738 L10:0x002ce6e0 L10:0x002ce770
int32_t class827_hook_carry(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which) {
    const int32_t r = which == 4 ? carry_hook4(m, a1, a2, f12) : carry_hook8(m, a1, a2, f12);
    if (r == 0) return carry_release(m, r);
    carry_take(m);
    return r;
}

// The carry hook 0xc: carried whatever it answers.
// orig: L6:0x002ea7c8 L10:0x002ce800
int32_t class827_hook_carryc(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t r = carry_hookc(m, a1, a2, f12);
    carry_take(m);
    return r;
}

}  // namespace rac1
