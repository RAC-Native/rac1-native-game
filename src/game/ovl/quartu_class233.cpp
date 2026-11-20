// Quartu (level 15) class 233, ported by hand from the level's own code: a flying craft of the factory, Quartu's build of
// the Blarg craft (class 631 on Kerwan / Umbris, shared_class631.cpp) with its own states. It carries a class 1371 piece,
// waits (hidden while the hero is not Giant Clank, when its flag +0x1a4 asks for it) for its group's mates to be gone or
// the hero in its box, flies its route path, then keeps near its target inside its zone polygon (away from its mates),
// turns its turret toward the target (or the class 848 charge it dropped), drops charges, and is destroyed with a blast,
// its piece thrown off; state 0x40 clears its moby's saved bits and starts it again when the hero is not Giant Clank.
//
// The pvars: the hit record +0x20.. (the health +0x20, the alive flag +0x2e), the colour pulse +0x60.., the turret's two
// joint records +0xd0 / +0x110 (their quaternions +0xe0 / +0x120), the velocity +0x150, the piece +0x160, the charge in
// flight +0x164, the fire timer +0x168 and the pause timer +0x16a (halves), the bob's phase +0x16c, the speed +0x170, the
// tilts' rates +0x174 / +0x178, the yaw's rate +0x17c, the turret's yaw +0x180 and pitch +0x188 (their rates +0x184 /
// +0x18c), the group +0x190, the zone polygon (a path index) +0x194, the target's ranges +0x198 (waiting) / +0x19c and the
// one in use +0x1a0, the Giant-Clank-only flag +0x1a4, the flight height +0x1a8, the route path +0x1ac, the start box +0x1b0.
// The original's order of calls and float operations is kept. Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1371.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {

constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t HERO_MOBY = 0x001413d0;
constexpr uint32_t HERO_FORM = 0x001413f4;  // (the hero moby's +0x24: 2 while the hero is Giant Clank)
constexpr int32_t CHARGE_CLASS = 0x350;     // class 848, the charge it drops
constexpr uint32_t APPLIED = 0x45afdf66;    // (a hit record's "applied" mark, +0x1c)

float frame_time() { return lwc1(0x0015ed6c); }
float frame_time2() { return lwc1(0x0015ed70); }
bool hero_is_giant() { return lbu(HERO_FORM) == 2; }

// hidden and still (its collision off), its piece too
void hide(uint32_t m, uint32_t pv) {
    sw(m + 0x94, 0);
    sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
    sb(pv + 0x58, 0);
    const uint32_t piece = lwu(pv + 0x160);
    if (piece != 0) sh(piece + 0x34, lhu(piece + 0x34) | 0x41);
}

// shown and solid (its collision from its class), its piece shown, and away: its route, or straight to its zone
void start(uint32_t m, uint32_t pv) {
    sh(m + 0x34, (lhu(m + 0x34) & 0xffbe) | 0x1000);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    sb(pv + 0x58, 0x19);
    sb(m + 0x20, lw(pv + 0x1ac) != -1 ? 2 : 3);
    const uint32_t piece = lwu(pv + 0x160);
    if (piece != 0) sh(piece + 0x34, lhu(piece + 0x34) & 0xffbe);
}

// The speed toward a goal `dist` away: braking (to 0) within its stopping distance at 10 units/s^2, else gaining (to 10 a
// second).
void speed_toward(uint32_t pv, float dist) {
    const float step = fpu_mul(frame_time2(), 10.0f);
    const float speed = lwc1(pv + 0x170);
    if (dist <= fpu_div(fpu_mul(speed, speed), fpu_add(step, step))) {
        const float s = fpu_sub(speed, step);
        swc1(pv + 0x170, s);
        if (s < 0.0f) swc1(pv + 0x170, 0.0f);
    } else {
        const float s = fpu_add(speed, step);
        swc1(pv + 0x170, s);
        const float fastest = fpu_mul(frame_time(), 10.0f);
        if (fastest < s) swc1(pv + 0x170, fastest);
    }
}

// The yaw toward the point (x, y)
void yaw_toward(uint32_t m, uint32_t pv, float dx, float dy) {
    swc1(m + 0x48, angle_seek_rate(pv + 0x17c, lwc1(m + 0x48), atan2_f(dx, dy), u2f(0x3c23d70a), u2f(0x3e99999a), u2f(0x3dcccccd)));
}

// The craft tilted with its move `dir` (an EE vector): its pitch and roll toward the speed's share along and across its yaw
void tilt(uint32_t m, uint32_t pv, uint32_t dir) {
    const float third = u2f(0x3f860a92);
    const float rel = angle_sub(atan2_f(lwc1(dir), lwc1(dir + 4)), lwc1(m + 0x48));
    const float pitch = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x170), u2f(0x3eb2b8c2)), cos_vu(rel)), fpu_mul(frame_time(), 10.0f));
    const float s = sin_vu(rel);
    const float roll = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x170), u2f(0xbeb2b8c2)), s), fpu_mul(frame_time(), 10.0f));
    swc1(m + 0x44, angle_seek_rate(pv + 0x178, lwc1(m + 0x44), pitch, fpu_mul(frame_time2(), u2f(0x3f060a92)),
                                   fpu_mul(frame_time2(), third), fpu_mul(frame_time(), u2f(0x3f490fdb))));
    swc1(m + 0x40, angle_seek_rate(pv + 0x174, lwc1(m + 0x40), roll, fpu_mul(frame_time2(), third),
                                   fpu_mul(frame_time2(), u2f(0x40060a92)), fpu_mul(frame_time(), u2f(0x3fc90fdb))));
}

// The turret (joint records +0xd0 / +0x110) turned toward `goal` (an EE vector), in the craft's own frame
void aim_turret(uint32_t m, uint32_t pv, uint32_t sp, uint32_t goal) {
    const uint32_t from = sp + 0xc0, rel = sp + 0xd0, inv = sp + 0x80;
    const float two_pi = u2f(0x40c90fdb);
    moby_chain_point_v(m, 0, V(from));
    vec3_sub(V(sp + 0xe0), V(goal), V(from));
    lq_sq(rel, sp + 0xe0);
    mat3_transpose(M(inv), M(m + 0xc0));
    mat4_transform(V(rel), V(rel), M(inv));
    swc1(pv + 0x180, angle_seek_rate(pv + 0x184, lwc1(pv + 0x180), atan2_f(lwc1(rel), lwc1(rel + 4)), fpu_mul(frame_time2(), two_pi),
                                     fpu_mul(frame_time2(), u2f(0x41490fdb)), fpu_mul(frame_time(), two_pi)));
    const float pitch = -atan2_f(vec2_length(V(rel)), lwc1(rel + 8));
    swc1(pv + 0x188, angle_seek_rate(pv + 0x18c, lwc1(pv + 0x188), pitch, fpu_mul(frame_time2(), u2f(0x40490fdb)),
                                     fpu_mul(frame_time2(), two_pi), fpu_mul(frame_time(), u2f(0x3fc90fdb))));
    quat_from_axis_angle(V(pv + 0xe0), 2, lwc1(pv + 0x180));
    quat_from_axis_angle(V(pv + 0x120), 2, lwc1(pv + 0x188));
}

}  // namespace

// The move by `vel` (an EE vector) with a sphere of radius `radius` `lift` above the position: up to six pushes out of
// what it meets steeper than 30 degrees (each also lowering it by `lift`); `vel` is left the move actually done. Bit 0 of
// the result: pushed. (`flag` << 1 & 0x20 joins the collision's flags.)
// orig: L15:0x00247e90
uint32_t qua_247e90_l15(uint32_t m, uint32_t vel, uint32_t flag, float lift, float radius) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t old = sp, probe = sp + 0x10;
    uint32_t pushed = 0;
    lq_sq(old, m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(vel));
    const uint32_t flags = (flag << 1) & 0x20;
    for (int i = 0; i < 6; ++i) {
        lq_sq(probe, m + 0x10);
        swc1(probe + 8, fpu_add(lwc1(probe + 8), lift));
        if (sphere_collide(probe, radius, flags | 4, m) == 0) break;
        const Vec4 normal = coll_hit_normal();
        if (u2f(0x3f060a92) < atan2_f(normal.z, vec2_length(normal))) {
            V(m + 0x10) = coll_hit_pushed();
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lift));
            pushed |= 1;
        }
    }
    vec3_sub(V(vel), V(m + 0x10), V(old));
    ee_stack_free(0x90);
    return pushed;
}

// The flight toward `goal` at the flight height, the yaw toward `look`: the speed braking or gaining, the velocity +0x150
// eased (0.9 of the last, plus a tenth of the speed toward the goal), the move done against the walls, the craft tilted.
// orig: L15:0x002c4f88
void qua_2c4f88_l15(uint32_t m, const Vec4& goal_in, const Vec4& look_in) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t goal = sp, look = sp + 0x10, step = sp + 0x20, old = sp + 0x30, tmp = sp + 0x40;
    V(goal) = goal_in;
    V(look) = look_in;
    lq_sq(old, m + 0x10);
    speed_toward(pv, vec3_distance(V(m + 0x10), V(goal)));
    vec3_sub(V(tmp), V(goal), V(m + 0x10));
    lq_sq(step, tmp);
    vec3_set_length(V(step), V(step), fpu_mul(lwc1(pv + 0x170), u2f(0x3dcccccd)));
    vec3_scale(V(tmp), V(pv + 0x150), u2f(0x3f666666));
    lq_sq(pv + 0x150, tmp);
    vec3_add_inplace(V(pv + 0x150), V(step));
    qua_247e90_l15(m, pv + 0x150, 0, 1.0f, 2.0f);
    swc1(m + 0x18, lwc1(pv + 0x1a8));
    yaw_toward(m, pv, fpu_sub(lwc1(look), lwc1(m + 0x10)), fpu_sub(lwc1(look + 4), lwc1(m + 0x14)));
    vec3_sub(V(tmp), V(m + 0x10), V(old));
    lq_sq(pv + 0x150, tmp);
    tilt(m, pv, pv + 0x150);
    ee_stack_free(0x90);
}

// Its place near the target (an EE target record: its point +0): away from the mates of its class within 10 (weight 6
// each), on a ring around the target (15 out, 10 out when no mate is nearer the target than it), all averaged and added
// to its position; its own position when that is within 1. (The original also returns the yaw toward it: no caller reads
// it.)
// orig: L15:0x002c52b8
void qua_2c52b8_l15(uint32_t m, uint32_t out, uint32_t target) {
    const Vec4 aim = V(target);
    const float to_target = vec2_distance(V(m + 0x10), aim);
    bool nearest = true;
    float weight = 0.0f;
    vec_clear(V(out));
    for (uint32_t mate = lwu(L15_ADDR(0x0015ffe4u)); mate != 0; mate = lwu(mate + 0x28)) {
        if (mate == m || lh(mate + 0xa6) != lh(m + 0xa6)) continue;
        const uint8_t st = lbu(mate + 0x20);
        if (st == 1 || st == 0x40) continue;
        if (vec2_distance(V(mate + 0x10), V(m + 0x10)) < 10.0f) {
            Vec4 away{};
            weight = fpu_add(weight, 6.0f);
            vec3_sub(away, V(m + 0x10), V(mate + 0x10));
            vec3_scale(away, away, 6.0f);
            vec3_add(V(out), V(out), away);
        }
        if (vec2_distance(V(mate + 0x10), aim) < to_target) nearest = false;
    }
    Vec4 ring{};
    vec3_sub(ring, V(m + 0x10), aim);
    const float radius = nearest ? 10.0f : 15.0f;
    float pull = 0.0f;
    if (to_target < fpu_sub(radius, 0.5f)) {
        pull = radius;
    } else if (fpu_add(radius, 0.5f) < to_target) {
        pull = -radius;
    }
    weight = fpu_add(weight, 10.0f);
    vec3_set_length(ring, ring, fpu_mul(pull, 10.0f));
    vec3_add(V(out), V(out), ring);
    vec3_scale(V(out), V(out), fpu_div(1.0f, weight));
    vec3_add(V(out), V(out), V(m + 0x10));
    if (vec2_distance(V(m + 0x10), V(out)) < 1.0f) lq_sq(out, m + 0x10);
}

// Its hits (kinds 0x330000): the damage off its health; destroyed (state 7) when the reaction says so or the health is
// gone; else its colour pulse, and a shot that pushes (its record applied) throws it off: tilted away, slowed, a pause
// (state 6) before the state it was in (+0xbc).
// orig: L15:0x002c6900
void qua_2c6900_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    swc1(sp + 4, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t st = lw(sp);
    float dmg = lwc1(sp + 4);
    const int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    sw(sp, static_cast<uint32_t>(st));
    swc1(sp + 4, dmg);
    const uint8_t state = lbu(m + 0x20);
    if (lw(sp) != 1 && state != 7 && state != 0x40) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(sp + 4));
        swc1(pv + 0x20, health);
        if (react == 1 || health <= 0.0f) {
            sb(m + 0x20, 7);
        } else if (react != 0xb) {
            sb(pv + 0x67, 0xb4);
            color_pulse_start(m, pv + 0x60);
            if (lwc1(hit + 0x1c) == u2f(APPLIED) && lbu(m + 0x20) != 6) {
                const float rel = angle_sub(atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(hit)), fpu_sub(lwc1(m + 0x14), lwc1(hit + 4))), lwc1(m + 0x48));
                const float pitch = fpu_mul(cos_vu(rel), u2f(0x3db2b8c2));
                const float roll = fpu_mul(sin_vu(rel), u2f(0xbeb2b8c2));
                if (abs_f(lwc1(m + 0x40)) < abs_f(roll)) {
                    swc1(m + 0x40, roll);
                    swc1(pv + 0x174, 0.0f);
                }
                if (abs_f(lwc1(m + 0x44)) < abs_f(pitch)) {
                    swc1(m + 0x44, pitch);
                    swc1(pv + 0x178, 0.0f);
                }
                swc1(pv + 0x170, fpu_mul(frame_time(), 4.0f));
                sh(pv + 0x16a, static_cast<uint32_t>(frame_scale_i(0x3c)));
                sb(m + 0xbc, lbu(m + 0x20));
                sb(m + 0x20, 6);
            }
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0x80);
}

// orig: L15:0x002c5630
void qua_class233_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x1c0);
    const uint32_t start_pos = sp + 0x20;
    const uint32_t target = sp + 0x30;  // the target record (target_select's): its point +0, +0x40 the moby, +0x44 the result
    const uint32_t target_moby = target + 0x40, status = target + 0x44;  // (status: 0 the hero, 1 another moby, 2 none)
    const uint32_t pos = m + 0x10;
    lq_sq(start_pos, pos);
    if (lwu(pv + 0x160) == 0) sw(pv + 0x160, class1371_spawn(m));

    const auto done = [sp] { ee_stack_free(0x1c0); };
    {
        const uint8_t st = lbu(m + 0x20);
        if (st != 0 && st != 0x40 && lw(pv + 0x1a4) != 0 && !hero_is_giant()) {
            hide(m, pv);
            sb(m + 0x20, 1);
            done();
            return;
        }
    }
    qua_2c6900_l15(m);

    // the target: the hero (or a moby) within its range, inside its zone polygon when it has one
    sw(status, 0);
    if (lbu(m + 0x20) == 1) {
        swc1(pv + 0x1a0, lwc1(pv + 0x198));
        target_select_any(m, target, lwc1(pv + 0x1a0));
    } else if (lw(pv + 0x194) == -1) {
        swc1(pv + 0x1a0, lwc1(pv + 0x19c));
        target_select_any(m, target, lwc1(pv + 0x1a0));
    } else {
        const float range = lwc1(pv + 0x19c);
        swc1(pv + 0x1a0, range);
        const uint32_t zone = lvd::path(lw(pv + 0x194));
        target_select(m, target, 0, 0, zone + 0x10, lw(zone), range);
    }
    if (lwc1(pv + 0x1a0) < vec3_distance(V(pos), V(target))) sw(status, 2);
    if (lwu(target_moby) == 0) sw(target_moby, lwu(HERO_MOBY));
    const uint32_t chased = lwu(target_moby);
    if (lbu(m + 0x20) != 0 && lbu(pv + 0x2e) != 1) {  // (its hit record dead: gone with its piece)
        if (lwu(pv + 0x160) != 0) moby_retire(lwu(pv + 0x160));
        moby_retire(m);
        done();
        return;
    }

    const uint32_t step = sp + 0x80, near = sp + 0x90;
    const uint32_t idx = sp + 0xf0, frac = sp + 0xf4;
    switch (lbu(m + 0x20)) {  // (the table L15:0x1e2e10, 65 entries; 8 .. 63: the tail alone)
        case 0: {  // (0x002c5834) set up: its numbers, its route's lengths and start, the turret's joints; hidden
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 7.5f));
            sb(m + 0x7f, 0x20);
            swc1(pv + 0x1a8, lwc1(m + 0x18));
            swc1(pv + 0x20, 3.0f);
            sh(pv + 0x24, static_cast<uint32_t>(float_to_int(3.0f)));
            sb(pv + 0x28, 2);
            sb(pv + 0x58, 0x19);
            swc1(pv + 0x30, 1.0f);
            sb(pv + 0x59, 1);
            sb(pv + 0x5a, 6);
            sb(pv + 0x2e, 1);
            sb(m + 0x30, 0xff);
            sh(m + 0x32, 0x80);
            if (lw(pv + 0x1ac) != -1) {
                const uint32_t route = lvd::path(lw(pv + 0x1ac));
                for (int32_t i = 0; i < lw(route) - 2; ++i) {  // each segment's length into its first point's w
                    const uint32_t p = route + 0x10 + static_cast<uint32_t>(i) * 0x10;
                    swc1(p + 0xc, vec3_distance(V(p), V(p + 0x10)));
                }
                lq_sq(pos, route + 0x10);
            }
            sb(m + 0x20, 1);
            sw(pv + 0x170, 0);
            sw(pv + 0x174, 0);
            sw(pv + 0x178, 0);
            sw(pv + 0x17c, 0);
            swc1(pv + 0x19c, 60.0f);
            sub_0024f590(m, 0, pv + 0xd0);
            sub_0024f590(m, 1, pv + 0x110);
            swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), 0.5f));
            const uint32_t piece = lwu(pv + 0x160);
            if (piece != 0) {
                sb(piece + 0x30, 0);
                sh(piece + 0x34, lhu(piece + 0x34) & 0xffbe);
                swc1(piece + 0x2c, fpu_mul(lwc1(piece + 0x2c), 0.5f));
            }
            hide(m, pv);
            break;
        }
        case 1: {  // (0x002c59f0) waiting: for its group's mates to be gone, or the hero in its box (or no box), a target near
            if (lw(pv + 0x1a4) != 0 && !hero_is_giant()) {
                hide(m, pv);
                done();
                return;
            }
            const int32_t group = lw(pv + 0x190);
            if (group != -1) {
                if (moby_group_count_l1(group, 0x40) == 0) start(m, pv);
                break;
            }
            const int32_t box = lw(pv + 0x1b0);
            const bool in_box = point_in_box(HERO_POS, box) != 0;
            if (lw(status) != 2 && (in_box || box == -1)) start(m, pv);
            break;
        }
        case 2: {  // (0x002c5b48) its route: two units ahead of the nearest point; into its zone (or at the route's end) to state 5 / 4
            const uint32_t route = lvd::path(lw(pv + 0x1ac));
            int32_t i = lw(idx);
            float t = lwc1(frac);
            path_closest_point_v(route, V(pos), V(near), &i, &t, 0, 999.0f, 5.0f, 0.0f);
            sw(idx, static_cast<uint32_t>(i));
            swc1(frac, t);
            const float ahead = fpu_add(fpu_add(fpu_mul(int_to_float(lw(idx)), lwc1(route + 0x1c)), lwc1(frac)), 2.0f);
            speed_toward(pv, vec3_distance(V(pos), V(route + (lwu(route) << 4))));
            const int32_t at = float_to_int(fpu_div(ahead, lwc1(route + 0x1c)));
            sw(idx, static_cast<uint32_t>(at));
            const float len = lwc1(route + 0x1c);
            swc1(frac, fpu_div(fpu_sub(ahead, fpu_mul(int_to_float(at), len)), len));
            if (!(lw(idx) < lw(route) - 1)) {
                sw(idx, static_cast<uint32_t>(lw(route) - 2));
                swc1(frac, 1.0f);
            }
            {
                const uint32_t p = route + (lwu(idx) << 4);
                vec3_sub(V(near), V(p + 0x20), V(p + 0x10));
                vec3_scale(V(near), V(near), lwc1(frac));
                vec3_add(V(near), V(near), V(route + (lwu(idx) << 4) + 0x10));
            }
            bool arrived = vec2_distance(V(route + (lwu(route) << 4)), V(pos)) < 1.0f;
            if (!arrived) {
                const uint32_t zone = lvd::path(lw(pv + 0x194));
                arrived = point_in_polygon(pos, zone + 0x10, lw(zone)) != 0;
            }
            if (arrived) {
                if (lw(status) == 2) {
                    sb(m + 0x20, 4);
                } else {
                    sb(m + 0x20, 5);
                    sh(pv + 0x168, 0);
                }
            }
            const int32_t next = lw(idx) + 1;
            if (next < lw(route) - 8) {
                const uint32_t p = route + (static_cast<uint32_t>(next) << 4);
                yaw_toward(m, pv, fpu_sub(lwc1(p + 0x10), lwc1(pos)), fpu_sub(lwc1(p + 0x14), lwc1(pos + 4)));
            } else {
                yaw_toward(m, pv, fpu_sub(lwc1(target), lwc1(pos)), fpu_sub(lwc1(target + 4), lwc1(pos + 4)));
            }
            vec3_sub(V(step), V(near), V(pos));
            float move = vec3_length(V(step));
            if (lwc1(pv + 0x170) < move) move = lwc1(pv + 0x170);
            vec3_set_length(V(step), V(step), move);
            vec3_add(V(pos), V(pos), V(step));
            tilt(m, pv, step);
            break;
        }
        case 3: {  // (0x002c5e60) into its zone: toward the zone's last corner (within 9999.999), at the target's height + 7.5
            const uint32_t zone = lvd::path(lw(pv + 0x194));
            lq_sq(step, chased + 0x10);
            for (int32_t k = 0; k < lw(zone); ++k) {
                const uint32_t p = zone + 0x10 + static_cast<uint32_t>(k) * 0x10;
                if (vec2_distance(V(p), V(pos)) < u2f(0x461c3fff)) lq_sq(step, p);  // (the bound is never lowered)
            }
            swc1(step + 8, fpu_add(lwc1(chased + 0x18), 7.5f));
            lq_sq(near, chased + 0x10);
            qua_2c4f88_l15(m, V(step), V(near));
            if (vec2_distance(V(step), V(pos)) < 1.0f || point_in_polygon(pos, zone + 0x10, lw(zone)) != 0) {
                if (lw(status) == 2) {
                    sb(m + 0x20, 4);
                } else {
                    sb(m + 0x20, 5);
                    sh(pv + 0x168, 0);
                }
            }
            break;
        }
        case 4: {  // (0x002c5f68) holding: the yaw on the target; a target in range: inside its zone to state 5, else 3
            lq_sq(step, pos);
            const uint32_t zone = lvd::path(lw(pv + 0x194));
            lq_sq(near, chased + 0x10);
            qua_2c4f88_l15(m, V(step), V(near));
            if (lw(status) == 2) break;
            if (point_in_polygon(pos, zone + 0x10, lw(zone)) != 0) {
                sb(m + 0x20, 5);
                sh(pv + 0x168, 0);
            } else {
                sb(m + 0x20, 3);
            }
            break;
        }
        case 5: {  // (0x002c5fdc) attacking: near the target, away from its mates; a charge every second when none is in flight
            const uint32_t gun = sp + 0x90, vel = sp + 0xa0, aim = sp + 0xb0;
            moby_chain_point_v(m, 0, V(gun));
            if (lw(status) == 2) {
                lq_sq(vel, pos);
                lq_sq(aim, chased + 0x10);
                qua_2c4f88_l15(m, V(vel), V(aim));
                sb(m + 0x20, 4);
                break;
            }
            qua_2c52b8_l15(m, step, target);
            lq_sq(vel, chased + 0x10);
            qua_2c4f88_l15(m, V(step), V(vel));
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x168))) == 0) break;
            if (lwu(pv + 0x164) != 0) break;
            sh(pv + 0x168, static_cast<uint32_t>(frame_scale_i(0x3c)));
            lq_sq(aim, target);
            vec3_sub(V(vel), V(aim), V(gun));
            vec2_set_length(V(vel), V(vel), fpu_mul(frame_time(), 6.0f));
            sw(vel + 8, 0);
            const uint32_t life = static_cast<uint32_t>(frame_scale_i(0xf0));
            sw(pv + 0x164, class848_spawn(vel, gun, m, life, lwc1(target + 8)));
            break;
        }
        case 6: {  // (0x002c60e8) thrown off: drifting back (slowing by 4 units/s^2) until the pause is over
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x16a))) != 0) {
                sb(m + 0x20, lbu(m + 0xbc));
                break;
            }
            const float s = fpu_sub(lwc1(pv + 0x170), fpu_mul(frame_time2(), 4.0f));
            swc1(pv + 0x170, s);
            if (s < 0.0f) swc1(pv + 0x170, 0.0f);
            swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), -lwc1(pv + 0x170)));
            swc1(step + 8, 0.0f);
            swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), -lwc1(pv + 0x170)));
            vec3_add(V(pos), V(pos), V(step));
            tilt(m, pv, step);
            break;
        }
        case 7: {  // (0x002c6294) destroyed: its piece thrown, a blast, bolts, the turret's joints let go; state 0x40
            vec_clear(V(step));
            // (the original also turns four points 1.5 around its joint 4 by a random start angle: nothing reads them, but
            // the random draw is kept; its joints 4 and 5 are looked up the same)
            rand_angle();
            moby_chain_point_v(m, 4, V(near));
            moby_chain_point_v(m, 5, V(sp + 0xa0));
            const uint32_t piece = lwu(pv + 0x160);
            if (piece != 0) {
                class1371_throw(0, piece);
                sw(pv + 0x160, 0);
            }
            explosion_effect(m, step, pos, 0x14, 3, 4, 2, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);
            moby_drop_bolts(m, 0x200, -1);
            sub_0024f608(m, pv + 0xd0);
            sub_0024f608(m, pv + 0x110);
            sb(m + 0x20, 0x40);
            done();
            return;
        }
        case 64: {  // (0x002c6404) gone: its moby's saved bits cleared; when the hero is not Giant Clank, set up again (state 0)
            hide(m, pv);
            const int32_t uid = lh(m + 0xb2);
            sb(L15_ADDR(0x001babd0u) + static_cast<uint32_t>(uid) + 0x454, 0);
            sb(L15_ADDR(0x001bb830u) + static_cast<uint32_t>(uid) + 0x454, 0);
            const uint32_t bit = 1u << (uid & 0x1f);
            const uint32_t word = static_cast<uint32_t>((uid >> 5) << 2);
            const uint32_t saved = 0x0014c190u + (lwu(0x0015ed84u) << 8) + word;
            sw(saved, lwu(saved) & ~bit);
            const uint32_t flags = L15_ADDR(0x001baad0u) + word;
            sw(flags, lwu(flags) & ~bit);
            if (hero_is_giant()) {
                done();
                return;
            }
            swc1(m + 0x2c, fpu_add(lwc1(m + 0x2c), lwc1(m + 0x2c)));
            sb(m + 0x20, 0);
            swc1(m + 0x18, fpu_sub(lwc1(pv + 0x1a8), 7.5f));
            done();
            return;
        }
        default:  // (0x002c6510)
            break;
    }

    // the tail (0x002c6510): the floor band near Clank, the height kept within 10 a second of the frame's start, the bob,
    // the piece carried, the turret on the charge in flight (or on the target, the charge then forgotten)
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L15_ADDR(0x001673c0u))) < 40.0f) moby_floor_band(m);
    if (lbu(m + 0x20) != 1) {
        const float z0 = lwc1(start_pos + 8);
        const float lim = fpu_mul(frame_time(), 10.0f);
        if (lim < abs_f(fpu_sub(lwc1(m + 0x18), z0))) swc1(m + 0x18, z0 < lwc1(m + 0x18) ? fpu_add(z0, lim) : fpu_sub(z0, lim));
    }
    {
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_mul(lwc1(L15_ADDR(0x00161a0cu)), cos_vu(lwc1(pv + 0x16c)))));
        const float period = frame_scale_f(fpu_mul(lwc1(L15_ADDR(0x00161a08u)), 60.0f));
        const float phase = angle_add(lwc1(pv + 0x16c), fpu_div(u2f(0x40c90fdb), period));
        swc1(pv + 0x16c, phase);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(L15_ADDR(0x00161a0cu)), cos_vu(phase))));
    }
    if (lwu(pv + 0x160) != 0) {
        lq_sq(lwu(pv + 0x160) + 0x10, pos);
        lq_sq(lwu(pv + 0x160) + 0x40, m + 0x40);
        moby_anim_advance(lwu(pv + 0x160));
        moby_update_transform(lwu(pv + 0x160));
        const uint32_t piece = lwu(pv + 0x160);
        sh(piece + 0x34, lhu(piece + 0x34) | 0x6);
    }
    const uint32_t charge = lwu(pv + 0x164);
    if (charge != 0 && lh(charge + 0xa6) == CHARGE_CLASS && lbu(charge + 0x20) != 0xfe && lbu(charge + 0x20) != 0xfd) {
        aim_turret(m, pv, sp, charge + 0x10);
    } else {
        aim_turret(m, pv, sp, target);
        sw(pv + 0x164, 0);
    }
    done();
}

}  // namespace rac1
