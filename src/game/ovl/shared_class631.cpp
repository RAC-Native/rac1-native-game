// Class 631 of Kerwan (L3) and Umbris (L7), a flying craft: one port of the two levels' copies, which are the same code
// relinked (word by word: only the relocations differ; the state table's entries the same offsets). It waits at its path's
// start for the hero in its zone, flies its route (a path, then two paths it goes to and fro between, pausing at their
// ends), turns its turret toward its target and drops class 848's charges at it, carries a rider (class 574, by index), and
// is destroyed with a blast and three pieces. Its addresses are the base ELF's (bare: the hero 0x13f3d0, the level number
// 0x15ed84, the per-level flag bytes 0x14c050, the sound slots 0x13e550.., the frame time 0x15ed6c / its square 0x15ed70 /
// the time step 0x15ed64), the engine's (OV: the camera 0x166dc0, the moby table 0x15ffd8, the path table 0x1b04b0, the
// moby bits 0x1ba4d0, the zero vector 0x15f580) or the level's own: three floats (tuning()). Header shared_class631.h.
//
// The pvars: the hit record +0x20.. (the health +0x20), the colour pulse +0x60.., its paths' followers +0x120 and +0x150
// (each: the point +0, the path +0x10), the start path +0x130 (its index +0x134), the second follower's path +0x160 (its
// index +0x164), the turret's two joint records +0x180 / +0x1c0 (their quaternions +0x190 / +0x1d0), the yaw's rate +0x200,
// the fire timer +0x204 and the pause timer +0x206 (halves), the fire zone +0x208 and the route zones +0x20c / +0x244, the
// bob's phase +0x210, the speed +0x214, the tilts' rates +0x218 / +0x21c, the turret's yaw +0x220 and pitch +0x228 (their
// rates +0x224 / +0x22c), the route's two paths +0x230 / +0x234, the rider +0x238 (an index, -1 none), the follower in use
// +0x23c, the charge in flight +0x240, the start zone +0x248, the first path +0x24c, the sound slot +0x250, the alert
// timer +0x254.
#include <game/ovl/shared_class631.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t RIDER_CLASS = 0x23e;  // class 574
constexpr uint32_t CHARGE_CLASS = 0x350;  // class 848, the charge it drops
constexpr uint32_t APPLIED = 0x45afdf66;  // (a hit record's "applied" mark, +0x1c)

// The class's numbers (the level's own block): +0 the bob's period (seconds), +4 its height, +8 the slowest speed (a
// second's worth)
uint32_t tuning(uint32_t o) {
    if (on_level(UMBRIS)) return L7_ADDR(0x00161934) + o;
    return L3_ADDR(0x00161994) + o;
}

float frame_time() { return lwc1(0x0015ed6c); }
float frame_time2() { return lwc1(0x0015ed70); }
uint32_t level_path(int32_t i) { return lwu(OV(0x001b04b0) + (static_cast<uint32_t>(i) << 2)); }
uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// each segment's length into its first point's w (+0x1c), up to the path's last but one
void segment_lengths(uint32_t path) {
    for (int32_t i = 0; i < lw(path) - 2; ++i) {
        const uint32_t p = path + 0x10 + static_cast<uint32_t>(i) * 0x10;
        swc1(p + 0xc, vec3_distance(V(p), V(p + 0x10)));
    }
}
// a pause (state 4) before the state `next` (+0xbc)
void pause(uint32_t m, uint32_t pv, uint32_t next) {
    sb(m + 0xbc, next);
    sb(m + 0x20, 4);
    sh(pv + 0x206, static_cast<uint32_t>(frame_scale_i(15)));
}

// The flight along `path`: the point two units ahead of the nearest, the speed braking within the stopping distance of
// the path's end (to its slowest) or gaining (to 20 a second); the point's index and fraction into `idx` / `frac` (EE words)
float path_ahead(uint32_t m, uint32_t pv, uint32_t path, uint32_t near, uint32_t idx, uint32_t frac) {
    int32_t i = lw(idx);
    float t = lwc1(frac);
    path_closest_point_v(path, V(m + 0x10), V(near), &i, &t, 0, 999.0f, 5.0f, 0.0f);
    sw(idx, static_cast<uint32_t>(i));
    swc1(frac, t);
    const float ahead = fpu_add(fpu_add(fpu_mul(int_to_float(lw(idx)), lwc1(path + 0x1c)), lwc1(frac)), 2.0f);
    const float to_end = vec3_distance(V(m + 0x10), V(path + (lwu(path) << 4)));
    const float step = fpu_mul(frame_time2(), 10.0f);
    const float speed = lwc1(pv + 0x214);
    if (to_end <= fpu_div(fpu_mul(speed, speed), fpu_add(step, step))) {
        const float s = fpu_sub(speed, step);
        swc1(pv + 0x214, s);
        const float slowest = fpu_mul(lwc1(tuning(8)), frame_time());
        if (s < slowest) swc1(pv + 0x214, slowest);
    } else {
        const float s = fpu_add(speed, step);
        swc1(pv + 0x214, s);
        const float fastest = fpu_mul(frame_time(), 20.0f);
        if (fastest < s) swc1(pv + 0x214, fastest);
    }
    const int32_t at = float_to_int(fpu_div(ahead, lwc1(path + 0x1c)));
    sw(idx, static_cast<uint32_t>(at));
    const float len = lwc1(path + 0x1c);
    swc1(frac, fpu_div(fpu_sub(ahead, fpu_mul(int_to_float(at), len)), len));
    return ahead;
}
// the point `frac` of the way along the path's segment `idx` into `out`
void path_lerp(uint32_t path, uint32_t out, uint32_t idx, uint32_t frac) {
    const uint32_t p = path + (lwu(idx) << 4);
    vec3_sub(V(out), V(p + 0x20), V(p + 0x10));
    vec3_scale(V(out), V(out), lwc1(frac));
    vec3_add(V(out), V(out), V(path + (lwu(idx) << 4) + 0x10));
}
// the move by `dir` (an EE vector) done, the craft tilted with it: its pitch and roll toward the speed's share along and
// across its yaw (`div`: the tilt's divisor, 20 or 10)
void tilt(uint32_t m, uint32_t pv, uint32_t dir, float div) {
    const float sixth = u2f(0x3f860a92);
    const float rel = angle_sub(atan2_f(lwc1(dir), lwc1(dir + 4)), lwc1(m + 0x48));
    const float pitch = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x214), u2f(0x3eb2b8c2)), cos_vu(rel)), fpu_mul(frame_time(), div));
    const float s = sin_vu(rel);
    const float roll = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x214), u2f(0xbeb2b8c2)), s), fpu_mul(frame_time(), div));
    swc1(m + 0x44, angle_seek_rate(pv + 0x21c, lwc1(m + 0x44), pitch, fpu_mul(frame_time2(), u2f(0x3f060a92)),
                                   fpu_mul(frame_time2(), sixth), fpu_mul(frame_time(), u2f(0x3f490fdb))));
    swc1(m + 0x40, angle_seek_rate(pv + 0x218, lwc1(m + 0x40), roll, fpu_mul(frame_time2(), sixth),
                                   fpu_mul(frame_time2(), u2f(0x40060a92)), fpu_mul(frame_time(), u2f(0x3fc90fdb))));
}
// the yaw toward `x`, `y`
void yaw_toward(uint32_t m, uint32_t pv, float x, float y) {
    const float to = atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14)));
    swc1(m + 0x48, angle_seek_rate(pv + 0x200, lwc1(m + 0x48), to, u2f(0x3c23d70a), u2f(0x3e99999a), u2f(0x3dcccccd)));
}
// the step toward `goal` (at most the speed) into `step`, the move done and the craft tilted
void move_to(uint32_t m, uint32_t pv, uint32_t goal, uint32_t step) {
    vec3_sub(V(step), V(goal), V(m + 0x10));
    float len = vec3_length(V(step));
    if (lwc1(pv + 0x214) < len) len = lwc1(pv + 0x214);
    vec3_set_length(V(step), V(step), len);
    vec3_add(V(m + 0x10), V(m + 0x10), V(step));
    tilt(m, pv, step, 20.0f);
}
}  // namespace

// orig: L3:0x002ce238 L7:0x002ff140
void class631_hits(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    swc1(sp + 4, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t st = lw(sp);
    float dmg = lwc1(sp + 4);
    const int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    sw(sp, static_cast<uint32_t>(st));
    swc1(sp + 4, dmg);
    if (lw(sp) != 1 && lbu(m + 0x20) != 5) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(sp + 4));
        swc1(pv + 0x20, health);
        if (react == 1 || health <= 0.0f) {
            sb(m + 0x20, 5);
        } else if (react != 0xb) {
            sb(pv + 0x67, 0xb4);
            color_pulse_start(m, pv + 0x60);
            // a shot that pushes (its record applied): thrown off its course, tilted away from it
            if (lwc1(hit + 0x1c) == u2f(APPLIED) && lbu(m + 0x20) != 4) {
                const float rel = angle_sub(atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(hit)), fpu_sub(lwc1(m + 0x14), lwc1(hit + 4))), lwc1(m + 0x48));
                const float pitch = fpu_mul(cos_vu(rel), u2f(0x3db2b8c2));
                const float roll = fpu_mul(sin_vu(rel), u2f(0xbeb2b8c2));
                if (abs_f(lwc1(m + 0x40)) < abs_f(roll)) {
                    swc1(m + 0x40, roll);
                    swc1(pv + 0x218, 0.0f);
                }
                if (abs_f(lwc1(m + 0x44)) < abs_f(pitch)) {
                    swc1(m + 0x44, pitch);
                    swc1(pv + 0x21c, 0.0f);
                }
                swc1(pv + 0x214, fpu_mul(frame_time(), 4.0f));
                sh(pv + 0x206, static_cast<uint32_t>(frame_scale_i(60)));
                sb(m + 0xbc, lbu(m + 0x20));
                sb(m + 0x20, 4);
                moby_sound_start(1, 0, m);
            }
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0x80);
}

// orig: L3:0x002cca00 L7:0x002fd908
void class631_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x1e0);
    const uint32_t target = sp + 0x20;  // the target's point (target_select_any's)
    const uint32_t near = sp + 0x80, step = sp + 0x70, aim = sp + 0xd0;
    const float two_pi = u2f(0x40c90fdb), pi = u2f(0x40490fdb);
    target_select_any(m, target, lw(pv + 0x254) != 0 ? 80.0f : 40.0f);
    // gone: destroyed before (its moby bit), or without its start path, or a ride over
    bool gone;
    const uint32_t idx = lhu(m + 0xb2);
    if (((static_cast<int32_t>(lwu(OV(0x001ba4d0) + (static_cast<uint32_t>(static_cast<int16_t>(idx) >> 5) << 2))) >> (idx & 0x1f)) & 1) != 0)
        gone = true;
    else
        gone = lw(pv + 0x134) == -1 || (lbu(m + 0x20) != 0 && lbu(pv + 0x2e) != 1);
    if (gone) {
        if (lw(pv + 0x238) != -1) moby_retire(moby_at(lw(pv + 0x238)));
        moby_retire(m);
        ee_stack_free(0x1e0);
        return;
    }
    if (lwu(pv + 0x38) != 0) {
        sw(pv + 0x38, 0);
        sw(pv + 0x254, static_cast<uint32_t>(frame_scale_i(0xf0)));
    }
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x254)));
    // the rider's head
    if (lw(pv + 0x238) != -1) {
        const uint32_t r = moby_at(lw(pv + 0x238));
        if (lh(r + 0xa6) == static_cast<int32_t>(RIDER_CLASS) && static_cast<int8_t>(lbu(r + 0x20)) >= 0) {
            const uint32_t node = lwu(r + 0x78) + 0x150;
            moby_set_70(node, 2.5f);
            joint_node_spring(r, node, 2, fpu_mul(lwc1(0x0015ed64), u2f(0x3cf5c28f)), fpu_mul(lwc1(0x0015ed64), u2f(0x3e99999a)));
        }
    }
    class631_hits(m);
    switch (lbu(m + 0x20)) {  // (the table 0x1e3320: Umbris 0x2112a0)
        case 0: {  // (0x2ccbd8) set up: its numbers, at its start path's start, the turret's joints, the rider still
            swc1(pv + 0x20, 3.0f);
            sb(m + 0x7f, 0x20);
            sh(pv + 0x24, static_cast<uint32_t>(float_to_int(3.0f)));
            sb(pv + 0x28, 2);
            sb(pv + 0x58, 0x19);
            sb(pv + 0x59, 1);
            sb(pv + 0x5a, 6);
            sb(pv + 0x2e, 1);
            sb(pv + 0x29, 0);
            sw(pv + 0x130, level_path(lw(pv + 0x134)));
            if (lw(pv + 0x164) != -1) sw(pv + 0x160, level_path(lw(pv + 0x164)));
            lq_sq(m + 0x10, lwu(pv + 0x130) + 0x10);
            sw(pv + 0x214, 0);
            sw(pv + 0x218, 0);
            sw(pv + 0x21c, 0);
            sw(pv + 0x23c, 0);
            sw(pv + 0x120, 1);
            sub_0024f590(m, 0, pv + 0x180);
            sub_0024f590(m, 1, pv + 0x1c0);
            sw(pv + 0x250, 0xffffffffu);
            sw(pv + 0x240, 0);
            sw(pv + 0x224, 0);
            swc1(pv + 0x220, lwc1(m + 0x48));
            sw(pv + 0x22c, 0);
            swc1(pv + 0x228, lwc1(m + 0x44));
            if (lh(m + 0x32) < 128) sh(m + 0x32, 0x80);
            if (lw(pv + 0x238) != -1) {
                const uint32_t r = moby_at(lw(pv + 0x238));
                moby_anim_set(r, 8, 0, 0);
                sb(r + 0x30, 0);
                sw(r + 0x94, 0);
                sh(r + 0x34, lhu(r + 0x34) & 0xffbe);
            }
            // its first path (and its start zone) when its flag byte of the level allows; else straight to its route
            if (lbu(0x0014c050 + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff || lw(pv + 0x24c) == -1 || lw(pv + 0x248) == -1) {
                sb(m + 0x20, 2);
                break;
            }
            const uint32_t path = level_path(lw(pv + 0x24c));
            segment_lengths(path);
            sb(m + 0x20, 1);
            lq_sq(m + 0x10, path + 0x10);
            break;
        }
        case 1: {  // (0x2ccdd0) its first path, once the hero is in its start zone
            const uint32_t path = level_path(lw(pv + 0x24c));
            const int32_t zone = lw(pv + 0x248);
            if (zone != -1 && point_in_box(HERO_POS, zone) == 0) break;
            sw(pv + 0x248, 0xffffffffu);
            const uint32_t at = sp + 0x100, frac = sp + 0x104;
            path_ahead(m, pv, path, near, at, frac);
            if (!(lw(at) < lw(path) - 1)) {
                const bool slow = lwc1(pv + 0x214) <= fpu_mul(lwc1(tuning(8)), frame_time());
                if (lw(pv + 0x134) != -1 && slow) {  // (its end reached: the start path's start, a pause)
                    sw(pv + 0x23c, 0);
                    lq_sq(m + 0x10, lwu(pv + 0x130) + 0x10);
                    sw(pv + 0x120, 1);
                    pause(m, pv, 2);
                    break;
                }
                if (lw(pv + 0x134) == -1 && slow) {
                    debug_trap();
                    ee_stack_free(0x1e0);
                    return;
                }
                sw(at, static_cast<uint32_t>(lw(path) - 2));
                swc1(frac, 1.0f);
            }
            path_lerp(path, near, at, frac);
            const int32_t next = lw(at) + 1;
            if (next < lw(path) - 8) {
                const uint32_t p = path + (static_cast<uint32_t>(next) << 4) + 0x10;
                yaw_toward(m, pv, lwc1(p), lwc1(p + 4));
            } else {
                yaw_toward(m, pv, lwc1(target), lwc1(target + 4));
            }
            move_to(m, pv, near, step);
            break;
        }
        case 2: {  // (0x2cd10c) its route: the turret on the target (or the charge it dropped), flying from point to point
            const uint32_t follower = lw(pv + 0x23c) != 0 ? pv + 0x150 : pv + 0x120;
            const int32_t at = lw(follower);
            yaw_toward(m, pv, lwc1(target), lwc1(target + 4));
            const uint32_t charge = lwu(pv + 0x240);
            const bool on_charge = charge != 0 && lh(charge + 0xa6) == static_cast<int32_t>(CHARGE_CLASS) && lbu(charge + 0x20) != 0xfe &&
                                   lbu(charge + 0x20) != 0xfd;
            const uint32_t from = sp + 0x70, rel = sp + 0x80, inv = sp + 0x90;
            moby_chain_point_v(m, 0, V(from));
            vec3_sub(V(aim), V(on_charge ? charge + 0x10 : target), V(from));
            lq_sq(rel, aim);
            sw(sp + 0x118, from);
            mat3_transpose(M(inv), M(m + 0xc0));
            mat4_transform(V(rel), V(rel), M(inv));
            swc1(pv + 0x220, angle_seek_rate(pv + 0x224, lwc1(pv + 0x220), atan2_f(lwc1(rel), lwc1(rel + 4)), fpu_mul(frame_time2(), two_pi),
                                             fpu_mul(frame_time2(), u2f(0x41490fdb)), fpu_mul(frame_time(), two_pi)));
            const float down = -atan2_f(vec2_length(V(rel)), lwc1(rel + 8));
            swc1(pv + 0x228, angle_seek_rate(pv + 0x22c, lwc1(pv + 0x228), down, fpu_mul(frame_time2(), pi), fpu_mul(frame_time2(), two_pi),
                                             fpu_mul(frame_time(), u2f(0x3fc90fdb))));
            quat_from_axis_angle(V(pv + 0x190), 2, lwc1(pv + 0x220));
            quat_from_axis_angle(V(pv + 0x1d0), 2, lwc1(pv + 0x228));
            if (!on_charge) sw(pv + 0x240, 0);
            // the next point once the target is near it (20 across, 10 up) and nearer it than the one after
            const uint32_t off = static_cast<uint32_t>(at) << 4;
            if (at > 0) {
                const uint32_t path = lwu(follower + 0x10);
                if (at < lw(path) - 1) {
                    const float d = vec2_distance(V(path + off + 0x10), V(target));
                    if (d < 20.0f && abs_f(fpu_sub(lwc1(target + 8), lwc1(m + 0x18))) < 10.0f &&
                        d < vec3_distance(V(lwu(follower + 0x10) + off + 0x20), V(target)) && at < lw(lwu(follower + 0x10)) - 2)
                        sw(follower, lwu(follower) + 1);
                }
            }
            // the speed: braking within the stopping distance of the point or when the point is nearer the target, else
            // gaining; then toward the point (away from it when it is nearer the target)
            const uint32_t point = lwu(follower + 0x10) + off + 0x10;
            vec3_sub(V(aim), V(point), V(m + 0x10));
            const float accel = fpu_mul(frame_time2(), 4.0f);
            const float speed = lwc1(pv + 0x214);
            bool brake = vec3_length(V(aim)) < fpu_div(fpu_mul(speed, speed), fpu_add(accel, accel));
            if (!brake) brake = vec3_distance(V(point), V(target)) < vec3_distance(V(m + 0x10), V(target));
            if (brake) {
                const float s = fpu_sub(lwc1(pv + 0x214), fpu_mul(frame_time2(), 4.0f));
                swc1(pv + 0x214, s < 0.0f ? 0.0f : s);
            } else {
                const float s = fpu_add(lwc1(pv + 0x214), fpu_mul(frame_time2(), 4.0f));
                const float fastest = fpu_mul(frame_time(), 10.0f);
                swc1(pv + 0x214, fastest < s ? fastest : s);
            }
            const float sp_now = lwc1(pv + 0x214);
            const uint32_t point2 = lwu(follower + 0x10) + off + 0x10;
            vec3_set_length(V(aim), V(aim), vec3_distance(V(point2), V(target)) < vec3_distance(V(m + 0x10), V(target)) ? -sp_now : sp_now);
            vec3_add(V(m + 0x10), V(m + 0x10), V(aim));
            tilt(m, pv, aim, 10.0f);
            // the target in the route's zone: on to the route's (other) path; in the fire zone: a charge now and then
            const int32_t zone = lw(pv + 0x23c) == 0 ? lw(pv + 0x20c) : lw(pv + 0x244);
            if (point_in_box(target, zone) != 0) {
                const int32_t other = lw(pv + 0x23c) != 0 ? lw(pv + 0x234) : lw(pv + 0x230);
                if (other != -1) segment_lengths(level_path(other));
                pause(m, pv, 3);
                break;
            }
            if (point_in_box(target, lw(pv + 0x208)) == 0) break;
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x204))) == 0 && (lwu(pv + 0x240) != 0 || lwu(pv + 0x254) == 0)) break;
            sh(pv + 0x204, static_cast<uint32_t>(frame_scale_i(0x168)));
            const uint32_t to = sp + 0xf0, vel = sp + 0xe0;
            lq_sq(to, target);
            vec3_sub(V(vel), V(to), V(lwu(sp + 0x118)));
            vec2_set_length(V(vel), V(vel), fpu_mul(frame_time(), 6.0f));
            sw(vel + 8, 0);
            sw(pv + 0x240, class848_spawn(vel, lwu(sp + 0x118), m, static_cast<uint32_t>(frame_scale_i(0xf0)), lwc1(target + 8)));
            break;
        }
        case 3: {  // (0x2cd898) between its route's two paths, to and fro (or, without a route, up and away)
            const int32_t first = lw(pv + 0x230);
            if (first == -1) {
                const float sixth = u2f(0x3f860a92), twelfth = u2f(0x3f060a92), eighth = u2f(0x3f490fdb);
                approach_f(m + 0x18, 200.0f, fpu_mul(frame_time(), 8.0f));
                swc1(m + 0x44, angle_seek_rate(pv + 0x21c, lwc1(m + 0x44), 0.0f, fpu_mul(frame_time2(), twelfth), fpu_mul(frame_time2(), sixth),
                                               fpu_mul(frame_time(), eighth)));
                swc1(m + 0x40, angle_seek_rate(pv + 0x218, lwc1(m + 0x40), 0.0f, fpu_mul(frame_time2(), twelfth), fpu_mul(frame_time2(), sixth),
                                               fpu_mul(frame_time(), eighth)));
                break;
            }
            const uint32_t path = level_path(lw(pv + 0x23c) != 0 ? lw(pv + 0x234) : first);
            const uint32_t at = sp + 0x108, frac = sp + 0x10c;
            path_ahead(m, pv, path, near, at, frac);
            if (!(lw(at) < lw(path) - 1)) {
                const bool slow = lwc1(pv + 0x214) <= fpu_mul(lwc1(tuning(8)), frame_time());
                if (lw(pv + 0x234) != -1 && lw(pv + 0x164) != -1 && slow) {  // (its end: the other follower's path, a pause)
                    const uint32_t flip = lwu(pv + 0x23c) ^ 1;
                    sw(pv + 0x23c, flip);
                    const uint32_t f = flip != 0 ? pv + 0x150 : pv + 0x120;
                    lq_sq(m + 0x10, lwu(f + 0x10) + 0x10);
                    sw(f, 1);
                    pause(m, pv, 2);
                    break;
                }
                if (slow) {
                    debug_trap();
                    ee_stack_free(0x1e0);
                    return;
                }
                sw(at, static_cast<uint32_t>(lw(path) - 2));
                swc1(frac, 1.0f);
            }
            path_lerp(path, near, at, frac);
            yaw_toward(m, pv, lwc1(target), lwc1(target + 4));
            move_to(m, pv, near, step);
            break;
        }
        case 4: {  // (0x2cdc70) pausing: slowing down, then on to its next state
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x206))) != 0) {
                sb(m + 0x20, lbu(m + 0xbc));
                break;
            }
            const float s = fpu_sub(lwc1(pv + 0x214), fpu_mul(frame_time2(), 4.0f));
            swc1(pv + 0x214, s < 0.0f ? 0.0f : s);
            swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), -lwc1(pv + 0x214)));
            swc1(step + 8, 0.0f);
            swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), -lwc1(pv + 0x214)));
            vec3_add(V(m + 0x10), V(m + 0x10), V(step));
            tilt(m, pv, step, 10.0f);
            break;
        }
        case 5: {  // (0x2cde20) destroyed: the rider thrown off, a blast, its bolts, three pieces, its joints and sound freed
            const uint32_t zero = OV(0x0015f580);
            V(step) = Vec4{};
            float a = rand_angle();
            const uint32_t at = sp + 0x80, ring = sp + 0x90;
            moby_chain_point_v(m, 4, V(at));
            sw(sp + 0x110, pv + 0x180);
            sw(sp + 0x114, pv + 0x1c0);
            sw(sp + 0x118, step);
            for (int32_t i = 0; i < 4; ++i) {  // (four points round its joint 4: the original's loop leaves only its frame)
                swc1(ring, fpu_mul(cos_vu(a), 1.5f));
                swc1(ring + 4, fpu_mul(sin_vu(a), 1.5f));
                sw(ring + 8, 0);
                a = angle_add(a, u2f(0x3fc90fdb));
                vec3_add_inplace(V(ring), V(at));
            }
            moby_chain_point_v(m, 5, V(ring));
            if (lw(pv + 0x238) != -1) {
                const uint32_t r = moby_at(lw(pv + 0x238));
                const uint32_t rec = sp + 0xa0;
                sb(lwu(r + 0x78) + 0x262, 2);
                sb(r + 0x30, 0xff);
                sh(r + 0x34, lhu(r + 0x34) & 0xfff9);
                swc1(rec + 0x1c, 20.0f);
                sw(rec + 0x14, 0x830000);
                sw(rec + 0x20, 1);
                sw(rec + 0x10, m);
                vec3_sub(V(rec), V(r + 0x10), V(m + 0x10));
                vec2_set_length(V(rec), V(rec), 1.0f);
                swc1(rec + 8, 1.0f);
                swc1(rec + 0xc, u2f(APPLIED));
                sb(rec + 0x19, 3);
                sh(rec + 0x1a, lhu(m + 0xa6));
                sb(rec + 0x18, 3);
                moby_hit_record(r, rec);
            }
            sw(sp, 1);
            sw(sp + 8, 0xffffffffu);
            sw(sp + 0x10, 0);
            explosion_effect(m, lwu(sp + 0x118), m + 0x10, 0x14, 3, 4, 2, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);
            moby_drop_bolts(m, 0, -1);
            for (uint32_t cls = 0x637; cls <= 0x639; ++cls) {
                sw(sp, zero);
                debris_spawn(m, cls, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            }
            sub_0024f608(m, lwu(sp + 0x110));
            sub_0024f608(m, lwu(sp + 0x114));
            const int32_t slot = lw(pv + 0x250);
            if (slot != -1) {
                const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
                if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
            }
            sw(pv + 0x250, 0xffffffffu);
            moby_retire(m);
            ee_stack_free(0x1e0);
            return;
        }
        default:
            break;
    }
    // the tail: the floor band near the camera, the bob, the rider carried along
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 40.0f) moby_floor_band(m);
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_mul(lwc1(tuning(4)), cos_vu(lwc1(pv + 0x210)))));
    const float period = frame_scale_f(fpu_mul(lwc1(tuning(0)), 60.0f));
    const float phase = angle_add(lwc1(pv + 0x210), fpu_div(two_pi, period));
    swc1(pv + 0x210, phase);
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(tuning(4)), cos_vu(phase))));
    if (lw(pv + 0x238) != -1) {
        const uint32_t r = moby_at(lw(pv + 0x238));
        lq_sq(r + 0x10, m + 0x10);
        lq_sq(r + 0x40, m + 0x40);
        moby_anim_advance(r);
        moby_update_transform(r);
        sh(r + 0x34, lhu(r + 0x34) | 6);
    }
    ee_stack_free(0x1e0);
}

}  // namespace rac1
