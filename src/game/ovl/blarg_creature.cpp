// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1048, a creature that lives in a pack inside a
// territory (a path of the level as its outline): it dozes, wakes when the hero comes near, circles him with its pack
// mates, lunges and bites, backs off, and goes home when he leaves; with its three helpers. Header blarg_own.h;
// registered in blarg_mobys.cpp. Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR (the class's tuning at
// 0x161eb0 .. 0x161eec), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/walker.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 its position, +0x1c0, +0x23c / +0x240 mobys it touches)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
const float kDegree = u2f(0x3c8efa35u);  // pi / 180
const float kTwoPi = u2f(0x40c90fdbu);

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of the level's moby table at the index a group list holds (bit 15: the list's last)
uint32_t group_moby(uint32_t entry) { return lwu(L6_ADDR(0x0015ffd8)) + ((lhu(entry) & 0x7fffu) << 8); }

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }
}  // namespace

// The creature's pack (its group +0x21) alerted: each mate dozing (state 1) wakes (state 2).
// orig: L6:0x002f9698
void creature_pack_alert_l6(uint32_t m) {
    const uint32_t list = lvd::group(static_cast<int32_t>(lbu(m + 0x21)));
    if (list == 0) return;
    for (uint32_t e = list;; e += 2) {
        const uint32_t mate = group_moby(e);
        if (lbu(mate + 0x20) == 1) sb(mate + 0x20, 2);
        if (lh(e) < 0) break;
    }
}

// The creature's frame before its state: its alert timer (+0x1ac, set to 3 .. 4 seconds when +0x38 asks for it); a hit
// (mask 0x330000; not from another creature, class 0x418) takes its damage from the health +0x20 and alerts the pack;
// killed (health gone: state 0x11, knocked away and its bolts dropped) or knocked back (the reaction 3 .. 10, or any hit
// while +0x1cc is set: state 0xe), with its colour pulse +0x60; every fourth frame (by its slot) in states 8, 9, 12 and 13
// it pushes what it touches; its glow (+0x90) pulses (0x161ee4 degrees a second, between 0x161ee8 and 0x161eec). Its
// range +0x1a0: 6 beyond 0x161ed0 while alert, 0x161ed0 awake, 6 dozing with the hero on it, else 0. It picks its target
// (pvar +0x120) inside its territory (the path +0x1b0) within the range; one out of the range from its home (+0x170), or
// 3 above or below it, is lost (+0x164 = 2). With no target moby (+0x160) yet, the hero's moby.
// orig: L6:0x002f91e8
void creature_frame_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t hit_state = sp, hit_damage = sp + 4;
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x1ac, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1ac)));
    swc1(hit_damage, 0.0f);
    uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    if (hit != 0 && lwu(hit + 0x20) != 0 && lh(lwu(hit + 0x20) + 0xa6) == 0x418) hit = 0;  // (a mate's)
    int32_t reaction;
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_damage);
        reaction = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_damage, dmg);
    }
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0x11 && lbu(m + 0x20) != 3) {
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), lwc1(hit_damage)));
        if (lbu(m + 0x21) != 0xff) creature_pack_alert_l6(m);
        if (lwc1(pv + 0x20) <= 0.0f) {
            reaction = 1;
        } else if (lw(pv + 0x1cc) != 0) {
            reaction = 0xb;
            sb(pv + 0x67, 0xfa);
        }
        // (the reactions' table 0x2016f0: 1 / 2 killed, 3 .. 10 knocked back, the rest nothing more)
        auto knock = [&](uint32_t state, float spin, float vmax, float up, float speed, uint32_t set) {
            sb(m + 0x20, state);
            swc1(pv + 0x8c, up);
            swc1(pv + 0xc0, spin);
            swc1(pv + 0xc4, vmax);
            swc1(pv + 0x88, speed);
            move_state_start(m, pv + 0x70, set, 1, 0, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
            sb(pv + 0x67, 0xfa);
        };
        if (reaction == 1 || reaction == 2) {
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            knock(0x11, 7.5f, 14.5f, fpu_mul(dt(), 5.0f), fpu_mul(dt(), 8.0f), 0xa);
            moby_drop_bolts(m, 0, -1);
        } else if (reaction >= 3 && reaction <= 10) {
            knock(0xe, 5.0f, 8.5f, dt(), fpu_add(dt(), dt()), 9);
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    const uint32_t state = lbu(m + 0x20);
    if (state == 8 || state == 9 || state == 12 || state == 13) {
        if (lw(L6_ADDR(0x0015f5cc)) % 4 == static_cast<int32_t>((m >> 8) & 3u))
            moby_sphere_contact(m, m + 0x10, NOT_ADDR(0x10000), 0, 1, 0, u2f(0x3f333333u), u2f(0x3f666666u), 1.0f);
    }
    const float glow = angle_add(lwc1(pv + 0x1c8), fpu_mul(fpu_mul(lwc1(L6_ADDR(0x00161ee4)), kDegree), dt()));
    swc1(pv + 0x1c8, glow);
    sw(m + 0x90, rgba_lerp(lwu(L6_ADDR(0x00161ee8)), lwu(L6_ADDR(0x00161eec)), fpu_mul(fpu_add(sin_vu(glow), 1.0f), 0.5f)));
    if (lw(pv + 0x1ac) != 0) {
        swc1(pv + 0x1a0, fpu_add(lwc1(L6_ADDR(0x00161ed0)), 6.0f));
    } else if (lbu(m + 0x20) != 1) {
        swc1(pv + 0x1a0, lwc1(L6_ADDR(0x00161ed0)));
    } else if (m == lwu(PLAYER + 0x240) || m == lwu(PLAYER + 0x23c)) {
        swc1(pv + 0x1a0, 6.0f);
    } else {
        sw(pv + 0x1a0, 0);
    }
    const uint32_t outline = lvd::path(lw(pv + 0x1b0));
    if (target_select(m, pv + 0x120, 0, 0, outline + 0x10, lw(outline), lwc1(pv + 0x1a0)) != 2) {
        if (lwc1(pv + 0x1a0) < vec2_distance(V(pv + 0x170), V(pv + 0x120)))
            sw(pv + 0x164, 2);
        else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128))))
            sw(pv + 0x164, 2);
    }
    if (lw(pv + 0x160) == 0) sw(pv + 0x160, lwu(0x001413d0));
    ee_stack_free(0x60);
}

// The heading the creature takes about its target moby (+0x160): alone (no pack), straight away from it; in a pack, the
// middle of the widest gap among its mates about the target (those nearer the target than it, or lunging: state 8),
// within 0x161ed8 degrees either side of its own bearing from the target.
// orig: L6:0x002f96f8
float creature_heading_l6(uint32_t m) {
    const uint32_t target = lwu(lwu(m + 0x78) + 0x160);
    if (lbu(m + 0x21) == 0xff) return heading(lwc1(target + 0x10), lwc1(target + 0x14), lwc1(m + 0x10), lwc1(m + 0x14));
    float right = fpu_mul(lwc1(L6_ADDR(0x00161ed8)), kDegree);
    float left = -right;
    const float bearing = heading(lwc1(target + 0x10), lwc1(target + 0x14), lwc1(m + 0x10), lwc1(m + 0x14));
    const float dist = vec3_distance(V(target + 0x10), V(m + 0x10));
    // (the original also builds the point two units along the bearing from the hero, in a stack word nothing reads)
    const uint32_t list = lvd::group(static_cast<int32_t>(lbu(m + 0x21)));
    if (list == 0) return 0.0f;
    for (uint32_t e = list;; e += 2) {
        const uint32_t mate = group_moby(e);
        if (lbu(mate + 0x20) == 8 || vec3_distance(V(mate + 0x10), V(target + 0x10)) < dist) {
            const float d = angle_sub(heading(lwc1(target + 0x10), lwc1(target + 0x14), lwc1(mate + 0x10), lwc1(mate + 0x14)), bearing);
            if (0.0f < d && d < right) {
                right = d;
            } else if (d < 0.0f && left < d) {
                left = d;
            }
        }
        if (lh(e) < 0) break;
    }
    return angle_add(angle_add(left, fpu_mul(angle_dist(right, left), 0.5f)), bearing);
}

// Class 1048, the creature. pvars: health +0x20, the move record +0x70 (knock-backs and jumps), the walker record +0xd0
// (its speed +0xf4, the top speed +0x1a4), its target +0x120, its target moby +0x160 (lost: +0x164 = 2), its home +0x170,
// the yaw rate +0x190, its range +0x1a0, a wait +0x1a8, the alert timer +0x1ac, its territory (a path) +0x1b0, its den box
// +0x1bc, its wake box +0x1c0 (none: negative), +0x1b4 awake at the start, +0x1b8 the longest pause, +0x1c4 how near the
// territory's edge it turns back, the glow's phase +0x1c8, +0x1cc it starts snarling (state 15). States: 0 set up; 1 dozes
// (until the pack is alerted); 2 awake, its idle animations, lunging (8) at the hero within 0x161ed0; 3 waits for the hero
// in its wake box; 4 rises and jumps to its den box (5: in the air); 6 turns to its target and pauses (7) or attacks (9);
// 8 closes in on the target (to 0x161ec4) at the heading its pack leaves it, biting (11) when near, at its height and the
// hero not just hit (+0x1c0 under 10 frames); 9 runs at the target the same way and, near the territory's edge, rears (10)
// or bites; 11 bites (a sphere at its mouth hits mobys between its animation's frames 1 and 8); 12 goes home; 13 backs
// away, then bites again (11); 14 knocked back, landing in its territory (13) or retired; 15 snarls at the hero and bites
// him (16) within 4; 17 dies: a burst and its debris (classes 0x6a0 / 0x69f) where it lands, or retired once below 2.
// orig: L6:0x002f7d78
void blg_class1048_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t dir = sp, step_out = sp + 0x10, aim = sp + 0x20, cross = sp + 0x30, mouth = sp + 0x40;
    const uint32_t pos = m + 0x10;
    creature_frame_l6(m);
    joint_node_scale(m, 1, pv + 0x1e0, u2f(0x40133333u));  // 2.3
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L6_ADDR(0x00167540))) < 27.0f) {  // (near the camera)
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L6_ADDR(0x00161eb0))));
    const uint32_t target = lwu(pv + 0x160);
    // the animation set `set` blended over `frames` (already scaled) from `frame`, unless it is playing
    auto anim_to = [&](uint32_t set, int32_t frames, int32_t frame = 0) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, frame, frames);
    };
    // the state `state` and the animation set `set` over `frames` 60 Hz frames
    auto go = [&](uint32_t state, uint32_t set, int32_t frames) {
        sb(m + 0x20, state);
        anim_to(set, frame_scale_i(frames));
    };
    auto turn_to = [&](float x, float y) {
        yaw_turn_damped(m, pv + 0x190, heading(lwc1(pos), lwc1(pos + 4), x, y), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                        fpu_mul(dt(), kTwoPi));
    };
    auto face_hero = [&] {
        const float accel = fpu_mul(dt_sq(), kTwoPi);
        yaw_seek(m + 0x48, pv + 0x190, heading(lwc1(pos), lwc1(pos + 4), lwc1(HERO_POS), lwc1(HERO_POS + 4)), accel, accel,
                 fpu_mul(dt(), u2f(0x41490fdbu)));
    };
    // a step of the walker record toward the yaw `yaw` (the moby's own yaw kept), then pushed back inside its territory
    auto walk = [&](float yaw) {
        const float own = lwc1(m + 0x48);
        swc1(m + 0x48, yaw);
        swc1(dir, fpu_add(cos_vu(yaw), cos_vu(yaw)));
        swc1(dir + 4, fpu_add(sin_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48))));
        sw(dir + 8, 0);
        walker_record_step(1.0f, m, pv + 0xd0, dir, step_out);
        swc1(m + 0x48, own);
        path_push_out_2d(lwu(pv + 0x1b0), pos, pos, 0.5f);
    };
    // a step toward the point at the pack's heading about the target, as near it as 0x161ec4 allows
    auto close_in = [&] {
        float reach = vec2_distance(V(pos), V(target + 0x10));
        if (lwc1(L6_ADDR(0x00161ec4)) < reach) reach = lwc1(L6_ADDR(0x00161ec4));
        const float h = creature_heading_l6(m);
        swc1(aim, fpu_mul(cos_vu(h), reach));
        swc1(aim + 4, fpu_mul(sin_vu(h), reach));
        sw(aim + 8, 0);
        vec3_add(V(aim), V(aim), V(target + 0x10));
        walk(heading(lwc1(pos), lwc1(pos + 4), lwc1(aim), lwc1(aim + 4)));
    };
    // the bite: between its animation's frames 1 and 8, the mobys within 0.5 of its mouth (joint 0) hit (a hit record of
    // the yaw's direction, the damage 1, the creature its source); true while it bites
    auto bite = [&](bool at_hero) {
        if (!(1.0f < moby_anim_time(m) && moby_anim_time(m) < 8.0f)) return false;
        // (the original also builds 0.2 along its yaw in a stack word nothing reads)
        moby_chain_point_v(m, 0, V(mouth));
        sw(dir + 0x20, 1);
        swc1(dir + 0x1c, 1.0f);
        sw(dir + 0x10, m);
        sw(dir + 0x14, 1);
        swc1(dir, cos_vu(lwc1(m + 0x48)));
        swc1(dir + 4, sin_vu(lwc1(m + 0x48)));
        sw(dir + 8, 0);
        mobys_in_sphere(mouth, 0.5f, 0, m, dir);
        if (at_hero) segment_collide_v(V(mouth), V(pos), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(dir)));
        return true;
    };
    auto approach_speed = [&](float goal, float rate) { approach_f(pv + 0xf4, goal, rate); };
    const float run = fpu_mul(lwc1(pv + 0x1a4), dt());
    switch (lbu(m + 0x20)) {  // (the table 0x2016a0: one case a state, 0 .. 17)
    case 0: {
        sb(pv + 0x28, 3);
        sb(pv + 0x29, 1);
        swc1(pv + 0x20, 4.0f);
        sh(pv + 0x24, 4);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(10.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(14.0f)));
        sh(m + 0x34, lhu(m + 0x34) | 0x5000u);
        lq_sq(pv + 0x170, pos);
        swc1(pv + 0x1c4, rand_range_f(0.0f, 1.0f));
        walker_record_init_l1(pv + 0xd0);
        const int32_t reach = float_to_int(fpu_mul(lwc1(L6_ADDR(0x00161eb0)), 1024.0f));
        swc1(pv + 0xd8, 2.0f);
        swc1(pv + 0xdc, 2.0f);
        swc1(pv + 0xd4, lwc1(L6_ADDR(0x00161eb0)));
        sw(pv + 0xd0, static_cast<uint32_t>(reach));
        swc1(pv + 0xf4, run);
        sb(pv + 0xad, 0);
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(768.0f)));
        swc1(pv + 0x98, 0.75f);
        swc1(pv + 0x80, fpu_mul(lwc1(L6_ADDR(0x00161eb4)), dt_sq()));
        swc1(pv + 0x84, u2f(0x3a03126fu));  // 0.0005
        sw(pv + 0x94, 9);
        swc1(pv + 0x1c8, rand_angle());
        if (lw(pv + 0x1cc) != 0) {
            sb(m + 0x20, 0xf);
            anim_to(2, 0);
        } else if (lw(pv + 0x1c0) >= 0) {
            sb(m + 0x20, 3);
        } else if (lw(pv + 0x1b4) != 0) {
            sb(m + 0x20, 2);
            sw(pv + 0x1a8, static_cast<uint32_t>(frame_scale_i(1)));
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            if (lbu(m + 0x21) != 0xff) creature_pack_alert_l6(m);
        } else {
            sb(m + 0x20, 1);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            swc1(pv + 0x1a0, lwc1(L6_ADDR(0x00161ed0)));
        }
        break;
    }
    case 1:
        if (lw(pv + 0x164) == 2) break;
        sb(m + 0x20, 2);
        sw(pv + 0x1a8, static_cast<uint32_t>(frame_scale_i(1)));
        if (lbu(m + 0x21) != 0xff) creature_pack_alert_l6(m);
        break;
    case 2:
        if (lbu(m + 0x53) == 0) {  // idle: now and then the animation 3
            if (lw(pv + 0x1a8) == 0) {
                sw(pv + 0x1a8, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(0.0f, 20.0f)))));
            } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1a8))) != 0) {
                anim_to(3, frame_scale_i(10));
            }
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;  // (the animation not at its end)
        if (vec2_distance(V(pos), V(HERO_POS)) < lwc1(L6_ADDR(0x00161ed0))) {
            sb(m + 0x20, 8);
            const float spread = fpu_mul(lwc1(pv + 0x198), kDegree);
            swc1(pv + 0x194, rand_range_f(spread, -spread));
            anim_to(1, frame_scale_i(10));
            break;
        }
        {
            const uint32_t sets = lbu(lwu(lwu(m + 0x24) + 0x50) + 0x10);  // (a count of the class's data: +0x50, then +0x10)
            sb(m + 0x20, 6);
            if (lbu(m + 0x53) == 2) break;
            const float pick = rand_range_f(0.0f, fpu_sub(static_cast<float>(static_cast<int32_t>(sets)), 1.0f));
            moby_anim_set(m, 2, static_cast<int32_t>(f2u(eer::cvt_w(pick))), frame_scale_i(0x1e));
        }
        break;
    case 3:
        if (lw(pv + 0x1c0) < 0) break;
        if (point_in_box(HERO_POS, lw(pv + 0x1c0)) == 0) break;
        sb(m + 0x20, 4);
        anim_to(3, frame_scale_i(5));
        break;
    case 4: {
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (lbu(m + 0x53) == 3) {
            moby_anim_set(m, 6, 0, frame_scale_i(5));
            break;
        }
        if (lbu(m + 0x53) != 6) break;
        sb(m + 0x20, 5);
        lq_sq(pv + 0x170, lvd::box(lvd::BOXES, lw(pv + 0x1bc)) + 0x30);
        const float speed = fpu_div(vec2_distance(V(pos), V(pv + 0x170)), static_cast<float>(frame_scale_i(0x3c)));
        swc1(pv + 0x88, speed);
        swc1(pv + 0x8c, sub_0025abf0(pos, pv + 0x170, nullptr, speed, -lwc1(pv + 0x80)));
        sw(pv + 0x94, 5);
        sb(pv + 0xad, 3);
        move_state_start(m, pv + 0x70, 7, 1, 0, heading(lwc1(pos), lwc1(pos + 4), lwc1(pv + 0x170), lwc1(pv + 0x174)));
        anim_to(7, 5);  // (a raw count)
        break;
    }
    case 5:
        if ((enemy_physics(m, pv + 0x70) & 1) == 0) break;
        sb(m + 0x20, 6);
        anim_to(2, 5);  // (a raw count)
        break;
    case 6:
        turn_to(lwc1(pv + 0x120), lwc1(pv + 0x124));
        if (lw(pv + 0x164) != 2) {
            sb(m + 0x20, 7);
            sw(pv + 0x1a8, static_cast<uint32_t>(float_to_int(rand_range_f(0.0f, static_cast<float>(lw(pv + 0x1b8))))));
        } else if (lw(pv + 0x1ac) != 0) {
            go(9, 1, 10);
        }
        break;
    case 7:
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1a8))) == 0) break;
        {
            sb(m + 0x20, 8);
            const float spread = fpu_mul(lwc1(pv + 0x198), kDegree);
            swc1(pv + 0x194, rand_range_f(spread, -spread));
            anim_to(1, frame_scale_i(10));
        }
        break;
    case 8:
        if (lwc1(L6_ADDR(0x00161ec4)) < vec2_distance(V(pos), V(target + 0x10)))
            approach_speed(run, fpu_mul(dt_sq(), 6.0f));
        else
            approach_speed(0.0f, fpu_mul(dt_sq(), 20.0f));
        turn_to(lwc1(target + 0x10), lwc1(target + 0x14));
        close_in();
        if (lwc1(L6_ADDR(0x00161ec8)) < vec3_distance(V(pos), V(pv + 0x170)) || lw(pv + 0x164) == 2) {
            sb(m + 0x20, 0xc);
            break;
        }
        if (!(vec2_distance(V(pos), V(target + 0x10)) < lwc1(L6_ADDR(0x00161ec4)))) break;
        if (!(abs_f(fpu_sub(lwc1(target + 0x18), lwc1(m + 0x18))) < 1.0f)) break;
        if (!(lw(PLAYER + 0x1c0) < frame_scale_i(10))) break;
        go(0xb, 4, 10);
        break;
    case 9: {
        approach_speed(run, fpu_mul(dt_sq(), 6.0f));
        turn_to(lwc1(target + 0x10), lwc1(target + 0x14));
        close_in();
        bool at_edge = false;
        if (path_crossing(lw(pv + 0x1b0), pos, target + 0x10, cross) != 0 && vec3_distance(V(pos), V(cross)) < lwc1(pv + 0x1c4))
            at_edge = true;
        if (lw(pv + 0x1ac) == 0) {
            sb(m + 0x20, 0xc);
        } else if (at_edge) {
            go(0xa, 5, 10);
        } else if (vec2_distance(V(pos), V(target + 0x10)) < lwc1(L6_ADDR(0x00161ec4))) {
            go(0xb, 4, 10);
        }
        break;
    }
    case 10:
        if ((lbu(m + 0x70) & 2) == 0) break;
        go(9, 1, 20);
        break;
    case 11:
        turn_to(lwc1(target + 0x10), lwc1(target + 0x14));
        if (bite(false)) break;
        if ((lbu(m + 0x70) & 2) == 0) break;
        go(8, 1, 20);
        break;
    case 12:
        approach_speed(run, fpu_mul(dt_sq(), 6.0f));
        turn_to(lwc1(pv + 0x170), lwc1(pv + 0x174));
        walk(lwc1(m + 0x48));
        if (vec3_distance(V(pos), V(pv + 0x170)) < 1.0f) {
            go(6, 2, 20);
        } else if (lw(pv + 0x164) != 2) {
            go(8, 1, 20);
        }
        break;
    case 13: {
        approach_speed(fpu_mul(fpu_mul(lwc1(pv + 0x1a4), 3.0f), dt()), fpu_mul(dt_sq(), 20.0f));
        turn_to(lwc1(target + 0x10), lwc1(target + 0x14));
        walk(angle_add(heading(lwc1(pos), lwc1(pos + 4), lwc1(target + 0x10), lwc1(target + 0x14)), u2f(0x40490fd0u)));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1a8))) == 0 &&
            !(lwc1(L6_ADDR(0x00161ed4)) < vec3_distance(V(pos), V(target + 0x10))))
            break;
        sb(m + 0x20, 0xb);
        swc1(pv + 0xf4, fpu_mul(dt(), 5.0f));
        if (lw(L6_ADDR(0x00161edc)) != 0)
            anim_to(4, frame_scale_i(5), lw(L6_ADDR(0x00161ee0)));
        else
            anim_to(4, frame_scale_i(10), 0xa);
        break;
    }
    case 14:
        if ((enemy_physics(m, pv + 0x70) & 0x41) != 0) {
            const uint32_t outline = lvd::path(lw(pv + 0x1b0));
            if (point_in_polygon(pos, outline + 0x10, lw(outline)) == 0) {
                moby_retire(m);
                break;
            }
            sb(m + 0x20, 0xd);
            swc1(m + 0x58, 1.0f);
            anim_to(1, frame_scale_i(0x14));
            sw(pv + 0x1a8, static_cast<uint32_t>(frame_scale_i(0x1e)));
        }
        path_push_out_2d(lwu(pv + 0x1b0), pos, pos, 0.5f);
        break;
    case 15:
        face_hero();
        if (!(vec2_distance(V(pos), V(HERO_POS)) < 4.0f)) break;
        go(0x10, 4, 20);
        break;
    case 16:
        face_hero();
        if (bite(true)) break;
        if ((lbu(m + 0x70) & 2) == 0) break;
        go(0xf, 2, 20);
        break;
    case 17:
        if ((enemy_physics(m, pv + 0x70) & 0x40) != 0) {
            lq_sq(sp, pos);
            enemy_death_burst(m, sp, 1.5f, 10.0f);
            debris_burst(m, 0x6a0, 2, 0x69f, 1, 5, 2);
            moby_retire(m);
        } else if (lwc1(m + 0x18) < 2.0f) {
            moby_retire(m);
        }
        break;
    default:
        break;
    }
    ee_stack_free(0xf0);
}

}  // namespace rac1
