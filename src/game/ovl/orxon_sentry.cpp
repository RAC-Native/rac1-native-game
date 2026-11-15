// Orxon's own moby classes (level 10): the flying sentries. Class 1196 rises from its spawn, follows its path (waking the
// groups its path's points name), hovers about at random, and falls when shot down; class 1199 hovers about its box or
// its floor point and swoops at its target. Their frames' common part (hits, target, alert) is sentry_frame, in two
// builds (orx_2dfc38_l10, orx_2e17c0_l10). Header orxon_classes.h, registered in orxon_mobys.cpp; level 10's own addresses
// (`orig: L10:`).
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/walker.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

namespace {
// the moby's animation `set` unless it plays, blended over 10 frames
void anim_to(uint32_t m, uint32_t set) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(10));
}
// the sentry's floor noted (+0x130: the floor under it) and its place (+0x140)
void note_floor(uint32_t m, uint32_t pv) {
    floor_probe_v(0.5f, V(m + 0x10), 0);
    V(pv + 0x130) = coll_hit_point();
    lq_sq(pv + 0x140, m + 0x10);
}
}  // namespace

namespace {
// The two sentries' frame, before their states (class 1196's build `a`, class 1199's `b`). The alert timer +0x184 (180 ..
// 240 frames, re-armed when +0x38 asks) runs. Unless the sentry is falling (state 11 / 13): a hit (mask 0x330000; b: not
// from a moby of its own class) is handled (moby_hit_react_r); one that takes all its health +0x20 kills it - a: the
// level's first such kill (L10:0x161f38 on, the game's byte 0x13d418 not yet) plays a sound and a message; b: its own
// sound 2 - and sets it falling with its fall's rates, knocked away from the hit (b: the knock turned by
// vec_yaw_marked_v), its colour pulse and its bolts; a lesser one takes its damage and pulses. The hit slot is cleared
// (b: also without a hit). Then its range +0x180 (42 in mode 2, +0x178 plus 6 while alerted, +0x188 in state 1, else
// +0x178) and its target (+0xc0): in mode 2 any target, the hero outside its box (pvar +0x194; a: -1 < x < 0, b: -1 < x <
// 1, |y|, |z| < 1) setting +0x104 = 2; else a target along its path (+0x18c in state 1, +0x17c otherwise; any target
// without one); a target beyond its range from its floor point, or found while it is not visible, sets +0x104 = 2. +0x100
// defaults to the hero.
void sentry_frame(uint32_t m, bool b) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pulse = pv + 0x110;
    const uint32_t falling = b ? 13 : 11;
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x184, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    tick32(pv + 0x184);
    const uint32_t frame = ee_stack_alloc(0x60);
    if (lbu(m + 0x20) != falling) {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
        {
            int32_t st = lw(frame + 0x20);  // (the original's stack word, in and out)
            moby_hit_react_r(m, hit, pv + 0x20, 5, st, nullptr, nullptr, 4);
            sw(frame + 0x20, static_cast<uint32_t>(st));
        }
        const bool counts = hit != 0 && (!b || lh(lwu(hit + 0x20) + 0xa6) != lh(m + 0xa6));
        if (counts) {
            const float health = lwc1(pv + 0x20), damage = lwc1(hit + 0x2c);
            if (health <= damage) {
                if (!b && lw(L10_ADDR(0x00161f38)) != 0 && lbu(0x0013d418) == 0) {
                    sb(0x0013d418, 1);
                    sound_play_indexed(1, 0, 0);
                    sub_00263d40(TXT(0x53d6), -1);
                }
                sw(pv + 0x20, 0);
                const uint32_t set = b ? 4 : 5;
                const uint32_t anim = lbu(m + 0x53);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                if (anim != set) moby_anim_set(m, set, 0, frame_scale_i(10));
                sb(m + 0x20, falling);
                sb(pv + 0x9d, 3);
                swc1(pv + 0x70, fpu_mul(dt2(), 26.0f));
                swc1(pv + 0x74, u2f(0x3a03126fu));
                sw(pv + 0x84, 9);
                swc1(pv + 0x78, fpu_mul(dt(), 7.0f));
                swc1(pv + 0x7c, fpu_mul(dt(), 10.0f));
                if (b) moby_sound_start(2, 0, m);
                const float away = (lwu(hit + 0x30) & 1u) != 0 ? atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14))
                                                               : atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(hit)), fpu_sub(lwc1(m + 0x14), lwc1(hit + 4)));
                if (b) {
                    const uint32_t yaw = frame + 0x24;
                    swc1(yaw, away);
                    const Vec4 dir = V(hit + 0x10);
                    vec_yaw_marked_v(dir, *static_cast<float*>(ee_ram_ptr(yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x78)),
                                     *static_cast<float*>(ee_ram_ptr(pv + 0x7c)));
                    move_state_start(m, pv + 0x60, 4, 1, 0, lwc1(yaw));
                } else {
                    move_state_start(m, pv + 0x60, 5, 1, 0, away);
                }
                sb(pv + 0x117, 0x78);
                color_pulse_start(m, pulse);
                bolts_drop(m, 1, 3, 0, -1);
            } else {
                swc1(pv + 0x20, fpu_sub(health, damage));
                sb(pv + 0x117, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pulse);
            }
        }
        if (counts || b) sb(m + 0xa4, 0xff);
    }
    color_pulse_update(m, pulse);
    if (lw(pv + 0x174) == 2) swc1(pv + 0x180, 42.0f);
    else if (lw(pv + 0x184) != 0) swc1(pv + 0x180, fpu_add(lwc1(pv + 0x178), 6.0f));
    else if (lbu(m + 0x20) != 1) swc1(pv + 0x180, lwc1(pv + 0x178));
    else swc1(pv + 0x180, lwc1(pv + 0x188));
    const uint32_t target = pv + 0xc0;
    int32_t found;
    auto along = [&](int32_t p) {  // a target along path p (any target without one)
        if (p < 0) return target_select_any(m, target, lwc1(pv + 0x180));
        const uint32_t path = lvd::path(p);
        return target_select(m, target, 0, 0, path + 0x10, lw(path), lwc1(pv + 0x180));
    };
    bool check = true;
    if (lw(pv + 0x174) == 2) {
        found = target_select_any(m, target, lwc1(pv + 0x180));
        if (found == 2) {
            check = false;
        } else {  // the hero inside its box (in the box's frame), or not
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x194));
            const uint32_t d = frame, local = frame + 0x10;
            vec3_sub(V(d), V(HERO_POS), V(box + 0x30));
            sw(d + 0xc, 0);
            mat3_transform(V(local), V(d), M(box + 0x40));
            const float x = lwc1(local), y = lwc1(local + 4), z = lwc1(local + 8);
            if (x <= -1.0f || (b ? 1.0f : 0.0f) <= x || y <= -1.0f || 1.0f <= y || z <= -1.0f || 1.0f <= z) sw(pv + 0x104, 2);
        }
    } else {
        found = along(lbu(m + 0x20) == 1 ? lw(pv + 0x18c) : lw(pv + 0x17c));
    }
    if (check && found != 2) {
        if (lwc1(pv + 0x180) < vec2_distance(V(pv + 0x130), V(target)) || lbu(m + 0x31) == 0) sw(pv + 0x104, 2);
    }
    if (lw(pv + 0x100) == 0) sw(pv + 0x100, lwu(0x001413d0));
    ee_stack_free(0x60);
}
}  // namespace

// Class 1196's frame before its state (sentry_frame, build a).
// orig: L10:0x002dfc38
void orx_2dfc38_l10(uint32_t m) { sentry_frame(m, false); }

// Class 1199's frame before its state (sentry_frame, build b).
// orig: L10:0x002e17c0
void orx_2e17c0_l10(uint32_t m) { sentry_frame(m, true); }

// Class 1196: the frame's common part (orx_2dfc38_l10), its joint 0's node scaled 2.5, its floor band near the camera
// (within 29, while visible). States (the table at L10:0x001dd2b0): 0 sets it up (health 1, its floor point and place),
// goes on to 1 in mode 1 (+0x174), else goes; 1 waits (unless alerted in mode 2 with +0x174 set) then 2; 2 waits for its
// rise animation's cue (+0x70 bit 2) and rises (4) to 2 above its floor; 5 follows its path (+0x150, point +0x158),
// turning first, and at a point with a w lane plays its cue (6: the point's group of +0x160 woken), at the path's end
// hovers (10); 8 flies back to its floor point; 9 waits 120 frames and picks a hover point within 3 around it, 5 to 7
// up; 10 drifts to it at 4 a frame time unless something is in the way; 11 falls (enemy_physics) and bursts on landing.
// orig: L10:0x002df270
void orx_class1196_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    orx_2dfc38_l10(m);
    const uint32_t watched = lwu(pv + 0x100);
    joint_node_scale(m, 0, pv + 0x270, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }
    swc1(m + 0x58, 1.0f);
    const uint32_t frame = ee_stack_alloc(0x80);
    auto path_point = [&](int32_t i) { return lvd::path(lw(pv + 0x150)) + static_cast<uint32_t>(i) * 16 + 0x10; };
    const float turn_rate = u2f(0x41c90fdbu), turn_max = u2f(0x42490fdbu);  // (8 pi, 16 pi)
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002df33c)
        swc1(pv + 0x30, 0.5f);
        swc1(pv + 0x20, 1.0f);
        sh(pv + 0x24, 1);
        sb(pv + 0x28, 0);
        sb(pv + 0x29, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        lq_sq(pv + 0x130, m + 0x10);
        lq_sq(pv + 0x140, pv + 0x130);
        if (lw(pv + 0x174) == 1) sb(m + 0x20, 1);
        else moby_retire(m);
        break;
    case 1:  // (0x002df3ac)
        if (lw(pv + 0x104) == 2 && lw(pv + 0x174) != 0) break;
        sb(m + 0x20, 2);
        anim_to(m, 1);
        break;
    case 2:  // (0x002df3f0)
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 4);
        vec_clear(V(pv + 0x120));
        anim_to(m, 4);
        break;
    case 4: {  // rising (0x002df424)
        swc1(m + 0x58, 4.0f);
        const float accel = fpu_mul(dt2(), 18.0f);
        if (seek_1d(m + 0x18, pv + 0x128, fpu_add(lwc1(pv + 0x138), 2.0f), accel, accel, fpu_mul(dt(), 6.0f)) == 0.0f) sb(m + 0x20, 5);
        break;
    }
    case 5: {  // along its path (0x002df498)
        swc1(m + 0x58, 4.0f);
        const Vec4 pt = V(path_point(lw(pv + 0x158)));
        const uint32_t pos = frame + 0x10, speed = frame + 0x14;
        swc1(speed, vec3_length(V(pv + 0x120)));
        const float dist = vec3_distance(pt, V(m + 0x10));
        sw(pos, 0);
        const float accel = fpu_mul(dt2(), turn_rate);
        yaw_seek(m + 0x48, pv + 0x170, atan2_f(fpu_sub(pt.x, lwc1(m + 0x10)), fpu_sub(pt.y, lwc1(m + 0x14))), accel, accel, fpu_mul(dt(), turn_max));
        if (angle_dist(atan2_f(fpu_sub(pt.x, lwc1(m + 0x10)), fpu_sub(pt.y, lwc1(m + 0x14))), lwc1(m + 0x48)) < u2f(0x3f490fdbu)) {
            const float a = fpu_mul(dt2(), 20.0f);
            seek_1d(pos, speed, dist, a, a, fpu_mul(dt(), 20.0f));
            vec3_sub(V(pv + 0x120), pt, V(m + 0x10));
            vec3_clamp_length(pv + 0x120, lwc1(speed));
        } else {
            vec_clear(V(pv + 0x120));
        }
        walker_move_fx(m, pv + 0x120, 0x200, 0, 0.5f, 0.0f);
        if (!(dist < 1.0f)) break;
        const int32_t reached = lw(pv + 0x158);
        sw(pv + 0x158, static_cast<uint32_t>(reached + 1));
        if (0.0f < lwc1(path_point(reached) + 0xc)) {  // a point with a cue
            sb(m + 0x20, 6);
            anim_to(m, 3);
            break;
        }
        if (reached + 1 != lw(lvd::path(lw(pv + 0x150)))) break;
        sb(m + 0x20, 10);  // the path's end
        anim_to(m, 4);
        note_floor(m, pv);
        break;
    }
    case 6: {  // its cue at a path point (0x002df724)
        const float accel = fpu_mul(dt2(), turn_rate);
        yaw_seek(m + 0x48, pv + 0x170, atan2_f(fpu_sub(lwc1(watched + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(watched + 0x14), lwc1(m + 0x14))), accel,
                 accel, fpu_mul(dt(), turn_max));
        if ((lbu(m + 0x70) & 2) == 0) break;
        const int32_t k = float_to_int(lwc1(path_point(lw(pv + 0x158) - 1) + 0xc)) - 1;
        const uint32_t woken = moby_at(lw(pv + 0x160 + static_cast<uint32_t>(k) * 4));
        if (woken != 0) group_wake(woken);
        if (lw(pv + 0x158) != lw(lvd::path(lw(pv + 0x150)))) {
            sb(m + 0x20, 5);
            anim_to(m, 4);
        } else {
            sb(m + 0x20, 10);
            anim_to(m, 4);
            note_floor(m, pv);
        }
        break;
    }
    case 8: {  // back to its floor point (0x002df8b8)
        turn_towards(m + 0x44, 0, 0.0f, fpu_mul(dt(), u2f(0x41490fdbu)));
        yaw_turn_damped(m, pv + 0x170, atan2_f(fpu_sub(lwc1(pv + 0x130), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x134), lwc1(m + 0x14))),
                        u2f(0x3ca3d70au), u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40c90fdbu)));
        swc1(pv + 0x120, fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(dt(), 6.0f)));
        const float s = sin_vu(lwc1(m + 0x48));
        const float step = fpu_mul(dt(), 6.0f);
        swc1(pv + 0x128, 0.0f);
        const float want = fpu_add(lwc1(pv + 0x138), 2.0f);
        swc1(pv + 0x124, fpu_mul(s, step));
        float dz = fpu_sub(want, lwc1(m + 0x18));
        if (step < dz) dz = step;
        else if (dz < fpu_mul(dt(), -6.0f)) dz = fpu_mul(dt(), -6.0f);
        swc1(pv + 0x128, dz);
        walker_move_fx(m, pv + 0x120, 0x200, 0, 0.5f, 0.0f);
        if (vec2_distance(V(m + 0x10), V(pv + 0x130)) < 2.0f) sb(m + 0x20, 7);
        break;
    }
    case 9: {  // a new hover point (0x002dfa28)
        if (tick32(pv + 0x19c) == 0) break;
        sw(pv + 0x19c, static_cast<uint32_t>(frame_scale_i(0x78)));
        const float r = rand_range_f(0.0f, 3.0f);
        const float a = rand_angle();
        const uint32_t off = frame;
        swc1(off, fpu_mul(cos_vu(a), r));
        swc1(off + 8, 0.0f);
        swc1(off + 4, fpu_mul(sin_vu(a), r));
        swc1(off + 8, rand_range_f(5.0f, 7.0f));
        vec3_add(V(pv + 0x140), V(off), V(pv + 0x130));
        sb(m + 0x20, 10);
        break;
    }
    case 10: {  // drifting to it (0x002dfabc)
        const uint32_t vel = pv + 0x40, next = frame;
        yaw_turn_damped(m, pv + 0x170, atan2_f(fpu_sub(lwc1(pv + 0x140), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x144), lwc1(m + 0x14))),
                        u2f(0x3d4ccccdu), u2f(0x3e99999au), u2f(0x3e4ccccdu));
        vec3_sub(V(vel), V(pv + 0x140), V(m + 0x10));
        vec3_clamp_length(vel, fpu_mul(dt(), 4.0f));
        vec3_add(V(next), V(vel), V(m + 0x10));
        if (sphere_collide(next, u2f(0x3ecccccdu), 0, m) != 0) {
            vec_clear(V(vel));
            sb(m + 0x20, 9);
        } else {
            lq_sq(m + 0x10, next);
        }
        if (vec3_length(V(vel)) == 0.0f) sb(m + 0x20, 9);
        break;
    }
    case 11:  // falling (0x002dfbbc)
        if ((enemy_physics(m, pv + 0x60) & 1u) != 0) {
            moby_drop_bolts(m, 0x200, -1);
            enemy_death_burst(m, m + 0x10, 0.5f, 10.0f);
            moby_retire(m);
        }
        break;
    default:  // 3, 7 and from 12 (0x002dfc08)
        break;
    }
    ee_stack_free(0x80);
}

// Class 1199's group (its +0x21; 0xff: none): each of its members' countdown +0xbc set to the level's L10:0x161e00 frames.
// orig: L10:0x002e1cb0
void orx_2e1cb0_l10(uint32_t m) {
    const uint32_t g = lbu(m + 0x21);
    if (g == 0xff) return;
    const uint32_t list = lvd::group(static_cast<int32_t>(g));
    if (list == 0) return;
    for (uint32_t e = list;; e += 2) {
        const uint32_t member = moby_at(static_cast<int32_t>(lhu(e) & 0x7fffu));
        sb(member + 0xbc, static_cast<uint32_t>(frame_scale_i(lw(L10_ADDR(0x00161e00)))));
        if (lh(e) < 0) break;
    }
}

namespace {
// class 1199's hover height (+0x1a0: 0 .. L10:0x161e14 below its box's centre) and how long it keeps it (+0x1a4: the
// level's seconds L10:0x161e18, times 0.75 .. 1.25)
void hover_rearm(uint32_t pv, float lo) {
    swc1(pv + 0x1a0, rand_range_f(lo, lwc1(L10_ADDR(0x00161e14))));
    const float span = frame_scale_f(fpu_mul(lwc1(L10_ADDR(0x00161e18)), 60.0f));
    sw(pv + 0x1a4, static_cast<uint32_t>(float_to_int(fpu_mul(span, rand_range_f(0.75f, 1.25f)))));
}
// class 1199's flight at `speed` a frame time along its heading, its height eased to `want` by `climb` a frame time
void fly_level(uint32_t m, uint32_t pv, float speed, float want, float climb) {
    swc1(pv + 0x120, fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(dt(), speed)));
    const float s = sin_vu(lwc1(m + 0x48));
    const float step = fpu_mul(dt(), speed);
    sw(pv + 0x128, 0);
    swc1(pv + 0x124, fpu_mul(s, step));
    const float up = fpu_mul(dt(), climb);
    float dz = fpu_sub(want, lwc1(m + 0x18));
    if (up < dz) dz = up;
    else if (dz < fpu_mul(dt(), -climb)) dz = fpu_mul(dt(), -climb);
    swc1(pv + 0x128, dz);
}
// a point 4 to 6 around class 1199's floor point (+0x130) at the level's height (L10:0x161e04 .. 0x161e08) into `to`
void point_about_floor(uint32_t pv, uint32_t off, uint32_t to) {
    const float r = rand_range_f(4.0f, 6.0f);
    const float a = rand_angle();
    swc1(off, fpu_mul(cos_vu(a), r));
    const float y = fpu_mul(sin_vu(a), r);
    sw(off + 8, 0);
    swc1(off + 4, y);
    swc1(off + 8, rand_range_f(lwc1(L10_ADDR(0x00161e04)), lwc1(L10_ADDR(0x00161e08))));
    vec3_add(V(to), V(off), V(pv + 0x130));
}
float box_height(uint32_t pv) { return lwc1(lvd::box(lvd::BOXES, lw(pv + 0x194)) + 0x38); }
}  // namespace

// Class 1199: a flying sentry that dives at its target. Its frame's common part (orx_2e17c0_l10), its joint 1's node
// scaled 2.5, its floor band near the camera; it bobs (its height +- L10:0x161e0c by the phase +0x19c, a turn every
// L10:0x161e10 seconds) in every state but 0 and 13. States (the table at L10:0x001dd2e0): 0 sets it up (in mode 0 it
// rises from the floor: 3, the animation 3; in mode 2 it starts hovering about its box: 9; in mode 1 it goes); 2 rises;
// 3 flies to its point +0x140 (a new one about its floor point when blocked), resting (4) when there; 4 rests and picks
// new points; 3 / 4 start an attack (6) on a target more than 3 away (its group warned: orx_2e1cb0_l10); 6 turns to the
// target and picks a side; 7 swoops at it (keeping above the floor, 4 times its drop below the target), biting (8) once
// near, its height within 2; 8 bites (a hit record at its joint 0, a line test along it) and turns back (5) at the
// animation's cue or when it lost the target; 5 flies back to its floor point (then 3); 9 hovers about its box at its
// hover height and dives (10) at a target beyond L10:0x161dfc; 10 closes in and bites (11) within that distance; 11
// strikes (a hit record at its joint 0) until the cue, the target near or the hero in state 7 (then 12); 12 flies back to
// its floor point and hovers again (9); 13 falls (enemy_physics) and bursts on landing.
// orig: L10:0x002e01a8
void orx_class1199_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    orx_2e17c0_l10(m);
    const uint32_t watched = lwu(pv + 0x100);
    joint_node_scale(m, 1, pv + 0x270, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }
    swc1(m + 0x58, 1.0f);
    {
        const uint32_t st = lbu(m + 0x20);
        if (st != 0 && st != 13)  // (the bob taken off)
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_mul(lwc1(L10_ADDR(0x00161e0c)), cos_vu(lwc1(pv + 0x19c)))));
    }
    const uint32_t frame = ee_stack_alloc(0x60);
    const uint32_t vel = pv + 0x120;
    auto yaw_to = [&](float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); };
    auto turn = [&](float yaw, float rate) { yaw_turn_damped(m, pv + 0x170, yaw, u2f(0x3ca3d70au), u2f(0x3e99999au), fpu_mul(dt(), rate)); };
    auto attack_start = [&] {  // its group warned, the attack (6) and the animation 1
        if (lbu(m + 0x21) != 0xff) orx_2e1cb0_l10(m);
        sb(m + 0x20, 6);
        anim_to(m, 1);
    };
    auto back_off = [&] {  // the turn back (5), the animation 3 over 20 frames
        if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(0x14));
        sb(m + 0x20, 5);
    };
    const float two_pi = u2f(0x40c90fdbu);
    bool gone = false;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002e02ac)
        swc1(pv + 0x30, 0.25f);
        swc1(pv + 0x20, 1.0f);
        sb(pv + 0x29, 1);
        sh(pv + 0x24, 1);
        sb(pv + 0x28, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        lq_sq(pv + 0x130, m + 0x10);
        lq_sq(pv + 0x140, pv + 0x130);
        const float phase = rand_angle();
        sw(pv + 0x170, 0);
        swc1(pv + 0x19c, phase);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(lwc1(L10_ADDR(0x00161e1c)))));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(L10_ADDR(0x00161e20)), 8.0f))));
        const int32_t mode = lw(pv + 0x174);
        if (mode == 1) {
            moby_retire(m);
            gone = true;
        } else if (mode == 0) {
            sb(m + 0x20, 3);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 2.0f));
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(10));
        } else if (mode == 2) {
            const uint32_t off = frame;
            const float x = fpu_mul(cos_vu(rand_angle()), 10.0f);
            swc1(off, x);
            const float y = fpu_mul(sin_vu(rand_angle()), 10.0f);
            sb(m + 0x20, 9);
            sw(off + 8, 0);
            swc1(off + 4, y);
            swc1(m + 0x18, box_height(pv));
            vec3_add_inplace(V(m + 0x10), V(off));
            hover_rearm(pv, 0.0f);
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(10));
        }
        break;
    }
    case 2: {  // rising (0x002e0460)
        swc1(m + 0x58, 4.0f);
        const float accel = fpu_mul(dt2(), 18.0f);
        if (seek_1d(m + 0x18, pv + 0x128, fpu_add(lwc1(pv + 0x138), 2.0f), accel, accel, fpu_mul(dt(), 6.0f)) == 0.0f) sb(m + 0x20, 3);
        break;
    }
    case 3: {  // to its point (0x002e04c8)
        swc1(m + 0x58, 4.0f);
        const uint32_t goal = pv + 0x140, pos = frame + 0x50, speed = frame + 0x54;
        swc1(speed, vec3_length(V(vel)));
        const float dist = vec3_distance(V(goal), V(m + 0x10));
        sw(pos, 0);
        const float accel = fpu_mul(dt2(), u2f(0x41c90fdbu));
        yaw_seek(m + 0x48, pv + 0x170, yaw_to(lwc1(goal), lwc1(goal + 4)), accel, accel, fpu_mul(dt(), u2f(0x42490fdbu)));
        if (angle_dist(yaw_to(lwc1(goal), lwc1(goal + 4)), lwc1(m + 0x48)) < u2f(0x3f490fdbu)) {
            const float a = fpu_mul(dt2(), 20.0f);
            seek_1d(pos, speed, dist, a, a, fpu_mul(dt(), 20.0f));
            vec3_sub(V(vel), V(goal), V(m + 0x10));
            vec3_clamp_length(vel, lwc1(speed));
        } else {
            vec_clear(V(vel));
        }
        if (walker_move_fx(m, vel, 0x200, 0, 0.5f, 0.0f) == 1) {
            point_about_floor(pv, frame, goal);
        } else if (dist < u2f(0x3dcccccdu)) {
            sb(m + 0x20, 4);
            sw(pv + 0x198, static_cast<uint32_t>(frame_scale_i(float_to_int(rand_range_f(30.0f, 90.0f)))));
        }
        if (lw(pv + 0x104) != 2 && 3.0f < vec2_distance(V(m + 0x10), V(watched + 0x10)) && countdown_u8(m + 0xbc) != 0) attack_start();
        break;
    }
    case 4: {  // resting (0x002e0780)
        swc1(m + 0x58, 4.0f);
        if (lw(pv + 0x104) != 2 && 3.0f < vec2_distance(V(m + 0x10), V(watched + 0x10)) && countdown_u8(m + 0xbc) != 0) {
            attack_start();
            break;
        }
        if (tick32(pv + 0x198) == 0) break;
        point_about_floor(pv, frame, pv + 0x140);
        sb(m + 0x20, 3);
        break;
    }
    case 5:  // back to its floor point (0x002e0d8c)
        turn_towards(m + 0x44, 0, 0.0f, fpu_mul(dt(), u2f(0x41490fdbu)));
        turn(yaw_to(lwc1(pv + 0x130), lwc1(pv + 0x134)), two_pi);
        fly_level(m, pv, 6.0f, fpu_add(lwc1(pv + 0x138), 2.0f), 6.0f);
        walker_move_fx(m, vel, 0x200, 0, 0.5f, 0.0f);
        if (vec2_distance(V(m + 0x10), V(pv + 0x130)) < 2.0f) sb(m + 0x20, 3);
        break;
    case 6: {  // turning to the target (0x002e0890)
        turn(yaw_to(lwc1(watched + 0x10), lwc1(watched + 0x14)), two_pi);
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 7);
        if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(10));
        swc1(pv + 0x190, (rand_mod(0xff) & 1) != 0 ? u2f(0x3e860a92u) : u2f(0xbe860a92u));
        break;
    }
    case 7: {  // the swoop (0x002e0964)
        turn(angle_add(yaw_to(lwc1(watched + 0x10), lwc1(watched + 0x14)), lwc1(pv + 0x190)), two_pi);
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        const uint32_t aim = frame;
        lq_sq(aim, watched + 0x10);
        const float tz = lwc1(aim + 8), mz = lwc1(m + 0x18);
        swc1(aim + 8, tz < mz ? fpu_sub(tz, fpu_mul(fpu_sub(mz, tz), 4.0f)) : mz);
        const float raised = fpu_add(lwc1(aim + 8), 0.5f);
        swc1(aim + 8, raised < floor ? floor : raised);
        vec3_sub(V(vel), V(aim), V(m + 0x10));
        vec3_set_length(V(vel), V(vel), fpu_mul(dt(), 10.0f));
        const float c = cos_vu(lwc1(m + 0x48));
        swc1(vel, fpu_mul(c, vec2_length(V(vel))));
        const float s = sin_vu(lwc1(m + 0x48));
        swc1(vel + 4, fpu_mul(s, vec2_length(V(vel))));
        walker_move_fx(m, vel, 0x200, 0, 0.5f, 0.0f);
        turn_towards(m + 0x44, 0, -atan2_f(vec2_length(V(vel)), lwc1(pv + 0x128)), fpu_mul(dt(), two_pi));
        if (vec2_distance(V(m + 0x10), V(watched + 0x10)) < lwc1(L10_ADDR(0x00161dfc))) {
            if (abs_f(fpu_sub(lwc1(m + 0x18), lwc1(HERO_POS + 8))) < 2.0f) {
                moby_sound_start(0, 0, m);
                sb(m + 0x20, 8);
                vec_clear(V(vel));
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 4, frame_scale_i(5));
                break;
            }
            back_off();
        } else if (lw(pv + 0x104) == 2) {
            back_off();
        }
        break;
    }
    case 8: {  // the bite (0x002e0bd0)
        turn(yaw_to(lwc1(watched + 0x10), lwc1(watched + 0x14)), two_pi);
        const float d = vec2_distance(V(m + 0x10), V(pv + 0xe0));
        turn_towards(m + 0x44, 0, -atan2_f(d, fpu_sub(lwc1(pv + 0xe8), lwc1(m + 0x18))), fpu_mul(dt(), u2f(0x41490fdbu)));
        if (anim_crossed(m, 20.0f) != 0) {  // its bite: a hit record (+0x20 of the frame) at its joint 0 and along the line to it
            const uint32_t at = frame, rec = frame + 0x20;
            sw(rec + 0x14, 1);
            sw(rec + 0x20, 1);
            sw(rec + 0x10, m);
            swc1(rec + 0x1c, 1.0f);
            swc1(rec, cos_vu(lwc1(m + 0x48)));
            swc1(rec + 4, sin_vu(lwc1(m + 0x48)));
            sb(rec + 0x19, 1);
            swc1(rec + 8, 1.0f);
            swc1(rec + 0xc, u2f(0x45afdf66u));
            sh(rec + 0x1a, lhu(m + 0xa6));
            sb(rec + 0x18, 0);
            moby_chain_point_v(m, 0, V(at));
            mobys_in_sphere(at, u2f(0x3ecccccdu), 0, m, rec);
            segment_collide_v(V(m + 0x10), V(at), 9, m, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
        }
        if (lw(pv + 0x104) == 2 || (lbu(m + 0x70) & 2) != 0) back_off();
        break;
    }
    case 9: {  // hovering about its box (0x002e0efc)
        const float d = vec3_distance(V(m + 0x10), V(watched + 0x10));
        yaw_turn_damped(m, pv + 0x170, yaw_to(lwc1(pv + 0x130), lwc1(pv + 0x134)), u2f(0x3ca3d70au), u2f(0x3e99999au),
                        fpu_mul(dt(), u2f(0x3f490fdbu)));
        swc1(vel, fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(dt(), 4.0f)));
        const float s = sin_vu(lwc1(m + 0x48));
        sw(vel + 8, 0);
        swc1(vel + 4, fpu_mul(s, fpu_mul(dt(), 4.0f)));
        if (tick32(pv + 0x1a4) != 0) hover_rearm(pv, 0.0f);
        {
            const float t = dt();
            const float lim = fpu_add(t, t);
            float dz = fpu_sub(fpu_sub(box_height(pv), lwc1(m + 0x18)), lwc1(pv + 0x1a0));
            if (lim < dz) dz = lim;
            else if (dz < fpu_mul(t, -2.0f)) dz = fpu_mul(t, -2.0f);
            swc1(vel + 8, dz);
        }
        walker_move_fx(m, vel, 0x200, 0, 0.5f, 0.0f);
        if (lw(pv + 0x104) == 2) break;
        if (countdown_u8(m + 0xbc) == 0) break;
        if (!(lwc1(L10_ADDR(0x00161dfc)) < d)) break;
        if (lbu(m + 0x21) != 0xff) orx_2e1cb0_l10(m);
        sb(m + 0x20, 10);
        if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(10));
        break;
    }
    case 10: {  // closing in (0x002e10f4)
        const float d = vec3_distance(V(m + 0x10), V(watched + 0x10));
        if (lbu(m + 0x21) != 0xff) orx_2e1cb0_l10(m);
        turn(yaw_to(lwc1(watched + 0x10), lwc1(watched + 0x14)), u2f(0x3fc90fdbu));
        fly_level(m, pv, 4.0f, box_height(pv), 4.0f);
        walker_move_fx(m, vel, 0x200, 0, 0.5f, 0.0f);
        if (d < lwc1(L10_ADDR(0x00161dfc)) && abs_f(fpu_sub(box_height(pv), lwc1(m + 0x18))) < 1.0f) {
            moby_sound_start(0, 0, m);
            sb(m + 0x20, 11);
            if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(2));
            break;
        }
        if (lw(pv + 0x104) != 2) break;
        sb(m + 0x20, 9);
        hover_rearm(pv, 0.0f);
        break;
    }
    case 11: {  // the strike (0x002e12d8)
        if (lbu(m + 0x21) != 0xff) orx_2e1cb0_l10(m);
        turn(yaw_to(lwc1(watched + 0x10), lwc1(watched + 0x14)), two_pi);
        fly_level(m, pv, 6.0f, box_height(pv), 3.0f);
        walker_move_fx(m, vel, 0x200, 0, 0.5f, 0.0f);
        const uint32_t at = frame, rec = frame + 0x10;  // (a hit record at its joint 0; +0x18 / +0x20 / +0x28 left as they are)
        sw(rec + 0x14, 0x10001);
        sw(rec + 0x20, 1);
        sw(rec + 0x10, m);
        swc1(rec + 0x1c, 1.0f);
        vec_clear(V(rec));
        moby_chain_point_v(m, 0, V(at));
        mobys_in_sphere(at, u2f(0x3e4ccccdu), 0, m, rec);
        if (vec3_distance(V(m + 0x10), V(watched + 0x10)) < 1.0f || lw(0x001413dc) == 7 || (lbu(m + 0x70) & 2) != 0) {
            sb(m + 0x20, 12);
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(2));
            break;
        }
        if (lw(pv + 0x104) != 2) break;
        sb(m + 0x20, 9);
        hover_rearm(pv, 0.0f);
        break;
    }
    case 12:  // back to its floor point, then hovering (0x002e1514)
        turn(yaw_to(lwc1(pv + 0x130), lwc1(pv + 0x134)), u2f(0x3f860a92u));
        fly_level(m, pv, 4.0f, box_height(pv), 3.0f);
        vec3_add_inplace(V(m + 0x10), V(vel));
        if (vec2_distance(V(m + 0x10), V(pv + 0x130)) < 2.0f) {
            sb(m + 0x20, 9);
            sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(0x78)));
            hover_rearm(pv, 0.0f);
        }
        break;
    case 13:  // falling (0x002e16b4)
        if ((enemy_physics(m, pv + 0x60) & 1u) != 0) {
            moby_drop_bolts(m, 0x200, -1);
            enemy_death_burst(m, m + 0x10, 0.5f, 10.0f);
            moby_retire(m);
            gone = true;
        }
        break;
    default:  // 1 and from 14
        break;
    }
    ee_stack_free(0x60);
    if (gone) return;
    const uint32_t st = lbu(m + 0x20);
    if (st == 0 || st == 13) return;
    // the bob put back: the phase turned by a full turn over L10:0x161e10 seconds
    const float period = static_cast<float>(float_to_int(frame_scale_f(fpu_mul(lwc1(L10_ADDR(0x00161e10)), 60.0f))));
    const float phase = angle_add(lwc1(pv + 0x19c), fpu_mul(fpu_div(360.0f, period), u2f(0x3c8efa35u)));
    swc1(pv + 0x19c, phase);
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(L10_ADDR(0x00161e0c)), cos_vu(phase))));
}

}  // namespace rac1
