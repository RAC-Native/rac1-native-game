// Kerwan's own moby classes: class 573, a small walking robot of the streets that keeps to its post, walks a path when it is
// given one, comes at whatever is in its reach and strikes it, goes home again, can be knocked off and carried, and that
// follows a leader of class 574 (its pvar +0x248). Header kerwan_mobys.h, registered in kerwan_mobys.cpp. Addresses are
// Kerwan's (`orig: L3:`); data only Kerwan has is L3_ADDR, the engine's words shared with Veldin are the level's own (OV),
// the base ELF's are bare.
//
// The pvars: +0x20 its health, +0x38 alerted, +0x40 the walk's out record, +0x110 the colour pulse, +0x120 the move record
// (knock-back, fall, carry), +0x180 the walker record, +0x200 its home, +0x220 the turn's speed, +0x22c / +0x230 its reach (the
// second doubled while alerted), +0x234 its walk speed, +0x23c the wait before it turns, +0x240 the alert's time, +0x244 the
// rest after a strike, +0x248 its leader (-1: none), +0x270 its path (-1: none), +0x274 the path's reach, +0x278 the path's
// point, +0x27c the box it guards, +0x280 the joint it scales.
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/walker.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of index `i` (the moby list in 0x100-byte steps)
uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the heading from the moby to (x, y)
float heading_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// a wait of 30 .. 90 frames (a random float, truncated)
int32_t random_wait() { return frame_scale_i(static_cast<int32_t>(f2u(eer::cvt_w(rand_range_f(30.0f, 90.0f))))); }
}  // namespace

// Whether the leader `l` (a class 574 alive) calls its followers: in its animation 10 from frame 10 on, in state 10 or 13, or
// without its own target (+0x284 -1). 0 for no moby, a moby gone or another class.
// orig: L3:0x002c6ca0
uint32_t walker_leader_calls_l3(uint32_t l) {
    if (l == 0) return 0;
    const uint32_t lpv = lwu(l + 0x78);
    const uint32_t st = lbu(l + 0x20);
    if (st == 0xfe || st == 0xfd || lh(l + 0xa6) != 0x23e) return 0;
    if (lbu(l + 0x52) == 10 && 10.0f <= moby_anim_time(l)) return 1;
    if (lbu(l + 0x20) == 10 || lbu(l + 0x20) == 0xd) return 1;
    return lw(lpv + 0x284) == -1 ? 1u : 0u;
}

// Whether a member of the group `group` (0xff: none) is a class 574 in state 9 or a class 573 in state 5, alive. Class 574's
// update, still generated, calls it too (glued to this port: kerwan.glue).
// orig: L3:0x002c6d98
uint32_t walker_group_busy_l3(uint32_t group) {
    if (group == 0xff) return 0;
    const uint32_t sp = ee_stack_alloc(0x20);
    uint32_t busy = 0;
    group_first(sp, static_cast<int32_t>(group), 0, 0);
    while (lwu(sp) != 0) {
        if (moby_alive_in_state(lwu(sp), 0x23e, 9u) != 0 || moby_alive_in_state(lwu(sp), 0x23d, 5u) != 0) {
            busy = 1;
            break;
        }
        group_next(sp, lwu(sp), 0, 0);
    }
    ee_stack_free(0x20);
    return busy;
}

// The walker's alert and hits: alerted (+0x38) or called by its leader, its reach is doubled for 240 frames; a hit (kinds
// 0x330000) that the hit reaction does not shrug off (its state 1) and that finds it not knocked off already alerts its group,
// costs its damage from its health, and knocks it off away from the hit's source (a jump arc, the move record's state 4 for
// one frame): state 21, a colour pulse.
// orig: L3:0x002c6a20
void walker_alert_and_hits_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t pv = lwu(m + 0x78);
    bool alert = lw(pv + 0x38) != 0;
    if (!alert) {
        const int32_t l = lw(pv + 0x248);
        alert = l != -1 && walker_leader_calls_l3(moby_at(l)) != 0;
    }
    if (alert) sw(pv + 0x240, static_cast<uint32_t>(frame_scale_i(0xf0)));
    const float reach = lwc1(pv + 0x22c);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x240))) == 0) swc1(pv + 0x230, fpu_add(reach, reach));
    else swc1(pv + 0x230, reach);
    int32_t state = 0;
    float damage = 0.0f;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    moby_hit_react_r(m, hit, pv + 0x20, 0, state, &damage, nullptr, 4);
    if (state != 1 && lbu(m + 0x20) != 0x15) {
        if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
        const uint32_t yaw = sp + 0x18;
        sw(pv + 0x144, 9);
        sb(pv + 0x15d, 0);
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), damage));
        swc1(pv + 0x130, fpu_mul(dt_sq(), 50.0f));
        const uint32_t from = lwu(hit + 0x20);
        swc1(yaw, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(from + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(from + 0x14))));
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        swc1(pv + 0x13c, fpu_mul(dt(), 10.0f));
        jump_arc_set(pv + 0x120, 4.5f, 2.0f);
        swc1(pv + 0x170, 5.0f);
        swc1(pv + 0x174, 12.0f);
        lq_sq(sp, hit + 0x10);
        vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                         *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
        move_state_start(m, pv + 0x120, 4, 1, 0, lwc1(yaw));
        sb(m + 0x20, 0x15);
        sb(pv + 0x117, 0xf0);
        color_pulse_start(m, pv + 0x110);
    }
    sb(m + 0xa4, 0xff);
    ee_stack_free(0x80);
}

// Class 573. Each frame: its target (anything in its reach +0x230), its joint scaled, its alert and hits, the rest +0x244
// counted. The state (+0x20):
// 0 its set-up (home, walker record, speeds, a random flip) -> 1, or 2 when it has a leader and no path (animation 0);
// 1 at its post (animation 1): on a path (+0x270) at its start, facing along it; now and then (1 in 14) it sets off along it
//   (-> 5) when its group is busy, the target is in its box, or near (+0x274) and level with it; without a path, now and then
//   it turns to a target near its home and level (-> 3);
// 2 with a leader: now and then, when the leader calls (or is gone), -> 4;
// 3 turns to the target, until its animation ends -> 4;
// 4 walks at the target, turning; close enough for a strike -> 6; the target beyond its reach from home, or far below or
//   above -> 7;
// 5 walks its path (the walker's step toward the point, the next point when it gets there), at its end -> 1 (or 2 with a
//   leader); now and then, when the leader calls, -> 4;
// 6 strikes: at its animation's frame 25, the target level, ahead and within 2, a blow (sub_00259bc8); the animation's end
//   -> 7 with a rest of 120 frames;
// 7 walks home -> 1 there; rested, a target near enough -> 6;
// 8 falls (enemy_physics) -> 4 on the ground; below z 2 it is lost (its bolts dropped);
// 9 carried -> 0 when let go;
// 21 knocked off: on landing it bursts (an explosion, two pieces of debris, a burst of class 0x77d) and its bolts drop;
//   below z 2 it is lost.
// Then: always drawn ones within 32 of the camera get their floor band; the colour pulse; +0xbc cleared.
// orig: L3:0x002c5bb8 (the jump table L3:0x1e3140 on the state; 10 .. 20 go to the frame's end)
void kerwan_class573_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t target = sp + 0x10;  // (the target record: its position, +0x40 the moby)
    const uint32_t ahead = sp + 0x60, out = sp + 0x70;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t home = pv + 0x200;
    target_select_any(m, target, lwc1(pv + 0x230));
    joint_node_scale(m, 0, pv + 0x280, u2f(0x40066666u));
    walker_alert_and_hits_l3(m);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x244)));
    auto anim_to = [&](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    auto walk_toward = [&](float x, float y, float accel, float damp, uint32_t out_rec) {
        yaw_turn_damped(m, pv + 0x220, heading_to(m, x, y), accel, u2f(0x3e99999au), damp);
        swc1(ahead, fpu_add(cos_vu(lwc1(m + 0x48)), cos_vu(lwc1(m + 0x48))));
        swc1(ahead + 4, fpu_add(sin_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48))));
        sw(ahead + 8, 0);
        walker_record_step(1.0f, m, pv + 0x180, ahead, out_rec);
    };
    auto strike_range = [&] {  // how near the target must be for a strike
        const float far = frame_scale_f(40.0f);
        const float near = frame_scale_f(10.0f);
        const float step = fpu_mul(lwc1(pv + 0x234), dt());
        return fpu_add(fpu_add(fpu_mul(fpu_mul(step, 0.5f), far), fpu_mul(step, near)), 1.5f);
    };
    auto go_strike = [&] {  // state 6, the strike's animation
        sb(m + 0x20, 6);
        if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, float_to_int(frame_scale_f(10.0f)));
    };
    auto set_off = [&] {  // -> 4 after a random wait, walking
        sw(pv + 0x23c, static_cast<uint32_t>(random_wait()));
        sb(m + 0x20, 4);
        anim_to(3, frame_scale_i(10));
    };
    // the leader calls, or there is none to wait for (none set, gone)
    auto leader_lets_go = [&] {
        const int32_t l = lw(pv + 0x248);
        if (l == -1) return true;
        const uint32_t lm = moby_at(l);
        if (lm == 0 || lbu(lm + 0x20) == 0xfe || lbu(lm + 0x20) == 0xfd) return true;
        return walker_leader_calls_l3(lm) != 0;
    };
    bool lost = false;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002c5c44)
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        lq_sq(home, m + 0x10);
        sb(m + 0x7f, 0x18);
        sb(m + 0x20, 1);
        swc1(pv + 0x20, 1.0f);
        sb(pv + 0x29, 0);
        sw(pv + 0x240, 0);
        sh(pv + 0x24, 1);
        walker_record_init_l1(pv + 0x180);
        const float walk = fpu_mul(lwc1(pv + 0x234), dt());
        swc1(pv + 0x1c0, fpu_mul(dt_sq(), u2f(0x41490fdbu)));
        swc1(pv + 0x1c4, fpu_mul(dt(), u2f(0x40490fdbu)));
        swc1(pv + 0x188, 2.0f);
        sw(pv + 0x180, 0x38d);
        swc1(pv + 0x1a4, walk);
        sw(pv + 0x1b8, lwu(pv + 0x1b8) | 0x20u);
        swc1(pv + 0x1a8, u2f(0x40061bbau));
        swc1(pv + 0x18c, 2.0f);
        swc1(pv + 0x1bc, fpu_mul(dt_sq(), u2f(0x40c90fdbu)));
        const float six = frame_scale_f(6.0f);
        swc1(pv + 0x264, 1.0f);
        swc1(pv + 0x1ac, fpu_mul(lwc1(pv + 0x1a4), six));
        sw(pv + 0x244, 0);
        sw(pv + 0x26c, 0);
        sw(pv + 0x260, 0);
        sb(pv + 0x58, 8);
        sb(pv + 0x5a, 7);
        if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        sw(pv + 0xd0, L3_ADDR(0x001618d0u));
        if (lw(pv + 0x248) != -1 && lw(pv + 0x270) == -1) {
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            sb(m + 0x20, 2);
            break;
        }
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
        break;
    }
    case 1: {  // (0x002c5de4)
        if (lw(pv + 0x270) != -1) {
            const uint32_t path = lvd::path(lw(pv + 0x270));
            lq_sq(m + 0x10, path + 0x10);
            swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
            bool go = rand_mod(0xe) == 0 && walker_group_busy_l3(lbu(m + 0x21)) != 0;
            if (!go) go = point_in_box(target, lw(pv + 0x27c)) != 0;
            if (!go) {
                if (!(vec3_distance(V(target), V(m + 0x10)) < lwc1(pv + 0x274))) break;
                if (!(abs_f(fpu_sub(lwc1(target + 8), lwc1(m + 0x18))) < 4.0f)) break;
            }
            sw(pv + 0x278, 0);
            sb(m + 0x20, 5);
            anim_to(3, frame_scale_i(10));
            break;
        }
        if (rand_mod(0xe) != 0) break;
        if (!(vec3_distance(V(target), V(home)) < lwc1(pv + 0x230))) break;
        if (!(abs_f(fpu_sub(lwc1(target + 8), lwc1(m + 0x18))) < 3.0f)) break;
        sb(m + 0x20, 3);
        anim_to(2, frame_scale_i(10));
        swc1(m + 0x58, u2f(0x3f60c7ceu));
        break;
    }
    case 2:  // (0x002c5f64)
        if (rand_mod(0xe) != 0) break;
        if (!leader_lets_go()) break;
        set_off();
        break;
    case 3:  // (0x002c6004)
        yaw_turn_damped(m, pv + 0x220, heading_to(m, lwc1(target), lwc1(target + 4)), u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
        if ((lbu(m + 0x70) & 2u) == 0) break;
        set_off();
        break;
    case 4: {  // (0x002c60b0)
        walk_toward(lwc1(target), lwc1(target + 4), u2f(0x3d4ccccdu), u2f(0x3e4ccccdu), out);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x23c))) != 0) sw(pv + 0x23c, static_cast<uint32_t>(random_wait()));
        const float d = vec2_distance(V(m + 0x10), V(target));
        if (d < strike_range()) {
            go_strike();
            break;
        }
        if (lwc1(pv + 0x230) < vec3_distance(V(target), V(home)) || 3.0f < abs_f(fpu_sub(lwc1(target + 8), lwc1(m + 0x18)))) sb(m + 0x20, 7);
        break;
    }
    case 5: {  // (0x002c6274)
        const uint32_t path = lvd::path(lw(pv + 0x270));
        int32_t nearest = 0;
        float dist = 0.0f;
        path_closest_point_v(path, V(m + 0x10), V(ahead), &nearest, &dist, 0, 999.0f, 5.0f, 0.0f);
        if (lw(pv + 0x278) < nearest) sw(pv + 0x278, static_cast<uint32_t>(nearest));
        const uint32_t goal = path + 0x10 + (static_cast<uint32_t>(lw(pv + 0x278)) << 4);
        if ((static_cast<uint32_t>(doomlet_259028(m, pv + 0x180, goal, pv + 0x40)) & 0x14u) != 0) {
            if (lw(pv + 0x278) < lw(path) - 1) {
                sw(pv + 0x278, lwu(pv + 0x278) + 1);
            } else {
                sw(pv + 0x270, 0xffffffffu);
                sb(m + 0x20, 1);
                anim_to(1, frame_scale_i(10));
                if (lw(pv + 0x248) != -1) {
                    anim_to(0, frame_scale_i(10));
                    sb(m + 0x20, 2);
                }
            }
        }
        if (rand_mod(0xe) != 0) break;
        if (!leader_lets_go()) break;
        sw(pv + 0x270, 0xffffffffu);
        set_off();
        break;
    }
    case 6: {  // (0x002c643c)
        walk_toward(lwc1(target), lwc1(target + 4), u2f(0x3d4ccccdu), u2f(0x3e4ccccdu), pv + 0x40);
        if (lbu(m + 0x52) == 5) approach_f(pv + 0x1a4, 0.0f, fpu_div(fpu_mul(lwc1(pv + 0x234), dt()), frame_scale_f(40.0f)));
        bool struck = false;
        if (abs_f(fpu_sub(moby_anim_time(m), 25.0f)) <= 0.25f && abs_f(fpu_sub(lwc1(target + 8), lwc1(m + 0x18))) < 1.0f &&
            angle_dist(heading_to(m, lwc1(target), lwc1(target + 4)), lwc1(m + 0x48)) < u2f(0x3e860a92u) && lwu(target + 0x40) != 0 &&
            vec2_distance(V(m + 0x10), V(target)) < 2.0f) {
            const uint32_t push = sp + 0x70, at = sp + 0x80;
            swc1(push, fpu_mul(cos_vu(lwc1(m + 0x48)), u2f(0x3e4ccccdu)));
            swc1(push + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), u2f(0x3e4ccccdu)));
            sw(push + 8, 0);
            lq_sq(at, target);
            swc1(at + 8, fpu_add(lwc1(at + 8), 0.75f));
            sub_00259bc8(lwu(target + 0x40), m, 1, at, push, 1.0f);
            struck = true;
        }
        if (struck || (lbu(m + 0x70) & 2u) == 0) break;
        swc1(pv + 0x1a4, fpu_mul(lwc1(pv + 0x234), dt()));
        sb(m + 0x20, 7);
        anim_to(3, frame_scale_i(3));
        sw(pv + 0x244, static_cast<uint32_t>(frame_scale_i(0x78)));
        break;
    }
    case 7: {  // (0x002c669c)
        walk_toward(lwc1(home), lwc1(home + 4), u2f(0x3ca3d70au), u2f(0x3dcccccdu), out);
        if (vec2_distance(V(m + 0x10), V(home)) < 1.0f) {
            sb(m + 0x20, 1);
            anim_to(1, frame_scale_i(3));
            break;
        }
        if (lw(pv + 0x244) != 0) break;
        if (vec2_distance(V(m + 0x10), V(target)) < strike_range()) go_strike();
        break;
    }
    case 8:  // (0x002c6828)
        if ((enemy_physics(m, pv + 0x120) & 0x60u) != 0) {
            sb(m + 0x20, 4);
            anim_to(3, frame_scale_i(10));
        } else if (lwc1(m + 0x18) < 2.0f) {
            lost = true;
        }
        break;
    case 9:  // (0x002c6874)
        if (carry_update(m, pv + 0x120) != 0) {
            sb(m + 0x20, 0);
            sh(pv + 0xc8, 0);
        }
        break;
    case 21: {  // (0x002c6894)
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        if ((enemy_physics(m, pv + 0x120) & 0x60u) != 0) {
            const uint32_t zero = OV(0x0015f580);
            explosion_burst(m, m + 0x10, -1, 0.5f, 13.0f);
            debris_spawn(m, 0x6ce, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            debris_spawn(m, 0x6cf, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            debris_burst(m, 0x77d, 1, 0x77d, 1, 3, 2);
            lost = true;
        } else if (lwc1(m + 0x18) < 2.0f) {
            lost = true;
        }
        break;
    }
    default:  // 10 .. 20 (0x002c69ac) and beyond the table
        break;
    }
    if (lost) {
        moby_drop_bolts(m, 0, -1);
        moby_retire(m);
        ee_stack_free(0x110);
        return;
    }
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 32.0f) moby_floor_band(m);
    color_pulse_update(m, pv + 0x110);
    sb(m + 0xbc, 0);
    ee_stack_free(0x110);
}

}  // namespace rac1
