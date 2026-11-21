// Eudora's class 563, the Woodcut Bot: a big two-legged logging robot walking on the legged walker (eudora_walker.cpp,
// its record at pvars +0x160, its gaits those of its animations 10 and 3). It stands at its place or cuts wood (throwing
// chips), notices a target within its sight +0x3e8 (doubled once alerted: pvars +0x38 or the timer +0x3c6), turns round
// to it, chases it inside its area (the polygon of the path +0x3dc) and swings at it close up; outside its area it stops,
// or goes home. Hit, it recoils and lunges away from the hero within its area, staggers, or dies (knocked back, then
// explodes). Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only data
// is L4_ADDR, the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/eudora_walker.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position)
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
float turn_dt() { return lwc1(0x0015ed60); }  // (the time scale of the turns)
const float kPi = u2f(0x40490fdbu);
const float kTwoPi = u2f(0x40c90fdbu);
const float kHalfPi = u2f(0x3fc90fdbu);
}  // namespace

// Eudora's own copy of the debris chunk's spawn (chunk_spawn, Veldin 0x2af8b0, whose engine copy is L4:0x2a2260): the
// same code, but its spin range is the level's own pair L4:0x161d78 / 0x161d7c (gp - 0x4e88), not the engine's
// (Veldin 0x161448, L4:0x161418) the shared port reads. Used by the wood chips of the bot's state 3.
// orig: L4:0x002e4fe0
uint32_t eud_chunk_spawn_l4(uint32_t src, uint32_t vel, uint32_t pos, uint32_t rot, uint32_t cls, uint32_t tag, float lo, float hi) {
    const uint32_t m = moby_spawn(cls);
    if (m == 0) return 0;
    const uint32_t p = lwu(m + 0x78);
    sw(p, src);
    sh(m + 0x32, 0x7e);
    sb(m + 0x31, 1);
    sb(m + 0x30, 0xff);
    sb(m + 0x20, 0);
    sd(m + 0x38, ld(src + 0x38));
    swc1(m + 0x40, rand_angle());
    swc1(m + 0x44, rand_angle());
    swc1(m + 0x48, rand_angle());
    V(m + 0x10) = V(pos);
    V(m + 0x40) = V(rot);
    V(p + 0x10) = V(vel);
    const float deg = u2f(0x3c8efa35u);
    swc1(p + 4, lwc1(m + 0x18));
    const int32_t t = frame_scale_i(0x14);
    sw(p + 0x20, 0);
    sh(p + 0xe, static_cast<uint32_t>(t));
    swc1(p + 0x24, (rand_range_signed_f(lwc1(L4_ADDR(0x00161d78)), lwc1(L4_ADDR(0x00161d7c))) * deg) * lwc1(0x0015ed6c));
    swc1(p + 0x28, (rand_range_signed_f(lwc1(L4_ADDR(0x00161d78)), lwc1(L4_ADDR(0x00161d7c))) * deg) * lwc1(0x0015ed6c));
    const float k = rand_range_f(lo, hi);
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), k));  // (the PS2 FPU's product: a missing class's scale clamps)
    sh(p + 0xc, tag);
    swc1(p + 8, k * 0.25f);
    moby_update_transform(m);
    return m;
}

// The legs' bend limits: the first leg -pi/16 .. pi/2, the second -pi/2 .. pi/16.
// orig: L4:0x00292370
void woodcut_leg_limits_l4(uint32_t rec) {
    swc1(rec + 0x10c, u2f(0xbe490fdbu));
    swc1(rec + 0x110, kHalfPi);
    swc1(rec + 0x1bc, u2f(0xbfc90fdbu));
    swc1(rec + 0x1c0, u2f(0x3e490fdbu));
}

// The walker set up on its gaits: each gait's first frame (+0xc), length (+0x10), feet's lift and plant spans (+0x38 /
// +0x3c, +0x20 / +0x24) and speed (+8: the stride +4 over twice the length); the six joints' places, the legs' headings
// and their joints' own rotations; the walker standing, its feet down (5).
// orig: L4:0x002923b8
void walker_setup_l4(uint32_t m, uint32_t rec) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t list = sp, joint = sp + 0x20;
    for (uint32_t slot = 0; slot < 13; ++slot) {
        const uint32_t at = rec + 0x70 + slot * 4;
        if (lw(lwu(at)) < 0) {  // a gait with no animation: the debug print (the slot, the class) and the debug trap
            debug_printf_stub();
            debug_trap();
        }
        const uint32_t g = lwu(at);
        swc1(g + 0xc, class_anim_first_time(lbu(m + 0x22), lwu(g)));
        swc1(g + 0x10, fpu_sub(class_anim_last_time(lbu(m + 0x22), lwu(g)), lwc1(g + 0xc)));
        if (lwc1(g + 0x10) == 0.0f) continue;
        // the frames from `from` to `to` (wrapped past the loop's end) into `out`, for each foot
        auto span = [g](uint32_t from, uint32_t to, uint32_t out) {
            const float a = lwc1(g + from), b = lwc1(g + to);
            swc1(g + out, a <= b ? fpu_sub(b, a) : fpu_sub(fpu_add(b, lwc1(g + 0x10)), a));
        };
        for (uint32_t foot = 0; foot < 8; foot += 4) {
            span(0x40 + foot, 0x48 + foot, 0x38 + foot);
            span(0x28 + foot, 0x30 + foot, 0x20 + foot);
        }
        swc1(g + 8, fpu_div(lwc1(g + 4), frame_scale_f(fpu_add(lwc1(g + 0x10), lwc1(g + 0x10)))));
    }
    sw(list + 0x00, lbu(rec + 0xf0));
    sw(list + 0x04, lbu(rec + 0xf1));
    sw(list + 0x08, lbu(rec + 0x1a0));
    sw(list + 0x0c, lbu(rec + 0x1a1));
    sw(list + 0x10, lbu(rec + 0xb4));
    sw(list + 0x14, lbu(rec + 0xb5));
    moby_joint_positions(m, 6, list, rec);
    for (uint32_t leg = rec; leg < rec + 0x160; leg += 0xb0) {
        moby_chain_world_one(m, lbu(leg + 0xf0), joint);
        sw(leg + 0x104, 0);
        sw(leg + 0xfc, 0);
        swc1(leg + 0xf8, atan2_f(lwc1(joint + 0x10), lwc1(joint + 0x14)));
        if (lbu(leg + 0x161) == 0) sub_0024f590(m, lbu(leg + 0xf3), leg + 0x160);
        if (lbu(leg + 0x121) == 0) sub_0024f590(m, lbu(leg + 0xf2), leg + 0x120);
    }
    sh(rec + 0xee, 0);
    sb(rec + 0xb7, 0);
    sw(rec + 0xb8, 0);
    sw(rec + 0xbc, 0);
    sw(rec + 0xc0, 0);
    sw(rec + 0xc8, 0);
    sw(rec + 0xe8, 0);
    sw(rec + 0xd8, 0);
    sw(rec + 0xe4, 0);
    sh(rec + 0xec, 0);
    sb(rec + 0xb6, 5);
    debug_printf_stub();  // (the record's size, 0x250)
    ee_stack_free(0xf0);
}

// The Woodcut Bot's walker: every gait that of its animation 10 but the run's (gait 7), its animation 3's; the turn
// rates, the gravity, the greatest change of speed a frame, the step's height, the idle and landing animations (0, 16)
// and the joints of its legs.
// orig: L4:0x002d1418
void woodcut_walker_init_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x40);
    (void)sp;
    const uint32_t pv = lwu(m + 0x78), rec = pv + 0x160;
    for (uint32_t slot = 0; slot < 13; ++slot) {  // (the animation's gait: its sequence's +0x14)
        const uint32_t set = slot == 7 ? 3 : 0xa;
        sw(rec + 0x70 + slot * 4, lwu(lwu(lwu(m + 0x24) + 0x48 + set * 4) + 0x14));
    }
    woodcut_leg_limits_l4(rec);
    swc1(rec + 0xdc, fpu_mul(dt_sq(), u2f(0x411ccccdu)));  // the gravity (9.8)
    swc1(rec + 0xcc, fpu_mul(dt_sq(), kPi));
    swc1(rec + 0xd0, fpu_mul(dt_sq(), kTwoPi));
    swc1(rec + 0xd4, fpu_mul(dt(), kTwoPi));
    swc1(rec + 0xc4, fpu_mul(dt_sq(), 10.0f));
    swc1(rec + 0xe0, 0.5f);
    sb(rec + 0xb0, 0);
    sb(rec + 0xb1, 0);
    sb(rec + 0xb2, 0xff);
    sb(rec + 0xb3, 0x10);
    sb(rec + 0xb4, 7);
    sb(rec + 0xb5, 7);
    sb(rec + 0xf0, 0xa);  // the first leg's joints
    sb(rec + 0xf1, 8);
    sb(rec + 0xf2, 0xe);
    sb(rec + 0xf3, 0xc);
    sb(rec + 0x1a0, 0xb);  // the second's
    sb(rec + 0x1a1, 9);
    sb(rec + 0x1a2, 0xf);
    sb(rec + 0x1a3, 0xd);
    walker_setup_l4(m, rec);
    ee_stack_free(0x40);
}

// The Woodcut Bot's explosion at its joint 0xc.
// orig: L4:0x002d1608
void woodcut_explode_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t at = sp + 0x20, none = sp + 0x30;
    vec_clear(V(none));
    moby_chain_point(m, 0xc, at);
    explosion_effect(m, none, at, 0xa, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, u2f(0x47c35000u), 1.0f, 15.0f);
    ee_stack_free(0x70);
}

// pvars: health +0x20 (the hit state +0x20 .. +0x30), the colour pulse +0xb0, the knock-back's move record +0xc0, the
// joint 6's node +0x120 (its rotation +0x130: the twitch), the walker +0x160, its home +0x3b0, cutting from the start
// +0x3c0, the last state +0x3c1, the timers +0x3c2 (action), +0x3c4 (stunned), +0x3c6 (alerted), the twitch's angle +0x3c8,
// the turn's seek +0x3d0, the lunge's heading +0x3d4 and speed +0x3d8, the area's path +0x3dc, the fall speed +0x3e0, the
// push away from the hero +0x3e4, its sight +0x3e8. States: 0 set up, 1 stands, 2 (an end of a rearing: no code here sets
// it), 3 cuts wood, 4 waits alerted, 5 chases, 6 goes home, 7 stops, 8 swings, 9 recoils (then lunges away), 10 turns
// round, 11 knocked back dying.
// orig: L4:0x002d16b8
void eud_class563_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x240);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t target = sp + 0x10, step = sp + 0x80;
    const uint32_t hit_state = sp + 0x140, hit_dmg = sp + 0x144, hit_yaw = sp + 0x148, hit_scale = sp + 0x14c;
    const uint32_t pos = m + 0x10, rec = pv + 0x160;
    const bool alerted = lw(pv + 0x38) != 0 || lh(pv + 0x3c6) != 0;
    const float sight = alerted ? fpu_add(lwc1(pv + 0x3e8), lwc1(pv + 0x3e8)) : lwc1(pv + 0x3e8);
    target_select_any(m, target, sight);
    joint_node_scale(m, 6, pv + 0x120, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L4_ADDR(0x00166f40))) < 27.0f) {  // (its floor band near the camera point)
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    const float to_target = atan2_f(fpu_sub(lwc1(target), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14)));
    const float facing = angle_dist(lwc1(m + 0x48), to_target);  // (how far it faces off the target)
    const float dist = vec2_distance(V(pos), V(target));
    const float frame = moby_anim_time(m);
    lq_sq(sp + 0x60, pos);
    lq_sq(sp + 0x70, pos);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x3c4)));

    // ---- the hit (the weapons' mask 0xa10000)
    const uint32_t state0 = lbu(m + 0x20);
    if (state0 != 0xb && state0 != 0) {
        swc1(hit_dmg, 0.0f);
        const uint32_t hit = sub_0025a420(m, 0xa10000u, 0);
        int32_t reaction = moby_hit_react(m, hit, pv + 0x20, 0, hit_state, hit_dmg, 0, 4);
        sw(pv + 0xe0, 0x15e);
        swc1(pv + 0xe8, u2f(0x3eb33333u));
        if (hit != 0 && lwc1(hit_dmg) != 0.0f && lbu(m + 0x20) != 0 && lh(pv + 0x3c4) == 0) {
            swc1(hit_scale, 1.0f);
            const uint32_t src = lwu(hit + 0x20);
            const uint32_t from = (src == 0 || src == lwu(0x001403e0)) ? HERO + 0x80 : src + 0x10;
            swc1(hit_yaw, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(from)), fpu_sub(lwc1(m + 0x14), lwc1(from + 4))));
            sw(pv + 0xe4, 9);
            swc1(pv + 0xd0, fpu_mul(dt_sq(), 30.0f));
            sb(pv + 0xfd, 0);
            swc1(pv + 0xd4, fpu_mul(dt_sq(), 10.0f));
            swc1(pv + 0xd8, fpu_mul(dt(), 5.5f));
            const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
            swc1(pv + 0x20, hp);
            if (hp <= 0.0f) reaction = 1;
            // knocked: pushed away from the hero by `push` (+0x3e4), its yaw marked; flashing; stunned for `stun` frames
            auto knocked = [&](float push) {
                swc1(pv + 0x3e4, push);
                lq_sq(step, hit + 0x10);
                vec_yaw_marked(step, hit_yaw, pv + 0x3e4, hit_scale);
            };
            auto flash = [&] {
                sb(pv + 0xb7, 0x78);
                color_pulse_start(m, pv + 0xb0);
            };
            if (static_cast<uint32_t>(reaction) < 12) {
                switch (reaction) {  // (the table L4:0x1dc480)
                case 1: case 2: {  // (0x002d19dc) killed: knocked back in an arc, its bolts dropped
                    swc1(pv + 0xd8, fpu_mul(dt(), 7.0f));
                    swc1(pv + 0xdc, fpu_mul(dt(), 13.0f));
                    swc1(pv + 0xd4, fpu_mul(dt_sq(), 17.0f));
                    lq_sq(step, hit + 0x10);
                    vec_yaw_marked(step, hit_yaw, pv + 0xd8, pv + 0xdc);
                    move_state_start(m, pv + 0xc0, 8, 1, 0, lwc1(hit_yaw));
                    swc1(pv + 0x114, 24.0f);
                    swc1(pv + 0x110, 14.0f);
                    sb(m + 0x20, 0xb);
                    moby_drop_bolts(m, 0, -1);
                    flash();
                    const float push = fpu_div(2.0f, frame_scale_f(15.0f));
                    swc1(pv + 0x3e4, fpu_add(push, push));
                    break;
                }
                case 3: case 6: {  // (0x002d1ac4) twitches; recoils (animation 5), unless in its swing's frames 16 .. 27
                    swc1(pv + 0x3c8, u2f(0xbf060a92u));
                    if (lbu(m + 0x52) == 4 && !(frame < 16.0f) && !(27.0f < frame)) break;
                    sb(m + 0xbc, lbu(m + 0xbc) | 5u);
                    moby_anim_set(m, 5, 0, frame_scale_i(3));
                    walker_legs_reset(rec);
                    sb(m + 0x20, 9);
                    const int32_t by = lh(lwu(hit + 0x20) + 0xa6);  // (the class of what hit it: two push it half as far)
                    float push = fpu_div(2.0f, frame_scale_f(15.0f));
                    if (by == 0x131 || by == 0xb0) push = fpu_mul(push, 0.5f);
                    knocked(push);
                    flash();
                    sh(pv + 0x3c4, static_cast<uint32_t>(frame_scale_i(0xf)));
                    sh(pv + 0x3c6, static_cast<uint32_t>(frame_scale_i(0xf0)));
                    break;
                }
                case 4: case 5:  // (0x002d1c00) staggers (animation 6), turned to face where the hit came from
                    moby_anim_set(m, 6, 0, frame_scale_i(0xa));
                    walker_legs_reset(rec);
                    sb(m + 0x20, 1);
                    knocked(fpu_div(2.0f, frame_scale_f(15.0f)));
                    swc1(m + 0x48, angle_add(kPi, lwc1(hit_yaw)));
                    flash();
                    sh(pv + 0x3c4, static_cast<uint32_t>(frame_scale_i(0x1e)));
                    sh(pv + 0x3c2, static_cast<uint32_t>(frame_scale_i(0xb4)));
                    sh(pv + 0x3c6, static_cast<uint32_t>(frame_scale_i(0xf0)));
                    break;
                default:  // 0, 7 .. 11 (0x002d1cb4)
                    break;
                }
            }
        }
    }
    sb(m + 0xa4, 0xff);
    if (lw(pv + 0x38) != 0) sh(pv + 0x3c6, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x38, 0);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x3c6)));

    // ---- the swing (animation 4, its frames 16 .. 27): the segments between its six arm joints (the list L4:0x1dc450),
    // a sphere between the first and the fifth, and one at its joint 0x11, hit what they meet; the hit record pushes the
    // hero away (+0 the push, +0x10 the moby, +0x14 its flags, +0x1c the damage, +0x20 one)
    if (lbu(m + 0x20) == 8 && lbu(m + 0x52) == 4 && 16.0f < frame && frame < 27.0f) {
        const uint32_t list = sp + 0x90, joints = sp + 0xb0, blow = sp + 0x110, mid = sp + 0x80;
        for (uint32_t i = 0; i < 6; ++i) sw(list + i * 4, lwu(L4_ADDR(0x001dc450) + i * 4));
        sw(blow + 0x14, 0x10001);
        swc1(blow + 0x1c, 1.0f);
        sw(blow + 0x10, m);
        vec3_sub(V(blow), V(HERO + 0x80), V(pos));
        vec3_set_length(V(blow), V(blow), fpu_mul(dt(), 5.0f));
        sw(blow + 0x20, 1);
        moby_joint_positions(m, 6, list, joints);
        for (uint32_t i = 0; i < 5; ++i) segment_collide(joints + i * 0x10, joints + (i + 1) * 0x10, 9, m, blow);
        vec3_add(V(mid), V(joints), V(joints + 0x40));
        vec3_scale(V(mid), V(mid), 0.5f);
        mobys_in_sphere(mid, fpu_mul(vec3_distance(V(joints), V(joints + 0x40)), 0.5f), 0x10, m, blow);
        moby_chain_point(m, 0x11, mid);
        mobys_in_sphere(mid, 0.5f, 0x10, m, blow);
    }
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x3c2)));

    // ---- the states
    auto anim_ended = [&] { return (lbu(m + 0x70) & 2) != 0; };
    auto swing_near = [&] { return dist < 3.0f && facing < kHalfPi; };  // the target within reach, ahead
    auto swing = [&](bool legs) {  // the swing (animation 4, state 8)
        moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        if (legs) walker_legs_reset(rec);
        sb(m + 0x20, 8);
    };
    auto stand = [&] {  // (animation 0, state 1)
        moby_anim_set(m, 0, 0, frame_scale_i(0xa));
        sb(m + 0x20, 1);
    };
    auto has_area = [&] { return lw(pv + 0x3dc) != -1; };
    auto in_area = [&](uint32_t p) {
        const uint32_t area = lvd::path(lw(pv + 0x3dc));
        return point_in_polygon(p, area + 0x10, lw(area)) != 0;
    };
    auto ahead_in_area = [&] {  // the point two ahead
        swc1(step, fpu_add(cos_vu(lwc1(m + 0x48)), cos_vu(lwc1(m + 0x48))));
        swc1(step + 4, fpu_add(sin_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48))));
        sw(step + 8, 0);
        vec3_add(V(step), V(step), V(pos));
        return in_area(step);
    };
    auto chase = [&](uint32_t next) {  // chasing (5) or going home (6) on the walker, set up anew
        sb(m + 0x20, next);
        sb(m + 0xbc, lbu(m + 0xbc) | 8u);
    };
    auto turn_round = [&] {  // turning round (animation 16, state 10)
        sw(pv + 0x3d0, 0);
        moby_anim_set(m, 0x10, 0, frame_scale_i(0xa));
        walker_legs_reset(rec);
        sb(m + 0x20, 0xa);
    };
    // the target in sight: the first time turning round to it, then chasing it (when alerted or it is in the area)
    auto notice = [&] {
        if ((lbu(m + 0xbc) & 1) == 0) {
            turn_round();
            sb(m + 0xbc, lbu(m + 0xbc) | 1u);
        } else if (alerted || !has_area() || in_area(target)) {
            chase(5);
        }
    };
    auto walk = [&](float heading) { walker_update_l4(m, rec, fpu_mul(dt(), 4.0f), heading, fpu_mul(dt_sq(), 20.0f)); };
    auto to_home = [&] { return atan2_f(fpu_sub(lwc1(pv + 0x3b0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x3b4), lwc1(m + 0x14))); };
    auto to_target_now = [&] { return atan2_f(fpu_sub(lwc1(target), lwc1(m + 0x10)), fpu_sub(lwc1(target + 4), lwc1(m + 0x14))); };
    auto turn_to = [&](float heading) {  // the turn of the lunge and of turning round home
        swc1(m + 0x48, angle_seek_rate(pv + 0x3d0, lwc1(m + 0x48), heading, fpu_mul(dt_sq(), kPi), fpu_mul(dt_sq(), kTwoPi),
                                       fpu_mul(dt(), u2f(0x40860a92u))));
    };
    auto walker_reinit = [&] {  // asked (+0xbc bit 8): the walker set up on the animation as it is
        if (lbu(m + 0xbc) & 8) {
            sb(m + 0xbc, lbu(m + 0xbc) & 0xf7u);
            walker_init_l4(m, rec);
        }
    };
    const uint32_t state = lbu(m + 0x20);
    switch (state < 12 ? state : 12) {  // (the table L4:0x1dc4b0)
    case 0:  // (0x002d1ed4) set up
        swc1(pv + 0x20, 2.0f);
        sh(pv + 0x24, 2);
        swc1(pv + 0x30, 1.0f);
        sb(pv + 0x28, 1);
        sb(pv + 0x29, 0);
        sb(m + 0xbc, 0);
        sb(pv + 0x5a, 8);
        sw(pv + 0x3e0, 0);
        sh(pv + 0x3c6, 0);
        sb(pv + 0x58, 8);
        lq_sq(pv + 0x3b0, pos);
        if (lbu(pv + 0x121) == 0) sub_0024f590(m, 6, pv + 0x120);
        woodcut_walker_init_l4(m);
        if (lbu(pv + 0x3c0) != 0) {
            sb(m + 0x20, 3);
            moby_anim_set(m, 0xb, 0, frame_scale_i(0xa));
        } else {
            sb(m + 0x20, 1);
        }
        break;
    case 1: {  // (0x002d1f84) standing: idles; swings at a target near, notices one in sight; away from home, goes back
        if (anim_ended()) {
            if (sight < dist) {  // an idle: 0 five times in six, else 1
                libc_memset(sp + 0x80, 0, 0x18);
                sw(sp + 0x94, 1);
                const uint32_t pick = sp + 0x80 + (static_cast<uint32_t>(rand_mod(6)) << 2);
                moby_anim_set(m, lwu(pick), 0, frame_scale_i(0x14));
            } else {
                moby_anim_set(m, 0, 0, frame_scale_i(0x14));
            }
        }
        if (lh(pv + 0x3c2) != 0) break;
        if (swing_near()) swing(false);
        else if (dist < sight) notice();
        if (alerted || !(2.0f < vec2_distance(V(pos), V(pv + 0x3b0)))) break;
        // (not alerted: its plain sight)
        if (lwc1(pv + 0x3e8) < dist || (has_area() && !in_area(target))) chase(6);
        break;
    }
    case 2:  // (0x002d2200) its animation 2 ended: chasing, or standing when the target is out of its area
        if (lbu(m + 0x52) != 2 || !anim_ended()) break;
        if (alerted || !has_area() || in_area(target)) chase(5);
        else stand();
        break;
    case 3: {  // (0x002d2268) cutting wood (animation 12 at a random rate once 11 ends); chips fly at its frame 36
        if (lbu(m + 0x52) != 0 && anim_ended()) {
            moby_anim_set(m, 0xc, 0, frame_scale_i(0xa));
            swc1(m + 0x58, rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au)));
        }
        if (anim_crossed(m, 36.0f) != 0) {
            // twenty chips (class 0x5ec) from its joint 0, thrown back and up
            const uint32_t at = sp + 0x80, vel = sp + 0x90, up = sp + 0xa0, spin = sp + 0xb0;
            for (int32_t i = 0; i < 20; ++i) {
                for (uint32_t k = 0; k < 16; k += 4) sw(spin + k, 0);
                swc1(spin, rand_range_f(-1.0f, 1.0f));
                swc1(spin + 4, rand_range_f(-1.0f, 1.0f));
                swc1(spin + 8, rand_range_f(-1.0f, 1.0f));
                lq_sq(up, spin);
                moby_chain_point(m, 0, at);
                const float c = cos_vu(lwc1(m + 0x48));
                swc1(vel, fpu_mul(c, -fpu_mul(rand_range_f(5.0f, 10.0f), dt())));
                const float s = sin_vu(lwc1(m + 0x48));
                const float r = rand_range_f(5.0f, 10.0f);
                sw(vel + 8, 0);
                swc1(vel + 4, fpu_mul(s, -fpu_mul(r, dt())));
                vec3_set_length(V(up), V(up), fpu_mul(dt(), 3.0f));
                swc1(up + 8, fpu_add(lwc1(up + 8), fpu_mul(dt(), 3.0f)));
                vec3_add_inplace(V(vel), V(up));  // (the velocity's w: the frame's word as it is)
                swc1(spin, rand_angle());
                swc1(spin + 4, rand_angle());
                swc1(spin + 8, rand_angle());
                eud_chunk_spawn_l4(m, vel, at, spin, 0x5ec, 0, u2f(0x3eb33333u), u2f(0x3f333333u));
            }
        }
        if (lh(pv + 0x3c2) != 0) break;
        if (swing_near()) {
            swing(false);
            break;
        }
        if (dist < sight) notice();
        break;
    }
    case 4:  // (0x002d2578) waiting alerted (else standing): swings, chases into its area, or turns round to the target
        if (!alerted) {
            sb(m + 0x20, 1);
            break;
        }
        if (swing_near()) {
            swing(true);
            break;
        }
        if (!has_area() || in_area(target)) {
            chase(5);
            break;
        }
        if (kHalfPi < facing) turn_round();
        break;
    case 5: {  // (0x002d268c) chasing the target on the walker; at its area's edge stopping or turning round; home
        walker_reinit();
        walk(to_target_now());
        if (swing_near()) {
            swing(true);
            break;
        }
        if (!has_area()) break;
        if (!ahead_in_area()) {
            if (in_area(target)) turn_round();
            else if (alerted) sb(m + 0x20, 7);
        }
        if (!alerted && !in_area(target)) sb(m + 0x20, 6);
        break;
    }
    case 6:  // (0x002d28bc) going home on the walker; there (or the target back in its area): stopping
        walker_reinit();
        if (swing_near()) {
            swing(true);
            break;
        }
        walker_reinit();  // (the original tests the bit twice)
        if (vec2_distance(V(pos), V(pv + 0x3b0)) < 2.0f) {
            sb(m + 0x20, 7);
            break;
        }
        walk(to_home());
        if (!has_area()) break;
        if (!ahead_in_area()) {  // its area's edge: turning round, home after (+0xbc bit 0x10)
            turn_round();
            sb(m + 0xbc, lbu(m + 0xbc) | 0x10u);
            break;
        }
        if (in_area(target)) sb(m + 0x20, 7);
        break;
    case 7:  // (0x002d2ac8) stopping: once the walker stands, waiting alerted or standing
        if (swing_near()) {
            swing(true);
            break;
        }
        walker_update_l4(m, rec, fpu_mul(dt(), 0.0f), lwc1(m + 0x48), fpu_mul(dt_sq(), 20.0f));
        if (lbu(rec + 0xb7) != 0) break;
        sb(m + 0x20, alerted ? 4 : 1);
        break;
    case 8:  // (0x002d2b7c) swinging: then standing
        if (lbu(m + 0x52) != 4 || !anim_ended()) break;
        sh(pv + 0x3c2, 0);
        stand();
        break;
    case 9: {  // (0x002d2ba4) recoiling (animation 5): then a way away from the hero within its area, lunging along it
        if (lbu(m + 0x53) == 5 && (lbu(m + 0x52) != 5 || !anim_ended())) break;
        const uint32_t flags = lbu(m + 0xbc);
        if (flags & 4) {
            sb(m + 0xbc, flags & 0xfbu);
            if (has_area()) {
                // nine tries 6 ahead, away from the hero turned by the angles of L4:0x1dc468 (each on a random side,
                // then the other): the first in its area is the lunge's heading (animation 16)
                const uint32_t angles = sp + 0x80, probe = sp + 0xa0;
                for (uint32_t i = 0; i < 5; ++i) sw(angles + i * 4, lwu(L4_ADDR(0x001dc468) + i * 4));
                bool found = false;
                float sign = 1.0f;
                for (int32_t i = 0; i < 9; ++i) {
                    if (i & 1) {
                        sign = -sign;
                    } else {
                        sign = 1.0f;
                        if (rand_mod(100) & 1) sign = -1.0f;
                    }
                    const float side = fpu_mul(sign, lwc1(angles + (static_cast<uint32_t>(i >> 1) << 2)));
                    const float away = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84)));
                    const float heading = angle_add(side, away);
                    vec_from_spherical_v(V(probe), 6.0f, heading, lwc1(m + 0x44));
                    vec3_add(V(probe), V(probe), V(pos));
                    if (in_area(probe)) {
                        moby_anim_set(m, 0x10, 0, frame_scale_i(0xa));
                        swc1(pv + 0x3d4, heading);
                        sw(pv + 0x3d8, 0);
                        found = true;
                        break;
                    }
                }
                if (found) break;
                if (lbu(m + 0x52) == 5) {  // (none: swinging or standing)
                    if (swing_near()) {
                        moby_anim_set(m, 4, 0, frame_scale_i(0xa));
                        sb(m + 0x20, 8);
                    } else {
                        stand();
                    }
                    break;
                }
            }
            moby_anim_set(m, 0, 0, frame_scale_i(0xa));
            sb(m + 0x20, 1);
            sh(pv + 0x3c2, 0);
            break;
        }
        // the lunge: speeding up (to a ninth of the turns' time scale) while its animation 16 plays its first 28
        // frames; else slowing, and once stopped standing
        const float speed = lwc1(pv + 0x3d8);
        if (frame < 28.0f && lbu(m + 0x52) == 0x10) {
            const float most = fpu_mul(turn_dt(), u2f(0x3de38e39u));
            swc1(pv + 0x3d8, speed < most ? fpu_add(speed, fpu_mul(dt_sq(), 20.0f)) : most);
        } else {
            if (!(0.0f < speed)) {
                sb(m + 0x20, 1);
                sh(pv + 0x3c2, 0);
                break;
            }
            swc1(pv + 0x3d8, fpu_sub(speed, fpu_mul(dt_sq(), 200.0f)));
        }
        vec_from_spherical_v(V(step), lwc1(pv + 0x3d8), lwc1(pv + 0x3d4), lwc1(m + 0x44));
        vec3_add(V(pos), V(pos), V(step));
        turn_to(to_target_now());
        break;
    }
    case 10:  // (0x002d2ff0) turning round (animation 16): then chasing, or home (+0xbc bit 0x10, or the target outside)
        if (lbu(m + 0x52) == 0x10 && anim_ended()) {
            // (the area's path read without its -1 test)
            sb(m + 0x20, ((lbu(m + 0xbc) & 0x10) == 0 && in_area(target)) ? 5 : 6);
            moby_anim_set(m, 0, 0, frame_scale_i(0xa));
            sb(m + 0xbc, (lbu(m + 0xbc) | 8u) & 0xefu);
            break;
        }
        if (lbu(m + 0xbc) & 0x10) {
            turn_to(to_home());
        } else {
            swc1(m + 0x48, angle_seek_rate(pv + 0x3d0, lwc1(m + 0x48), to_target_now(), u2f(0x3cf5c28fu), u2f(0x3e99999au),
                                           fpu_mul(dt(), u2f(0x40cea5a1u))));
        }
        break;
    case 11: {  // (0x002d3188) knocked back dying: on the ground (or under 1), exploding into its three pieces
        const uint32_t flags = enemy_physics(m, pv + 0xc0);
        if ((flags & 0x40) == 0 && !(lwc1(m + 0x18) < 1.0f)) break;
        woodcut_explode_l4(m);
        const uint32_t z = L4_ADDR(0x0015f580);
        for (const uint32_t cls : {0x6cbu, 0x6ccu, 0x6cdu}) debris_spawn(m, cls, pos, m + 0x40, 0, 0, z, z, z, 0.0f);
        moby_retire(m);
        ee_stack_free(0x240);
        return;
    }
    default:
        break;
    }

    // ---- every frame: the colour pulse, the push away from the hero, the twitch, the fall
    color_pulse_update(m, pv + 0xb0);
    if (0.0f < lwc1(pv + 0x3e4)) {
        const float away = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84)));
        swc1(step, fpu_mul(cos_vu(away), lwc1(pv + 0x3e4)));
        swc1(step + 8, 0.0f);
        swc1(step + 4, fpu_mul(sin_vu(away), lwc1(pv + 0x3e4)));
        vec3_add(V(step), V(step), V(pos));
        const float t = fpu_mul(frame_scale_f(15.0f), frame_scale_f(15.0f));
        swc1(pv + 0x3e4, fpu_add(lwc1(pv + 0x3e4), fpu_div(-2.0f, t)));
        walker_step(m, pos, step, 1, 0.25f, 1.0f, 0.75f, u2f(0x3f5f66f3u));
    }
    const float twitch = fpu_mul(lwc1(pv + 0x3c8), u2f(0x3f75c28fu));
    swc1(pv + 0x3c8, twitch);
    quat_from_axis_angle(V(step), 1, twitch);
    quat_from_axis_angle(V(pv + 0x130), 2, 0.0f);
    quat_mul(V(pv + 0x130), V(pv + 0x130), V(step));
    if (lbu(m + 0x20) != 9) sb(pv + 0x3c1, lbu(m + 0x20));
    // the fall (2 below to the floor), but chasing (the walker keeps it on the ground); +0xbc == 6 as the original has it
    if (lbu(m + 0x20) != 5 && lbu(m + 0xbc) != 6) {
        swc1(pv + 0x3e0, fpu_add(lwc1(pv + 0x3e0), fpu_mul(dt_sq(), u2f(0x411ccccdu))));
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_sub(lwc1(pv + 0x3e0), 2.0f)));
        const float floor = floor_probe(0.5f, pos, 0);
        const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
        swc1(m + 0x18, z);
        if (z < floor) {
            swc1(m + 0x18, floor);
            swc1(pv + 0x3e0, 0.0f);
        }
        if (lwc1(m + 0x18) < 2.0f) {  // fallen off the level
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
        }
    }
    ee_stack_free(0x240);
}

}  // namespace rac1
