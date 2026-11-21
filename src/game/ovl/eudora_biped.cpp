// Eudora's class 217: a two-legged robot carrying a second moby (class 570, at its joint 0xd) that copies its
// animations. It stands at its place, rears up (animation 0xd) at a target within its sight +0x43c (doubled once
// alerted: pvars +0x38 or the timer +0x406), walks a path (+0x434), chases the target inside its area (the polygon of
// the path +0x41c) on the legged walker (eudora_walker.cpp, its record at pvars +0x1a0) and strikes it close up; hit, it
// recoils, is stunned (+0x404), or dies (knocked back, then explodes). Header eudora_mobys.h; registered in
// eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR, the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/eudora_walker.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position)
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
const float kPi = u2f(0x40490fdbu);
const float kTwoPi = u2f(0x40c90fdbu);
const float kFourPi = u2f(0x41490fdbu);

// the animation set `set` from `frame`, blended over `frames`, on the robot and its rider
void biped_anim_set(uint32_t m, uint32_t set, int32_t frame, int32_t frames) {
    const uint32_t pv = lwu(m + 0x78);
    moby_anim_set(m, set, frame, frames);
    if (const uint32_t rider = lwu(pv + 0x424); rider != 0) moby_anim_set(rider, set, frame, frames);
}
}  // namespace

// orig: L4:0x002ba3e0
void biped_anim_set_l4(uint32_t m, uint32_t set, int32_t frame, int32_t frames) { biped_anim_set(m, set, frame, frames); }

// The animation rate `rate` on the robot and its rider.
// orig: L4:0x002ba448
void biped_anim_rate_l4(uint32_t m, float rate) {
    swc1(m + 0x58, rate);
    if (const uint32_t rider = lwu(lwu(m + 0x78) + 0x424); rider != 0) swc1(rider + 0x58, rate);
}

// The robot's explosion at its joint 6.
// orig: L4:0x002ba470
void biped_explode_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t at = sp + 0x20, none = sp + 0x30;
    vec_clear(V(none));
    moby_chain_point(m, 6, at);
    explosion_effect(m, none, at, 0xa, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, u2f(0x47c35000u), 1.5f, 15.0f);
    ee_stack_free(0x70);
}

// pvars: health +0x20 (the hit state +0x20 .. +0x30), its home +0x3f0 (+0x3f8 the height it dies below 8 under),
// the colour pulse +0xb0, the knock-back's move record +0xc0, the head's turn +0x130 / +0x170 (its heading +0x428),
// the walker +0x1a0, the turn's seek +0x410 (the heading set +0x414), the lunge's speed +0x418, the fall speed +0x420,
// the rider +0x424, the push away from the hero +0x42c, the patrol's path +0x434 (its point +0x438, started +0x43a),
// the area's path +0x41c, the box +0x430 that wakes it, its sight +0x43c, the timers +0x402 (action), +0x404 (stunned),
// +0x406 (alerted), +0x407 (its cry), +0x448 (its twitch, +0x408 the twitch's angle). States: 0 set up, 1 stands, 2 rears
// up, 3 starts, 4 walks its path, 5 chases, 6 goes home, 7 strikes, 8 recoils (and lunges), 9 turns at its area's edge,
// 10 knocked back dying.
// orig: L4:0x002ba520
void eud_class217_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x1f0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t target = sp + 0x10, mark = sp + 0x80, step = sp + 0x90, joint = sp + 0xa0, chain = sp + 0xc0;
    const uint32_t hit_state = sp + 0x110, hit_dmg = sp + 0x114, hit_yaw = sp + 0x118;
    const uint32_t pos = m + 0x10, walker = pv + 0x1a0;
    const bool alerted = lw(pv + 0x38) != 0 || lbu(pv + 0x406) != 0;
    joint_node_scale(m, 0, pv + 0x120, 2.5f);
    auto near_camera = [&](float r, uint32_t band) {  // (its floor band near the camera point)
        if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L4_ADDR(0x00166f40))) < r) {
            moby_floor_band(m);
            sb(m + 0x7f, band);
        }
    };
    near_camera(27.0f, 0x15);
    const float sight = alerted ? fpu_add(lwc1(pv + 0x43c), lwc1(pv + 0x43c)) : lwc1(pv + 0x43c);
    target_select_any(m, target, sight);
    const uint32_t victim = lwu(target + 0x40);
    const float yaw0 = lwc1(m + 0x48);
    const float dist = vec2_distance(V(pos), V(target));
    const float to_target = atan2_f(fpu_sub(lwc1(target), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
    const float frame = moby_anim_time(m);
    const uint32_t rider0 = lwu(pv + 0x424);
    if (lw(L4_ADDR(0x0015f5c4)) == 2) {
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        if (rider0 != 0) sh(rider0 + 0x34, lhu(rider0 + 0x34) | 0x41u);
    } else if (lhu(m + 0x34) & 1) {
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        if (rider0 != 0) sh(rider0 + 0x34, lhu(rider0 + 0x34) & 0xffbeu);
    }
    lq_sq(sp + 0x60, pos);
    lq_sq(sp + 0x70, pos);
    lq_sq(mark, target);
    near_camera(32.0f, 0x1a);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x404)));

    // ---- the hit (the weapons' mask 0x210000)
    auto twitch = [&] {
        if (lw(pv + 0x448) <= 0) swc1(pv + 0x408, u2f(0xbf060a92u));
        sw(pv + 0x448, static_cast<uint32_t>(frame_scale_i(0x1e)));
    };
    auto knocked = [&](float push) {  // pushed away from the hero; stunned, alerted
        sb(pv + 0xb7, 0x78);
        swc1(pv + 0x42c, push);
        color_pulse_start(m, pv + 0xb0);
        sh(pv + 0x404, static_cast<uint32_t>(frame_scale_i(0x46)));
        sb(pv + 0x406, static_cast<uint32_t>(frame_scale_i(0xf0)));
    };
    auto recoil = [&] {  // (the recoil animation 5, twice as fast)
        sb(m + 0xbc, lbu(m + 0xbc) | 5u);
        biped_anim_set(m, 5, 0, frame_scale_i(6));
        biped_anim_rate_l4(m, 2.0f);
        sb(m + 0x20, 8);
    };
    const uint32_t state0 = lbu(m + 0x20);
    if (state0 != 0xa && state0 != 0) {
        swc1(hit_dmg, 0.0f);
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x210000), 0);
        int32_t reaction = moby_hit_react(m, hit, pv + 0x20, 0, hit_state, hit_dmg, 0, 4);
        if (hit != 0 && lwc1(hit_dmg) != 0.0f && lbu(m + 0x20) != 0) {
            const uint32_t src = lwu(hit + 0x20);
            const uint32_t from = (src == 0 || src == lwu(0x001403e0)) ? HERO + 0x80 : src + 0x10;
            swc1(hit_yaw, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(from)), fpu_sub(lwc1(m + 0x14), lwc1(from + 4))));
            sw(pv + 0xe4, 9);
            swc1(pv + 0xd4, fpu_mul(dt_sq(), 12.0f));
            sb(pv + 0xfd, 0);
            swc1(pv + 0xd0, fpu_mul(dt_sq(), 35.0f));
            swc1(pv + 0xd8, fpu_mul(dt(), 7.0f));
            const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
            swc1(pv + 0x20, hp);
            if (hp <= 0.0f) reaction = 1;
            if (lh(pv + 0x404) != 0 && reaction != 1) {
                twitch();  // stunned already: only twitches
                sb(pv + 0xb7, 0x78);
                color_pulse_start(m, pv + 0xb0);
            } else if (static_cast<uint32_t>(reaction) < 12) {
                switch (reaction) {  // (the table L4:0x1dbaa0)
                case 3: case 6: case 8: {  // (0x002ba904) recoils, unless struck hard by a moby mid-strike
                    moby_sound_start(0xb, 0, m);
                    twitch();
                    const bool by_moby = lh(lwu(hit + 0x20) + 0xa6) != 0;
                    if (!by_moby || 1.0f < lwc1(hit + 0x2c)) {
                        if (lbu(m + 0x52) != 4 || frame < 17.0f || 20.0f < frame) recoil();
                    }
                    knocked(fpu_mul(fpu_div(u2f(0x4059999au), frame_scale_f(7.0f)), 0.5f));
                    break;
                }
                case 7:  // (0x002baa38) recoils, knocked back
                    moby_sound_start(0xb, 0, m);
                    twitch();
                    sb(m + 0xbc, lbu(m + 0xbc) | 5u);
                    biped_anim_set(m, 5, 0, frame_scale_i(6));
                    biped_anim_rate_l4(m, 2.0f);
                    sb(m + 0x20, 8);
                    move_state_start(m, pv + 0xc0, 5, 1, 0, lwc1(hit_yaw));
                    swc1(pv + 0x114, 10.0f);
                    swc1(pv + 0x110, 5.0f);
                    knocked(fpu_div(u2f(0x4059999au), frame_scale_f(7.0f)));
                    break;
                case 4: case 5:  // (0x002baaf4) staggers (animation 6)
                    moby_sound_start(0xb, 0, m);
                    biped_anim_set(m, 6, 2, frame_scale_i(4));
                    sh(pv + 0x402, static_cast<uint32_t>(frame_scale_i(0x28)));
                    sb(m + 0x20, 1);
                    knocked(fpu_div(u2f(0x4059999au), frame_scale_f(7.0f)));
                    break;
                case 1: {  // (0x002bab88) killed: knocked back in an arc, its bolts dropped
                    swc1(pv + 0xe8, 1.0f);
                    sw(pv + 0xe0, 0x400);
                    jump_arc_set(pv + 0xc0, u2f(0x40b66666u), 4.0f);
                    lq_sq(step, hit + 0x10);
                    vec_yaw_marked(step, hit_yaw, pv + 0xd8, pv + 0xdc);
                    move_state_start(m, pv + 0xc0, 7, 1, 0, lwc1(hit_yaw));
                    swc1(pv + 0x110, 12.0f);
                    swc1(pv + 0x114, 25.0f);
                    sb(m + 0x20, 0xa);
                    sb(pv + 0xb7, 0x78);
                    color_pulse_start(m, pv + 0xb0);
                    moby_drop_bolts(m, 0, -1);
                    break;
                }
                default:  // 0, 2, 9, 10, 11 (0x002bac38)
                    break;
                }
            }
        }
    }
    sb(m + 0xa4, 0xff);
    if (lw(pv + 0x38) != 0) sb(pv + 0x406, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x38, 0);
    countdown_u8(pv + 0x406);
    if (lbu(m + 0x20) != 0 && lbu(pv + 0x2e) != 1) {  // (taken off the level)
        if (const uint32_t rider = lwu(pv + 0x424); rider != 0) moby_retire(rider);
        moby_retire(m);
        ee_stack_free(0x1f0);
        return;
    }
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x402)));

    // ---- the states
    // (the two strict comparisons of the original: they differ at the sight's edge)
    auto in_sight = [&] { return dist < (alerted ? fpu_add(lwc1(pv + 0x43c), lwc1(pv + 0x43c)) : lwc1(pv + 0x43c)); };
    auto out_of_sight = [&] { return (alerted ? fpu_add(lwc1(pv + 0x43c), lwc1(pv + 0x43c)) : lwc1(pv + 0x43c)) < dist; };
    auto anim_ended = [&] { return (lbu(m + 0x70) & 2) != 0; };
    // the target within reach: the strike (animation 4 from its frame 4, state 7)
    auto strike_if_near = [&] {
        if (!(dist < 4.5f)) return false;
        biped_anim_set(m, 4, 4, frame_scale_i(0xc));
        sb(m + 0x20, 7);
        return true;
    };
    // the target in sight (and not rearing yet): rearing up (animation 2, state 2)
    auto rear_if_seen = [&] {
        if (!in_sight() || (lbu(m + 0xbc) & 1) != 0) return;
        biped_anim_set(m, 2, 0, frame_scale_i(0xc));
        sb(m + 0x20, 2);
        sb(m + 0xbc, lbu(m + 0xbc) | 1u);
    };
    // the turn toward `heading` (the walker's turn rates)
    auto chase_turn = [&](float heading) {
        const float yaw = angle_seek_rate(pv + 0x410, lwc1(m + 0x48), heading, fpu_mul(dt_sq(), kTwoPi), fpu_mul(dt_sq(), u2f(0x42fb53d1u)),
                                          fpu_mul(dt(), kFourPi));
        swc1(pv + 0x414, yaw);
        walker_update_l4(m, walker, fpu_mul(dt(), 4.0f), yaw, fpu_mul(dt_sq(), 20.0f));
    };
    // the point two ahead outside the area: turning back (animation 8, state 9)
    auto area_check = [&](bool going_home) {
        if (lw(pv + 0x41c) == -1) return;
        swc1(step, fpu_add(cos_vu(lwc1(m + 0x48)), cos_vu(lwc1(m + 0x48))));
        swc1(step + 4, fpu_add(sin_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48))));
        sw(step + 8, 0);
        vec3_add(V(step), V(step), V(pos));
        uint32_t area = lvd::path(lw(pv + 0x41c));
        if (point_in_polygon(step, area + 0x10, lw(area)) == 0) {
            sw(pv + 0x410, 0);
            biped_anim_set(m, 8, 0, frame_scale_i(0xc));
            sb(m + 0x20, 9);
            if (going_home) sb(m + 0xbc, lbu(m + 0xbc) | 0x10u);
        }
        area = lvd::path(lw(pv + 0x41c));
        const bool target_inside = point_in_polygon(mark, area + 0x10, lw(area)) != 0;
        if (going_home ? target_inside : !target_inside) sb(m + 0x20, going_home ? 1 : 6);
    };
    // a turn of the head at the lunge's rates
    auto lunge_turn = [&](float heading) {
        swc1(m + 0x48, angle_seek_rate(pv + 0x410, lwc1(m + 0x48), heading, fpu_mul(dt_sq(), kPi), fpu_mul(dt_sq(), kTwoPi),
                                       fpu_mul(dt(), u2f(0x40860a92u))));
    };
    auto to_home = [&] { return atan2_f(fpu_sub(lwc1(pv + 0x3f0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x3f4), lwc1(m + 0x14))); };
    const uint32_t state = lbu(m + 0x20);
    switch (state < 11 ? state : 11) {  // (the table L4:0x1dbad0)
    case 0:  // (0x002bacac) set up
        swc1(pv + 0x20, 3.0f);
        sb(pv + 0x28, 2);
        swc1(pv + 0x30, 1.5f);
        sh(pv + 0x24, 3);
        sb(pv + 0x2e, 1);
        sb(pv + 0x29, 0);
        sb(m + 0xbc, 0);
        sw(pv + 0x420, 0);
        sw(pv + 0x424, 0);
        sb(pv + 0x407, 0);
        lq_sq(pv + 0x3f0, pos);
        if (lbu(pv + 0x121) == 0) sub_0024f590(m, 0, pv + 0x120);
        sub_0024f590(m, 0xc, pv + 0x160);
        swc1(pv + 0x428, lwc1(m + 0x48));
        sb(pv + 0x58, 0xa);
        sb(pv + 0x5a, 0xc);
        if (lbu(pv + 0x400) != 0) {
            sb(m + 0x20, 3);
            biped_anim_set(m, 0xa, 0, frame_scale_i(0xc));
        } else {
            sb(m + 0x20, 1);
        }
        break;
    case 1: {  // (0x002bad68) standing: idles; rears at a target; woken by its box
        if (lbu(m + 0x53) == 6 && lh(pv + 0x402) == frame_scale_i(0x25)) moby_sound_start(2, 0, m);
        bool turn_up = true;  // (the alert animation may start)
        if (anim_ended() && out_of_sight()) {
            // an idle: 0 five times in six, else 1
            libc_memset(sp + 0xa0, 0, 0x18);
            sw(sp + 0xb4, 1);
            const uint32_t pick = sp + 0xa0 + (static_cast<uint32_t>(rand_mod(6)) << 2);
            biped_anim_set(m, lwu(pick), 0, frame_scale_i(0x18));
            turn_up = false;
        }
        if (turn_up && lbu(m + 0x53) != 0xd && (lbu(m + 0x53) != 6 || anim_ended()) && in_sight())
            biped_anim_set(m, 0xd, 0, frame_scale_i(0x18));
        const int32_t box = lw(pv + 0x430);
        if (box != -1 && lw(pv + 0x434) != -1 && lh(pv + 0x43a) == 0 && (alerted || point_in_box(HERO + 0x80, box) != 0)) {
            sb(m + 0x20, 4);  // off along its path
            sb(m + 0xbc, lbu(m + 0xbc) | 1u);
            sh(pv + 0x43a, 1);
            break;
        }
        if (lbu(m + 0x52) == 0xd && countdown_u8(pv + 0x407) != 0) {
            moby_sound_start(0xa, 0, m);
            sb(pv + 0x407, static_cast<uint32_t>(frame_scale_i(0xb4)));
        }
        if (lh(pv + 0x402) != 0) break;
        if (strike_if_near()) break;
        rear_if_seen();
        break;
    }
    case 2:  // (0x002baf94) rearing: then standing, alert (0xd) while the target is in sight
        if (lbu(m + 0x52) != 2 || !anim_ended()) break;
        biped_anim_set(m, in_sight() ? 0xd : 0, 0, frame_scale_i(0xc));
        sb(m + 0x20, 1);
        break;
    case 3:  // (0x002bafc4) starting (animation 0xb once its first ends)
        if (lbu(m + 0x52) != 0 && anim_ended()) biped_anim_set(m, 0xb, 0, frame_scale_i(0xc));
        if (lh(pv + 0x402) != 0) break;
        if (strike_if_near()) break;
        rear_if_seen();
        break;
    case 4: {  // (0x002bb0c4) walking its path (animation 3); at its end standing again
        const uint32_t path = lvd::path(lw(pv + 0x434));
        if (lbu(m + 0x53) != 3) biped_anim_set(m, 3, 0, frame_scale_i(0xc));
        strike_if_near();
        auto point = [&] { return path + 0x10 + (static_cast<uint32_t>(lh(pv + 0x438)) << 4); };
        if (vec2_distance(V(point()), V(pos)) < 1.0f) {
            const uint32_t next = (lhu(pv + 0x438) + 1) & 0xffffu;
            sh(pv + 0x438, next);
            if (static_cast<int16_t>(next) == lw(path)) {
                biped_anim_set(m, out_of_sight() ? 0 : 0xd, 0, frame_scale_i(0x18));
                sb(m + 0x20, 1);
                break;
            }
        }
        const uint32_t p = point();
        const float heading = atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14)));
        swc1(m + 0x48, angle_seek_rate(pv + 0x410, lwc1(m + 0x48), heading, fpu_mul(dt_sq(), kFourPi), fpu_mul(dt_sq(), kTwoPi),
                                       fpu_mul(dt(), kPi)));
        swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(dt(), 6.0f)));
        sw(step + 8, 0);
        swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_mul(dt(), 6.0f)));
        vec3_add(V(step), V(step), V(pos));
        walker_step(m, pos, step, 1, 0.25f, 1.5f, 0.75f, u2f(0x3f5f66f3u));
        break;
    }
    case 5:  // (0x002bb318) chasing the target on the walker; out of its area: back home
        if (lbu(m + 0xbc) & 8) {
            sb(m + 0xbc, lbu(m + 0xbc) & 0xf7u);
            walker_init_l4(m, walker);
        }
        chase_turn(to_target);
        if (strike_if_near()) break;
        area_check(false);
        break;
    case 6:  // (0x002bb4d8) going home; the target back in its area: standing
        if (strike_if_near()) break;
        if (lbu(m + 0xbc) & 8) {
            sb(m + 0xbc, lbu(m + 0xbc) & 0xf7u);
            walker_init_l4(m, walker);
        }
        chase_turn(to_home());
        if (vec2_distance(V(pos), V(pv + 0x3f0)) < 2.0f) sb(m + 0x20, 1);
        area_check(true);
        break;
    case 7: {  // (0x002bb6e8) striking: the hit at its frames 17 .. 20 within reach; then back to standing
        if (lbu(m + 0x52) == lbu(m + 0x53) && 17.0f <= moby_anim_time(m) && moby_anim_time(m) < 20.0f &&
            abs_f(fpu_sub(lwc1(m + 0x18), lwc1(target + 8))) < 2.0f && vec2_distance(V(pos), V(target)) < 4.5f) {
            const float k = u2f(0x3e4ccccdu);
            swc1(step, fpu_mul(cos_vu(to_target), k));
            swc1(step + 4, fpu_mul(sin_vu(to_target), k));
            sw(step + 8, 0);
            lq_sq(joint, target);
            swc1(joint + 8, fpu_add(lwc1(joint + 8), 0.75f));
            if (victim != 0) sub_00259bc8(victim, m, 1, joint, step, 1.0f);
        }
        if (20.0f < frame) {  // (the strike's rate eased back to 1)
            float rate = lwc1(m + 0x58);
            if (1.0f < rate) {
                rate = fpu_sub(rate, fpu_mul(dt(), 3.0f));
                if (rate < 1.0f) rate = 1.0f;
                biped_anim_rate_l4(m, rate);
            }
        }
        if (lbu(m + 0x52) == 4 && anim_ended()) {
            sh(pv + 0x402, static_cast<uint32_t>(frame_scale_i(0x5a)));
            biped_anim_set(m, in_sight() ? 0xd : 0, 0, frame_scale_i(0xc));
            sb(m + 0x20, 1);
        }
        break;
    }
    case 8: {  // (0x002bb904) recoiling (animation 5): then striking again (asked: +0xbc bit 4), or lunging (animation 8)
        const uint32_t set = lbu(m + 0x53);
        if (set == 5 && (lbu(m + 0x52) != 5 || frame <= 13.0f)) break;
        if (lbu(m + 0xbc) & 4) {
            sb(m + 0xbc, lbu(m + 0xbc) & 0xfbu);
            biped_anim_set(m, 4, 4, frame_scale_i(6));
            biped_anim_rate_l4(m, 1.5f);
            sb(m + 0x20, 7);
            break;
        }
        // the lunge speeding up (to a ninth of the frame's turn time) while its animation 8 plays its first 28 frames
        // (or blends in); else slowing, and once stopped standing
        float speed = lwc1(pv + 0x418);
        if (set == 8 && (frame < 28.0f || lbu(m + 0x52) != 8)) {
            const float most = fpu_mul(lwc1(0x0015ed60), u2f(0x3de38e39u));
            speed = speed < most ? fpu_add(speed, fpu_mul(dt_sq(), 20.0f)) : most;
        } else {
            if (!(0.0f < speed)) {
                sb(m + 0x20, 1);
                sh(pv + 0x402, 0);
                break;
            }
            speed = fpu_sub(speed, fpu_mul(dt_sq(), 200.0f));
        }
        swc1(pv + 0x418, speed);
        vec_from_spherical_v(V(step), lwc1(pv + 0x418), lwc1(pv + 0x414), lwc1(m + 0x44));
        vec3_add(V(pos), V(pos), V(step));
        lunge_turn(to_target);
        break;
    }
    case 9:  // (0x002bba88) at the area's edge (animation 8): then chasing (back home with +0xbc bit 0x10)
        if (lbu(m + 0x52) == 8 && anim_ended()) {
            sb(m + 0x20, 5);
            sb(m + 0xbc, (lbu(m + 0xbc) | 8u) & 0xefu);
            biped_anim_set(m, 9, 0, frame_scale_i(0xc));
            break;
        }
        lunge_turn((lbu(m + 0xbc) & 0x10) != 0 ? to_home() : to_target);
        break;
    case 10: {  // (0x002bbbb0) knocked back dying: on the ground (or under 1), exploding into its three pieces
        const uint32_t flags = enemy_physics(m, pv + 0xc0);
        if ((flags & 0x40) == 0 && !(lwc1(m + 0x18) < 1.0f)) break;
        biped_explode_l4(m);
        if (const uint32_t rider = lwu(pv + 0x424); rider != 0) moby_retire(rider);
        moby_drop_bolts(m, 0, -1);
        const uint32_t z = L4_ADDR(0x0015f580);
        for (const uint32_t cls : {0x798u, 0x799u, 0x79au}) debris_spawn(m, cls, pos, m + 0x40, 0, 0, z, z, z, 0.0f);
        moby_retire(m);
        ee_stack_free(0x1f0);
        return;
    }
    default:
        break;
    }

    // ---- every frame: the colour pulse, the push away from the hero, the twitch, the fall, the head, the rider
    color_pulse_update(m, pv + 0xb0);
    if (0.0f < lwc1(pv + 0x42c)) {
        const float away = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84)));
        swc1(step, fpu_mul(cos_vu(away), lwc1(pv + 0x42c)));
        swc1(step + 8, 0.0f);
        swc1(step + 4, fpu_mul(sin_vu(away), lwc1(pv + 0x42c)));
        vec3_add(V(step), V(step), V(pos));
        if (lwc1(pv + 0x420) == 0.0f) {
            const float t = fpu_mul(frame_scale_f(7.0f), frame_scale_f(7.0f));
            swc1(pv + 0x42c, fpu_add(lwc1(pv + 0x42c), fpu_div(u2f(0xc059999au), t)));
        }
        walker_step(m, pos, step, 1, 0.25f, 1.5f, 0.75f, u2f(0x3f5f66f3u));
    }
    swc1(pv + 0x408, fpu_mul(lwc1(pv + 0x408), u2f(0x3f75c28fu)));
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x448)));
    quat_from_axis_angle(V(step), 1, lwc1(pv + 0x408));
    quat_from_axis_angle(V(pv + 0x130), 2, 0.0f);
    quat_mul(V(pv + 0x130), V(pv + 0x130), V(step));
    if (lbu(m + 0x20) != 8) sb(pv + 0x401, lbu(m + 0x20));
    // the fall (2 below to the floor)
    swc1(pv + 0x420, fpu_add(lwc1(pv + 0x420), fpu_mul(dt_sq(), u2f(0x41ee6666u))));
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_sub(lwc1(pv + 0x420), 2.0f)));
    const float floor = floor_probe(0.5f, pos, 0);
    const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
    swc1(m + 0x18, z);
    if (z < floor) {
        swc1(m + 0x18, floor);
        swc1(pv + 0x420, 0.0f);
    }
    if (lwc1(m + 0x18) < fpu_sub(lwc1(pv + 0x3f8), 8.0f)) {  // fallen off the level
        biped_explode_l4(m);
        if (const uint32_t rider = lwu(pv + 0x424); rider != 0) moby_retire(rider);
        moby_retire(m);
        ee_stack_free(0x1f0);
        return;
    }
    if (lbu(m + 0x20) != 0) {  // the head toward the target (its joint 0xc), the body's turn taken out
        swc1(pv + 0x428, angle_add(lwc1(pv + 0x428), angle_sub(lwc1(m + 0x48), yaw0)));
        moby_chain_world_one(m, 0xc, chain);
        mat_to_euler(chain, sp + 0x100);
        quat_from_axis_angle(V(joint), 2, angle_sub(to_target, lwc1(pv + 0x428)));
        swc1(pv + 0x428, to_target);
        quat_mul(V(pv + 0x170), V(pv + 0x170), V(joint));
    }
    if (const uint32_t rider = lwu(pv + 0x424); rider != 0) {  // the rider at the joint 0xd
        moby_chain_world_one(m, 0xd, joint);
        lq_sq(rider + 0x10, joint + 0x30);
        moby_anim_advance(rider);
        moby_update_transform(rider);
        mat3_copy(M(rider + 0xc0), M(joint));
        mat3_normalize_columns(rider + 0xc0);
        moby_update_transform_kept(rider);
        sh(rider + 0x34, lhu(rider + 0x34) | 6u);
    } else {  // (spawned at the joint 0xb: class 570)
        moby_chain_point(m, 0xb, joint);
        sw(pv + 0x424, spawn_like(m, joint, 0x23a));
    }
    ee_stack_free(0x1f0);
}

}  // namespace rac1
