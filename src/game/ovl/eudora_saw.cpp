// Eudora's class 340 (0x154), the Circular Saw Enemy: a robot with a spinning saw. It patrols a path (+0x150), resting a
// while in the level boxes it crosses (+0x160 .. +0x16c), or wanders inside its area (the polygon of the path +0x138);
// a target in sight (doubled once alerted: pvars +0x38 or the timer +0x136) it chases, circling to an attack point spread
// among the other saws near the target, and saws it close up (the hero hurt by its four saw joints). Hit, it is pushed
// back facing the hit (state 6) or dies (its bolts dropped, an explosion at its joint 5). The hero's and the mobys' carry
// system can pick it up: its class hooks +0 / +4 / +8 (carry_hooks_levels.cpp) and +0xc (Veldin's class-749 hook) set
// state 9, in which this update runs carry_update on it until it settles, then sets it up again. A transport ship drops
// it in state 2 (a leap). Its saw's sound follows the animation (volume and pitch eased per animation set). Header
// eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR, the
// base ELF's are bare.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position, +0x240 the moby he touches)
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
const float kPi = u2f(0x40490fdbu);
const float kTwoPi = u2f(0x40c90fdbu);
const float kHalfPi = u2f(0x3fc90fdbu);
const float kThirdPi = u2f(0x3f860a92u);
const float kEighthPi = u2f(0x3ec90fdbu);
const float kTenDegrees = u2f(0x3e32b8c2u);
const float kStepRadius = u2f(0x3f11eb85u);  // (0.57: the walker's radius)
const float kStepSlope = u2f(0x3f060a92u);   // (pi / 6: the steepest slope it walks)

// The saw's sound (its slot +0x238) stopped if the slot still plays it for this saw; the slot forgotten.
void saw_sound_stop(uint32_t m, uint32_t pv) {
    if (const int32_t slot = lw(pv + 0x238); slot != -1) {
        const uint32_t c = 0x0013e550 + static_cast<uint32_t>(slot) * 0x70;  // (a sound slot: owner +0x88, playing +0x74)
        if (lwu(c + 0x88) == m && lbu(c + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x238, 0xffffffffu);
}

// the animation `set` from its start, blended over `frames` frames
void anim_to(uint32_t m, uint32_t set, int32_t frames) { moby_anim_set(m, set, 0, frame_scale_i(frames)); }

// whether the animation has played out
bool anim_ended(uint32_t m) { return (lbu(m + 0x70) & 2) != 0; }

// whether the point `p` is inside the saw's area (the polygon of the path +0x138; -1: no area)
bool in_area(uint32_t pv, uint32_t p) {
    const int32_t area = lw(pv + 0x138);
    if (area == -1) return true;
    const uint32_t poly = lvd::path(area);
    return point_in_polygon(p, poly + 0x10, lw(poly)) != 0;
}

// a turn toward `heading` (+0x130 the turn's speed)
void saw_turn(uint32_t m, uint32_t pv, float heading) {
    yaw_turn_damped(m, pv + 0x130, heading, fpu_mul(dt_sq(), kThirdPi), fpu_mul(dt_sq(), kPi), fpu_mul(dt(), kPi));
}

// the heading from the saw to the point `p`
float heading_to(uint32_t m, uint32_t p) { return atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14))); }

// the range it sees a target within (doubled once alerted)
float sight_range(uint32_t pv, bool alerted) {
    return alerted ? fpu_add(lwc1(pv + 0x140), lwc1(pv + 0x140)) : lwc1(pv + 0x140);
}
bool alerted_now(uint32_t pv) { return lw(pv + 0x38) != 0 || lh(pv + 0x136) != 0; }
}  // namespace

// The saw's sound: carried (state 9) or in an animation set from 8 on, stopped; else started once it no longer plays
// (sound 0, volume 1024, pitch 0) and its volume (+0x230) and pitch (+0x234) eased toward the animation's: idle 0 at
// 0.6 of full volume, the moves 1 / 2 / 3-4 at full volume pitched 3 / -2 / 4, the saw 5-7 at 0.8.
// orig: L4:0x002c1b80
void saw_sound_update_l4(uint32_t m) {
    ee_stack_alloc(0x40);  // (the original's frame; it keeps no locals there)
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 9) {
        saw_sound_stop(m, pv);
        ee_stack_free(0x40);
        return;
    }
    if (sub_0028d8c0(m, lw(pv + 0x238)) == 0 && lbu(m + 0x53) < 8) {
        sw(pv + 0x238, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        swc1(pv + 0x230, 1024.0f);
        sw(pv + 0x234, 0);
    }
    const uint32_t set = lbu(m + 0x53);
    if (!(set < 8)) {
        saw_sound_stop(m, pv);
        ee_stack_free(0x40);
        return;
    }
    float volume = 1024.0f, pitch = 0.0f;
    switch (set) {  // (the table L4:0x1dbc00)
    case 0:  // (0x002c1c18)
        volume = fpu_mul(1024.0f, u2f(0x3f19999au));
        break;
    case 1:  // (0x002c1c38)
        pitch = 3.0f;
        break;
    case 2:  // (0x002c1c48)
        pitch = -2.0f;
        break;
    case 3: case 4:  // (0x002c1c58)
        pitch = 4.0f;
        break;
    default:  // 5, 6, 7 (0x002c1c74)
        volume = fpu_mul(1024.0f, u2f(0x3f4ccccdu));
        break;
    }
    const float rate = fpu_mul(1024.0f, dt());
    approach_f(pv + 0x230, volume, fpu_add(rate, rate));
    approach_f(pv + 0x234, pitch, fpu_mul(dt(), 8.0f));
    sound_slot_volume_set(lw(pv + 0x238), static_cast<uint32_t>(float_to_int(lwc1(pv + 0x230))));
    metal_28df38(lwu(pv + 0x238), static_cast<uint32_t>(float_to_int(lwc1(pv + 0x234))));
    ee_stack_free(0x40);
}

// The saw's death: its sound stopped, its bolts dropped, an explosion at its joint 5, the sound 1.
// orig: L4:0x002c1d70
void saw_die_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t at = sp + 0x20, none = sp + 0x30;
    vec_clear(V(none));
    saw_sound_stop(m, pv);
    moby_drop_bolts(m, 0, -1);
    moby_chain_point(m, 5, at);
    explosion_effect(m, none, at, 0xa, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 100000.0f, 1.0f, 15.0f);
    moby_sound_start(1, 0, m);
    ee_stack_free(0x90);
}

// Whether the saw may attack its target: no other saw (class 0x154) within 3 of it, nor nearer the target than it.
// orig: L4:0x002c1e98
int32_t saw_may_attack_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t target = sp, pos = m + 0x10;
    target_select_any(m, target, sight_range(pv, alerted_now(pv)));
    const float dist = vec2_distance(V(pos), V(target));
    int32_t may = 1;
    for (uint32_t other = lwu(L4_ADDR(0x0015ffe4)); other != 0; other = lwu(other + 0x28)) {  // (the live mobys)
        if (other == m || lh(other + 0xa6) != 0x154) continue;
        if (vec2_distance(V(pos), V(other + 0x10)) < 3.0f || vec2_distance(V(other + 0x10), V(target)) < dist) {
            may = 0;
            break;
        }
    }
    ee_stack_free(0xc0);
    return may;
}

// The saw's attack point into `out`, and the heading to it. Alone (no other saw of its class nearer the target), the
// target itself. Else the mean of its pushes away from the saws within 3 (each weighted by L4:0x161884) and of a push
// 10 times as heavy keeping it 4.5 .. 5.5 from the target, from its place; within 1 of it, its place (the heading then
// to the target).
// orig: L4:0x002c1fb8
float saw_attack_point_l4(uint32_t m, uint32_t out) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t push = sp, target = sp + 0x10, pos = m + 0x10;
    target_select_any(m, target, sight_range(pv, alerted_now(pv)));
    const float dist = vec2_distance(V(pos), V(target));
    vec_clear(V(out));
    float weight = 0.0f;
    bool alone = true;
    for (uint32_t other = lwu(L4_ADDR(0x0015ffe4)); other != 0; other = lwu(other + 0x28)) {  // (the live mobys)
        if (other == m || lh(other + 0xa6) != lh(m + 0xa6)) continue;
        if (vec2_distance(V(other + 0x10), V(pos)) < 3.0f) {
            vec3_sub(V(push), V(pos), V(other + 0x10));
            vec3_scale(V(push), V(push), lwc1(L4_ADDR(0x00161884)));
            vec3_add(V(out), V(out), V(push));
            weight = fpu_add(weight, lwc1(L4_ADDR(0x00161884)));
        }
        if (vec2_distance(V(other + 0x10), V(target)) < dist) alone = false;
    }
    if (alone) {
        lq_sq(out, target);
    } else {
        vec3_sub(V(push), V(pos), V(target));
        float length = 0.0f;
        if (dist < 4.5f) {
            length = 5.0f;  // (too near: away)
        } else if (5.5f < dist) {
            length = -5.0f;  // (too far: closer)
        }
        length = fpu_mul(length, 10.0f);
        weight = fpu_add(weight, 10.0f);
        vec3_set_length(V(push), V(push), length);
        vec3_add(V(out), V(out), V(push));
        vec3_scale(V(out), V(out), fpu_div(1.0f, weight));
        vec3_add(V(out), V(out), V(pos));
    }
    float heading;
    if (vec2_distance(V(pos), V(out)) < 1.0f) {
        lq_sq(out, pos);
        heading = atan2_f(fpu_sub(lwc1(target), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
    } else {
        heading = atan2_f(fpu_sub(lwc1(out), lwc1(m + 0x10)), fpu_sub(lwc1(out + 4), lwc1(m + 0x14)));
    }
    ee_stack_free(0x100);
    return heading;
}

// pvars: health +0x20 (the hit state +0x20 .. +0x30; +0x2e 1 while on the level), the alert request +0x38, the movement
// record +0x60 (its +0x18, pvars +0x78, the fall speed), the colour pulse +0xb0 (+0xb7 its strength), carry_update's
// state +0xc0 (+0xd0 .. +0xfd also set by a hit), its home +0x120 (+0x128 the height it dies 5 below), the turn's speed
// +0x130, the alerted timer +0x136, its area's path +0x138, the push-back speed +0x13c, its sight +0x140, the chase
// animation's hold +0x144, the action timer +0x146, the chase's side +0x14c (0 .. 3: the animations of L4:0x1618a0),
// the hit cool-down +0x14d, the flags +0x14e (bit 0 a target seen, bit 1 sawed), the patrol path +0x150 (its point
// +0x154), the box it rests in +0x158 (an index of the level's boxes; +0x15a 0 going there, 1 resting, 2 back to the
// path), its four boxes +0x160 .. +0x16c (-1: none), the carry record +0x180 (class hook +0x10; its state +0x1e8, its
// parameters +0x1f0), the saw's sound: volume +0x230, pitch +0x234, slot +0x238, the wander record +0x240, the saw's
// joint +0x270. States: 0 set up, 1 wanders, 2 dropped by a ship (leaping off it), 3 chases, 4 patrols its path,
// 5 rests in a box, 6 pushed back by a hit, 7 saws, 8 dies once its animation ends (set elsewhere), 9 carried.
// orig: L4:0x002c2270
void eud_class340_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x1a0);
    const uint32_t pv = lwu(m + 0x78);
    // (the EE frame's locals: the target +0 (+0x40 its moby), the point it heads for +0x60, the step / the saw's hit on
    // the hero +0x70 .. +0x93 (its +0x18 never written), the place walked from / the sparks' place +0x80, the saw's joint
    // +0xa0, the hit's state / damage words +0xc0 / +0xc4)
    const uint32_t target = sp, goal = sp + 0x60, step = sp + 0x70, from = sp + 0x80, joint = sp + 0xa0;
    const uint32_t hit_state = sp + 0xc0, hit_dmg = sp + 0xc4;
    const uint32_t pos = m + 0x10;
    const bool alerted = alerted_now(pv);
    const float yaw0 = lwc1(m + 0x48);
    joint_node_scale(m, 7, pv + 0x270, u2f(0x4039999au));
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L4_ADDR(0x00166f40))) < 27.0f) {  // (its floor band near the camera point)
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    target_select_any(m, target, sight_range(pv, alerted));
    const uint32_t victim = lwu(target + 0x40);
    const float frame = moby_anim_time(m);
    const float dist = vec2_distance(V(pos), V(target));
    lq_sq(goal, pos);
    sb(m + 0x30, 0x20);
    saw_sound_update_l4(m);
    // wandering or chasing, touched by the hero (or within 1.5 of him): sawing
    if (const uint32_t s = lbu(m + 0x20); s == 1 || s == 3) {
        if (lwu(HERO + 0x240) == m || vec3_distance(V(pos), V(HERO + 0x80)) < 1.5f) {
            anim_to(m, 7, 0xa);
            sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x3c)));
            sb(m + 0x20, 7);
        }
    }
    countdown_u8(pv + 0x14d);

    // ---- the hit (the weapons' mask 0x210000), unless setting up or dying; none while the cool-down +0x14d runs
    if (const uint32_t s = lbu(m + 0x20); s != 8 && s != 0) {
        sw(hit_dmg, 0);
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x210000), 0);
        int32_t reaction = moby_hit_react(m, hit, pv + 0x20, 0, hit_state, hit_dmg, 0, 4);
        if (hit != 0 && lbu(m + 0x20) != 0 && lbu(pv + 0x14d) == 0) {
            const uint32_t src = lwu(hit + 0x20);
            const uint32_t at = (src == 0 || src == lwu(0x001403e0)) ? HERO + 0x80 : src + 0x10;
            const float hit_yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(at)), fpu_sub(lwc1(m + 0x14), lwc1(at + 4)));
            swc1(pv + 0xd0, u2f(0x3c03126fu));
            swc1(pv + 0xd4, u2f(0x3a03126fu));
            swc1(pv + 0xd8, fpu_mul(dt(), 4.0f));
            sw(pv + 0xe4, 9);
            sb(pv + 0xfd, 0);
            if (lhu(lwu(hit + 0x20) + 0xa6) - 0xb0u < 2u) {  // (by the classes 0xb0 / 0xb1: a damage of 1)
                swc1(hit + 0x2c, 1.0f);
                swc1(hit_dmg, 1.0f);
            }
            const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
            swc1(pv + 0x20, hp);
            if (hp <= 0.0f) reaction = 1;
            sh(pv + 0x136, static_cast<uint32_t>(frame_scale_i(0xf0)));
            switch (static_cast<uint32_t>(reaction) < 12 ? reaction : 0) {  // (the table L4:0x1dbc20)
            case 1: case 2:  // (0x002c3e48) killed
                saw_die_l4(m);
                moby_retire(m);
                ee_stack_free(0x1a0);
                return;
            case 3: case 4: case 5: case 6: {  // (0x002c25e4) pushed back facing the hit (half as far from 0x131 / 0xb0)
                anim_to(m, 8, 5);
                const int32_t by = lh(lwu(hit + 0x20) + 0xa6);
                const float push = (by == 0x131 || by == 0xb0) ? fpu_mul(fpu_div(10.0f, frame_scale_f(15.0f)), 0.5f)
                                                               : fpu_div(10.0f, frame_scale_f(15.0f));
                swc1(pv + 0x13c, push);
                sb(m + 0x20, 6);
                swc1(m + 0x48, angle_add(kPi, hit_yaw));
                for (int i = 0; i < 2; ++i) {  // (twice in the original)
                    sb(pv + 0xb7, 0x78);
                    color_pulse_start(m, pv + 0xb0);
                    sb(pv + 0x14d, static_cast<uint32_t>(frame_scale_i(0x14)));
                }
                break;
            }
            default:  // 0, 7 .. 11 (0x002c26cc)
                break;
            }
        }
    }
    sb(m + 0xa4, 0xff);
    if (lw(pv + 0x38) != 0) sh(pv + 0x136, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x38, 0);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x136)));

    // ---- sawing (its animation 7, frames 15 .. 44): its four saw joints (L4:0x161890) hurt the hero; hit, the
    // target holds the action timer at most a second
    if (lbu(m + 0x20) == 7 && lbu(m + 0x52) == 7 && 15.0f < frame && frame < 44.0f) {
        for (uint32_t i = 0; i < 4; ++i) {
            moby_chain_point(m, lwu(L4_ADDR(0x00161890) + i * 4), joint);
            sw(step + 0x10, m);  // (the hit: its source, kind, damage 1, ...)
            sw(step + 0x14, 0x10001);
            swc1(step + 0x1c, 1.0f);
            vec3_sub(V(step), V(HERO + 0x80), V(pos));  // (its push: toward the hero)
            vec3_set_length(V(step), V(step), fpu_mul(dt(), 10.0f));
            sw(step + 0x20, 1);
            mobys_in_sphere(joint, 0.5f, 0x10, m, step);
            if (coll_hit_moby() == victim && !(lh(pv + 0x146) < frame_scale_i(0x3c)))
                sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
    }
    if (lbu(m + 0x20) != 0 && lbu(pv + 0x2e) != 1) {  // (taken off the level)
        saw_sound_stop(m, pv);
        moby_retire(m);
        ee_stack_free(0x1a0);
        return;
    }
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x146)));

    // ---- the states
    // A target in sight (and, unless alerted, inside its area): the first time the alert animation 6; within 2, the
    // action timer out and no other saw in the way, sawing (state 7); else, still in sight once the timer is out,
    // chasing (state 3, animation 1).
    enum class Notice { None, Saw, Chase };
    auto notice = [&] {
        if (!(dist < sight_range(pv, alerted))) return Notice::None;
        if (!alerted && !in_area(pv, target)) return Notice::None;
        if (const uint32_t flags = lhu(pv + 0x14e); (flags & 1) == 0) {
            sh(pv + 0x14e, flags | 1u);
            anim_to(m, 6, 0xa);
            sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
        // (the original computes the heading to the target here, unused)
        if (dist < 2.0f && lh(pv + 0x146) == 0 && saw_may_attack_l4(m) != 0) {
            anim_to(m, 7, 0xa);
            sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x3c)));
            sb(m + 0x20, 7);
            return Notice::Saw;
        }
        if (!(dist < sight_range(pv, alerted)) || lh(pv + 0x146) != 0) return Notice::None;
        anim_to(m, 1, 0xa);
        sb(pv + 0x14c, 0);
        sb(m + 0x20, 3);
        return Notice::Chase;
    };
    auto path_point = [&](uint32_t path) { return path + 0x10 + (lwu(pv + 0x154) << 4); };
    bool dies = false;
    const uint32_t state = lbu(m + 0x20);
    switch (state < 10 ? state : 10) {  // (the table L4:0x1dbc50)
    case 0:  // (0x002c28f8) set up
        swc1(pv + 0x20, 1.0f);
        sh(pv + 0x24, 1);
        sb(pv + 0x28, 0);
        sb(pv + 0x2e, 1);
        sb(pv + 0x29, 0);
        swc1(pv + 0x30, 0.75f);
        sb(pv + 0x58, 8);
        sb(pv + 0x5a, 6);
        walker_record_init_l1(pv + 0x60);
        swc1(pv + 0x6c, u2f(0x3e19999au));
        swc1(pv + 0x68, 0.25f);
        sw(pv + 0x60, 0x200);
        swc1(pv + 0x84, fpu_mul(dt(), 3.0f));
        swc1(pv + 0x64, 1.0f);
        sw(pv + 0x98, 4);
        lq_sq(pv + 0x240, pos);  // (the wander record: its centre, rates)
        swc1(pv + 0x25c, 2.0f);
        swc1(pv + 0x254, fpu_add(dt(), dt()));
        swc1(pv + 0x258, fpu_mul(dt(), u2f(0x403de44eu)));
        lq_sq(pv + 0x120, pos);  // (home)
        sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sw(pv + 0x1f0, L4_ADDR(0x00161878));  // (the carry record's parameters)
        sh(pv + 0x136, 0);
        sh(pv + 0x144, 0);
        sb(m + 0x20, lw(pv + 0x150) != -1 ? 4 : 1);  // (a path: patrolling it)
        break;
    case 1:  // (0x002c29fc) wandering inside its area (idle animation 0); a target noticed
        lq_sq(step, pos);
        wander_update(m, pv + 0x240, u2f(0x3eb33333u), 0.25f);
        if (!in_area(pv, pos)) lq_sq(pos, step);  // (kept inside)
        if (lbu(m + 0x52) != 0 && anim_ended(m)) anim_to(m, 0, 5);
        notice();
        break;
    case 2:  // (0x002c2c4c) dropped by a ship: leaping off (animation 5, rising), then wandering
        if (lbu(m + 0x53) != 5) {
            anim_to(m, 5, 5);
            swc1(pv + 0x78, -fpu_mul(dt(), u2f(0x40f56042u)));
            break;
        }
        if (anim_ended(m)) {
            anim_to(m, 0, 0xa);
            sb(m + 0x20, 1);
        }
        break;
    case 3: {  // (0x002c2cd4) chasing toward its attack point; out of sight (not alerted) going home, there wandering
        float heading = yaw0;  // (lost, alerted: keeps its heading)
        if (dist < sight_range(pv, alerted) && (alerted || in_area(pv, target))) {
            heading = saw_attack_point_l4(m, goal);
            if (dist < 2.0f && lh(pv + 0x146) == 0 && saw_may_attack_l4(m) != 0) {
                anim_to(m, 7, 0xa);
                sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x3c)));
                sb(m + 0x20, 7);
            }
        } else if (!alerted) {
            lq_sq(goal, pv + 0x120);
            if (vec2_distance(V(pos), V(pv + 0x120)) < 1.0f) {
                anim_to(m, 0, 0xa);
                sb(m + 0x20, 1);
            }
        }
        saw_turn(m, pv, heading);
        break;
    }
    case 4: {  // (0x002c2e3c) patrolling its path; a target noticed; resting in a box it enters
        const uint32_t path = lvd::path(lw(pv + 0x150));
        notice();
        // the next point once within 1 of this one or this one more than 135 degrees off its heading (wrapping round)
        bool next = vec2_distance(V(path_point(path)), V(pos)) < 1.0f;
        if (!next) next = u2f(0x4016cbe4u) < angle_dist(lwc1(m + 0x48), heading_to(m, path_point(path)));
        if (next) {
            const uint32_t point = lwu(pv + 0x154) + 1;
            sw(pv + 0x154, point);
            if (point == lwu(path)) sw(pv + 0x154, 0);
        }
        for (uint32_t i = 0; i < 4; ++i) {  // (once the action timer is out)
            if (lh(pv + 0x146) != 0) continue;
            const int32_t box = lw(pv + 0x160 + i * 4);
            if (box == -1 || point_in_box(pos, box) == 0) continue;
            sh(pv + 0x158, lhu(pv + 0x160 + i * 4));
            sb(m + 0x20, 5);
            sh(pv + 0x15a, 0);
            break;
        }
        lq_sq(goal, path_point(path));
        saw_turn(m, pv, heading_to(m, path_point(path)));
        break;
    }
    case 5: {  // (0x002c316c) a box: to its centre and heading, resting (animation 7 once), back to its path's heading
        const uint32_t path = lvd::path(lw(pv + 0x150));
        if (notice() == Notice::Saw) sw(pv + 0x130, 0);
        auto box = [&] { return lvd::box(lvd::BOXES, lh(pv + 0x158)); };
        switch (lh(pv + 0x15a)) {
        case 0:  // going there (animation 1)
            lq_sq(goal, box() + 0x30);
            saw_turn(m, pv, lwc1(box() + 0x78));
            if (lbu(m + 0x53) != 1) anim_to(m, 1, 0xa);
            if (vec2_distance(V(pos), V(goal)) < 1.0f && angle_dist(lwc1(m + 0x48), lwc1(box() + 0x78)) < kTenDegrees)
                sh(pv + 0x15a, 1);
            break;
        case 1:  // resting
            lq_sq(goal, box() + 0x30);
            if (lbu(m + 0x53) != 7) {
                anim_to(m, 7, 0xa);
            } else if (anim_ended(m)) {
                anim_to(m, 0, 0xa);
                sw(pv + 0x130, 0);
                sh(pv + 0x15a, 2);
            }
            break;
        case 2:  // turning to its path's point: then patrolling, the boxes left alone four seconds
            lq_sq(goal, box() + 0x30);
            saw_turn(m, pv, heading_to(m, path_point(path)));
            if (angle_dist(lwc1(m + 0x48), heading_to(m, path_point(path))) < kTenDegrees) {
                sb(m + 0x20, 4);
                sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0xf0)));
            }
            break;
        default:
            break;
        }
        break;
    }
    case 6:  // (0x002c36c8) pushed back (animation 8): past its frame 30 or played out, wandering
        if ((lbu(m + 0x52) == 8 && 30.0f < moby_anim_time(m)) || anim_ended(m)) {
            anim_to(m, 0, 0xa);
            sb(m + 0x20, 1);
            sh(pv + 0x146, 0);
        }
        break;
    case 7: {  // (0x002c35e8) sawing, turned to its attack point; past frame 44 or played out, wandering again
        const float heading = saw_attack_point_l4(m, goal);
        if (lh(pv + 0x146) == 0 && (44.0f < frame || anim_ended(m))) {
            anim_to(m, 0, 5);
            sh(pv + 0x146, static_cast<uint32_t>(frame_scale_i(0x78)));
            sb(m + 0x20, 1);
            sh(pv + 0x14e, lhu(pv + 0x14e) | 2u);
        }
        saw_turn(m, pv, heading);
        break;
    }
    case 8:  // (0x002c3738) dying once its animation ends
        dies = anim_ended(m);
        break;
    case 9:  // (0x002c3750) carried: the carry system moves it (its record from the class hook +0x10, carry_update's
             // state at +0xc0); once it settles, set up again, the record's state cleared
        if (carry_update(m, pv + 0xc0) != 0) {
            sb(m + 0x20, 0);
            sh(pv + 0x1e8, 0);
        }
        break;
    default:
        break;
    }
    if (dies) {  // (0x002c3e48)
        saw_die_l4(m);
        moby_retire(m);
        ee_stack_free(0x1a0);
        return;
    }

    // ---- every frame: the push-back (+0x13c, slowing down)
    if (0.0f < lwc1(pv + 0x13c)) {
        swc1(step, fpu_mul(cos_vu(angle_add(lwc1(m + 0x48), kPi)), lwc1(pv + 0x13c)));
        swc1(step + 8, 0.0f);
        swc1(step + 4, fpu_mul(sin_vu(angle_add(lwc1(m + 0x48), kPi)), lwc1(pv + 0x13c)));
        vec3_add(V(step), V(step), V(pos));
        const float t = fpu_mul(frame_scale_f(15.0f), frame_scale_f(15.0f));
        swc1(pv + 0x13c, fpu_add(lwc1(pv + 0x13c), fpu_div(-10.0f, t)));
        walker_step(m, pos, step, 1, 0.25f, kStepRadius, 0.5f, kStepSlope);
    }
    // the walk by state: from its place (`from`) to `step`, kept inside its area
    auto walk = [&](float radius) {
        lq_sq(from, pos);
        const int32_t stepped = walker_step(m, from, step, 0, 0.25f, radius, 0.5f, kStepSlope);
        const bool inside = in_area(pv, from);
        if (inside) lq_sq(pos, from);
        return inside && stepped != 0;
    };
    const uint32_t now = lbu(m + 0x20);
    if (now == 9) {  // (carried: nothing more)
        ee_stack_free(0x1a0);
        return;
    }
    if (now == 4) {  // patrolling: ahead at twice the frame time (animation 1)
        if (lbu(m + 0x53) != 1) anim_to(m, 1, 0xa);
        swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_add(dt(), dt())));
        swc1(step + 8, 0.0f);
        swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_add(dt(), dt())));
        vec3_add(V(step), V(step), V(pos));
        walk(kStepRadius);
    } else if (now == 3) {
        // chasing: toward its point at three times the frame time, sideways or backward as it faces, on the animation
        // of that side (L4:0x1618a0, chosen again once more than 100 degrees off); stopped (or not moved), animation 0,
        // changed at most every 15 frames (+0x144)
        const float heading = heading_to(m, goal);
        uint32_t set = lbu(m + 0x53);
        const float speed = fpu_mul(dt(), 3.0f);
        if (fpu_add(speed, speed) <= vec2_distance(V(pos), V(goal))) {
            const float off = angle_sub(heading, lwc1(m + 0x48));
            float sector = angle_add(off, kEighthPi);
            if (sector < 0.0f) sector = fpu_add(sector, kTwoPi);
            const float facing = angle_wrap(angle_wrap(fpu_mul(static_cast<float>(lbu(pv + 0x14c)), kHalfPi)));
            if (u2f(0x3fdf66f3u) < angle_dist(facing, off)) {
                const int32_t side = float_to_int(fpu_div(fpu_mul(sector, u2f(0x3fffdf3bu)), kPi));
                sb(pv + 0x14c, static_cast<uint32_t>(side));
                set = lwu(L4_ADDR(0x001618a0) + (static_cast<uint32_t>(side) & 0xffu) * 4);
            }
            const float side_yaw = angle_wrap(fpu_mul(static_cast<float>(lbu(pv + 0x14c)), kHalfPi));
            swc1(step, fpu_mul(cos_vu(angle_add(lwc1(m + 0x48), side_yaw)), fpu_mul(dt(), 3.0f)));
            swc1(step + 8, 0.0f);
            swc1(step + 4, fpu_mul(sin_vu(angle_add(lwc1(m + 0x48), side_yaw)), fpu_mul(dt(), 3.0f)));
            vec3_add(V(step), V(step), V(pos));
        } else {
            lq_sq(step, pos);
            set = 0;
        }
        if (!walk(kStepRadius)) set = 0;
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x144))) != 0 && set != lbu(m + 0x53)) {
            anim_to(m, set, 0xa);
            sh(pv + 0x144, static_cast<uint32_t>(frame_scale_i(0xf)));
        }
    } else if (now == 5 || (now == 7 && lbu(m + 0x52) == 7 && 15.0f < frame)) {
        // resting in a box, or sawing: to its point at twice the frame time
        if (fpu_add(dt(), dt()) < vec2_distance(V(pos), V(goal))) {
            swc1(step, fpu_mul(cos_vu(heading_to(m, goal)), fpu_add(dt(), dt())));
            swc1(step + 8, 0.0f);
            swc1(step + 4, fpu_mul(sin_vu(heading_to(m, goal)), fpu_add(dt(), dt())));
            vec3_add(V(step), V(step), V(pos));
            walk(1.25f);
        }
    } else if (now != 6 && now != 8) {  // (standing: kept on the ground)
        lq_sq(step, pos);
        walker_step(m, pos, step, 0, 0.25f, kStepRadius, 0.5f, kStepSlope);
    }

    // ---- the fall (2 below to the floor); 5 below its home, dead
    swc1(pv + 0x78, fpu_add(lwc1(pv + 0x78), fpu_mul(dt_sq(), u2f(0x411ccccdu))));
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_sub(lwc1(pv + 0x78), 2.0f)));
    const float floor = floor_probe(0.5f, pos, 0);
    const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
    swc1(m + 0x18, z);
    if (z < floor) {
        swc1(m + 0x18, floor);
        sw(pv + 0x78, 0);
    }
    if (lwc1(m + 0x18) < fpu_sub(lwc1(pv + 0x128), 5.0f)) {
        saw_die_l4(m);
        moby_retire(m);
        ee_stack_free(0x1a0);
        return;
    }
    // ---- near the camera point, the saw's sparks (from its joint 0, white or red)
    if (vec2_distance(V(pos), V(L4_ADDR(0x00166f40))) < 20.0f) {
        const uint32_t at = sp + 0x80, vel = sp + 0x70;
        vec_clear(V(at));
        swc1(at, fpu_mul(rand_range_f(1.0f, -1.0f), fpu_mul(dt(), u2f(0x3dcccccdu))));
        swc1(at + 4, fpu_mul(rand_range_f(1.0f, -1.0f), fpu_mul(dt(), u2f(0x3dcccccdu))));
        swc1(at + 8, fpu_mul(rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au)), fpu_mul(dt(), -2.5f)));
        lq_sq(vel, at);
        const bool red = (rand_mod(100) & 1) != 0;
        moby_chain_point(m, 0, at);
        mat3_transform(V(vel), V(vel), M(m + 0xc0));
        const float size = rand_range_f(75000.0f, 100000.0f);
        const int32_t frames = frame_scale_i(red ? rand_range(10, 15) : rand_range(20, 30));
        particle_16_spawn(at, vel, red ? 0x400f0f7fu : 0x407f7f7fu, NOT_ADDR(0x00272727), frames, size);
    }
    color_pulse_update(m, pv + 0xb0);
    ee_stack_free(0x1a0);
}

// The saw's class hook +0x10: its carry record (pvars +0x180), 0 for no moby or no pvars.
// orig: L4:0x002c41b0
uint32_t eud_hook_10_l4(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x180 : 0;
}

}  // namespace rac1
