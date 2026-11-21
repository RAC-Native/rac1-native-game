// Rilgar's (level 5) hoverboard race rivals (class 717): the racers Ratchet races through Blackwater City, each on a
// board (a moby of class 0x1b7 that follows it). They wait until the race starts (the hero's state 0x16), then ride
// the course's path, taking its side paths now and then and jumping its gaps, using the boost pads, speeding up or
// easing off by their place and the race's attempts, with the board's tricks in the air. Ported by hand from Rilgar's
// own code (L5:0x307910 and its helpers 0x2f8718, 0x306d30, 0x306e10, 0x307358, 0x307470, 0x307480, 0x307570), the
// original's order of calls and float operations kept.
#include <game/ovl/rilgar_own.h>

#include <game/level/level_data.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x208c his state: 0x16 racing; +0x7c0 the racers'
                                        // places, +0x894 the race's frames, +0x8c0 his place)
float dt() { return lwc1(0x0015ed6c); }    // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }   // its square
float dt64() { return lwc1(0x0015ed64); }  // (the springs' time scale)
float gravity() { return fpu_mul(dt2(), 21.0f); }  // (21 a second squared)
}  // namespace

// The racer's board: a moby of class 0x1b7 at the racer's place and angles, drawn, in state 2. 0 when none could spawn.
// orig: L5:0x002f8718
uint32_t ril_2f8718_l5(uint32_t m) {
    const uint32_t b = moby_spawn(0x1b7);
    if (b == 0) return 0;
    sb(b + 0x30, 0xff);
    sh(b + 0x32, 0xff);
    sb(b + 0x31, 1);
    sb(b + 0x20, 2);
    lq_sq(b + 0x10, m + 0x10);
    lq_sq(b + 0x40, m + 0x40);
    sd(b + 0x38, ld(m + 0x38));
    moby_update_transform(b);
    return b;
}

// Point `i` of the racer's path (+0x244) into `out`, moved sideways off the line to the next point by nothing (the
// offset the code leaves at 0).
// orig: L5:0x00306d30
void ril_306d30_l5(uint32_t m, int32_t i, uint32_t out) {
    const uint32_t path = lwu(lwu(m + 0x78) + 0x244);
    const uint32_t sp = ee_stack_alloc(0x40);
    lq_sq(out, path + 0x10 + (static_cast<uint32_t>(i) << 4));
    const int32_t next = ee_rem(i + 1, lw(path));
    lq_sq(sp, path + 0x10 + (static_cast<uint32_t>(next) << 4));
    const float a = angle_add(atan2_f(fpu_sub(lwc1(sp), lwc1(out)), fpu_sub(lwc1(sp + 4), lwc1(out + 4))), u2f(0x3fc90fdbu));
    swc1(out, fpu_add(lwc1(out), fpu_mul(cos_vu(a), 0.0f)));
    swc1(out + 4, fpu_add(lwc1(out + 4), fpu_mul(sin_vu(a), 0.0f)));
    ee_stack_free(0x40);
}

// The racer's next point (+0x258 on its path +0x244): it moves on while the point is within 2 ahead (within 90
// degrees) or within 4 behind. On the course's main path (+0x220) a point whose w lane is n > 0 sends it, 99 times in
// 100, onto the side path of its slot n (+0x224 + 4 (n - 1)); at a side path's last point it rejoins the main path 5
// points past the nearest one.
// orig: L5:0x00306e10
void ril_306e10_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x60);
    for (;;) {
        ril_306d30_l5(m, lh(pv + 0x258), sp);
        const float a = atan2_f(fpu_sub(lwc1(sp), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 4), lwc1(m + 0x14)));
        ril_306d30_l5(m, lh(pv + 0x258), sp);
        const float d = vec2_distance(V(sp), V(pv + 0x200));
        if (angle_dist(a, lwc1(m + 0x48)) < u2f(0x3fc90fdbu)) {
            if (2.0f < d) break;
        } else if (4.0f < d) {
            break;  // (the original tests 2 < d after it: true)
        }
        const uint32_t path = lwu(pv + 0x244);
        const int32_t next = ee_rem(lh(pv + 0x258) + 1, lw(path));
        sh(pv + 0x258, static_cast<uint32_t>(next));
        const uint32_t main_path = lvd::path(lw(pv + 0x220));
        if (path == main_path) {  // a fork of the course: its side path, 99 times in 100
            const uint32_t pt = path + (static_cast<uint32_t>(static_cast<int16_t>(next)) << 4);
            if (0.0f < lwc1(pt + 0x1c) && rand_mod(0x64) > 0) {
                const uint32_t q = lwu(pv + 0x244) + (static_cast<uint32_t>(lh(pv + 0x258)) << 4);
                const int32_t slot = float_to_int(lwc1(q + 0x1c)) - 1;
                const uint32_t side = lvd::path(lw(pv + 0x224 + (static_cast<uint32_t>(slot) << 2)));
                sh(pv + 0x258, 0);
                sw(pv + 0x244, side);
            }
        } else if (static_cast<int16_t>(next) == lw(path) - 1) {  // the side path's end: back onto the main path
            sw(pv + 0x244, main_path);
            const int32_t i = path_nearest_l1(pv + 0x200, main_path, 0.0f);
            sh(pv + 0x258, static_cast<uint32_t>(i));
            sh(pv + 0x258, static_cast<uint32_t>(step_index(lwu(pv + 0x244), static_cast<int16_t>(i), 5, 1)));
        }
    }
    ee_stack_free(0x60);
}

// A jump: on a side path, on the ground (its fall speed +0x24c not up), at a point whose w lane is 1 (a take-off), the
// landing (the next point whose w lane is 2) kept: its index +0x27c, its place +0x210, the air frames +0x26c started.
// orig: L5:0x00307358
void ril_307358_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lwu(pv + 0x244);
    if (path == lvd::path(lw(pv + 0x220))) return;
    if (!(lwc1(pv + 0x24c) <= 0.0f)) return;
    int32_t i = lh(pv + 0x258);
    if (lwc1(path + (static_cast<uint32_t>(i) << 4) + 0x1c) != 1.0f) return;
    uint32_t p = path;
    do {
        i = step_index(p, i, 1, 1);
        p = lwu(pv + 0x244);
    } while (lwc1(p + (static_cast<uint32_t>(i) << 4) + 0x1c) != 2.0f);
    sw(pv + 0x27c, static_cast<uint32_t>(i));
    lq_sq(pv + 0x210, lwu(pv + 0x244) + (static_cast<uint32_t>(i) << 4) + 0x10);
    sw(pv + 0x26c, 1);
}

// The board's animation set for the racer's set `set`: its set 5 is the board's 6.
// orig: L5:0x00307470
int32_t ril_307470_l5(int32_t set) { return set == 5 ? 6 : set; }

// The racer's animation `set` from `frame` over `frames`, and its board's: the board's set (0x307470), its frame scaled
// by the two speeds (+0x5c) and kept within the set's frame count.
// orig: L5:0x00307480
void ril_307480_l5(uint32_t m, int32_t set, int32_t frame, int32_t frames) {
    moby_anim_set(m, static_cast<uint32_t>(set), frame, frames);
    const uint32_t b = lwu(lwu(m + 0x78) + 0x268);
    if (b == 0) return;
    const float k = fpu_div(lwc1(b + 0x5c), lwc1(m + 0x5c));
    const int32_t bset = ril_307470_l5(set);
    float f = fpu_mul(static_cast<float>(frame), k);
    const float last = static_cast<float>(lbu(lwu(lwu(b + 0x24) + (static_cast<uint32_t>(bset) << 2) + 0x48) + 0x10));
    if (last < f) f = last;
    else if (f < 0.0f) f = 0.0f;
    moby_anim_set(lwu(lwu(m + 0x78) + 0x268), static_cast<uint32_t>(bset), static_cast<int32_t>(f2u(eer::cvt_w(f))), frames);
}

// The racer's animations: on the ground its set 0 at speed 1; in the air the jump (set 5), a trick (sets 1 to 3, one in
// four... drawn when the time to land, the quadratic's root, is long enough), the trick's speed fitted to the time in
// the air, the landing; the board's animation speed the racer's.
// orig: L5:0x00307570
void ril_307570_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x26c) == 0) {  // on the ground
        swc1(m + 0x58, 1.0f);
        if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) != 0) ril_307480_l5(m, 0, 0, frame_scale_i(8));
    } else {
        if (lbu(m + 0x53) != 5 && !(frame_scale_i(3) < lw(pv + 0x26c))) {  // the take-off
            ril_307480_l5(m, 5, 1, frame_scale_i(5));
            sh(pv + 0x272, 0xffffu);
        }
        // the time to land: the root of the fall to the landing's height
        const uint32_t sp = ee_stack_alloc(0x60);
        const float a = fpu_mul(gravity(), -0.5f);
        const float b = fpu_sub(lwc1(pv + 0x24c), a);
        const float c = fpu_sub(lwc1(pv + 0x208), lwc1(pv + 0x218));
        float r0 = lwc1(sp), r1 = lwc1(sp + 4);
        const int32_t roots = solve_quadratic(&r0, &r1, a, b, c);
        swc1(sp, r0);
        swc1(sp + 4, r1);
        float t = 30.0f;
        if (roots > 0 && 0.0f < lwc1(sp)) t = static_cast<float>(float_to_int(lwc1(sp)));
        ee_stack_free(0x60);
        if (frame_scale_i(7) < lw(pv + 0x26c) && lw(pv + 0x26c) < frame_scale_i(0xb) && lbu(m + 0x53) == 5 &&
            static_cast<float>(frame_scale_i(0x1b)) < t) {  // a trick (sets 1 .. 3) when the air time allows
            const int32_t k = rand_range(0, 3) + 1;
            sh(pv + 0x272, static_cast<uint32_t>(k));
            ril_307480_l5(m, static_cast<int16_t>(k), 1, frame_scale_i(7));
        }
        if (lh(pv + 0x272) != -1) {
            if (lbu(m + 0x52) == lbu(m + 0x53)) {  // the trick's speed fitted to the air time (0.3 .. 1.7)
                const float at = moby_anim_time(m);
                if (7.0f < at && at < 20.0f && t != 0.0f) {
                    float s = fpu_div(30.0f, t);
                    if (u2f(0x3fd9999au) < s) s = u2f(0x3fd9999au);
                    if (s < u2f(0x3e99999au)) s = u2f(0x3e99999au);
                    swc1(m + 0x58, s);
                }
            }
            if (lh(pv + 0x272) != -1) {  // the trick ends: the landing pose (set 5 from frame 3 when near, 2 at the end)
                if (t < static_cast<float>(frame_scale_i(9))) {
                    ril_307480_l5(m, 5, 3, frame_scale_i(7));
                    sh(pv + 0x272, 0xffffu);
                } else if ((lbu(m + 0x70) & 2) != 0) {
                    ril_307480_l5(m, 5, 2, frame_scale_i(7));
                    sh(pv + 0x272, 0xffffu);
                }
            }
        }
        const uint32_t set = lbu(m + 0x52);
        if (set == lbu(m + 0x53) && set == 5 && t != 0.0f)  // the jump's pose held to frame 25 over the air time
            swc1(m + 0x58, fpu_div(fpu_sub(25.0f, moby_anim_time(m)), fpu_mul(t, 0.5f)));
    }
    const uint32_t b = lwu(pv + 0x268);
    if (b != 0) swc1(b + 0x58, lwc1(m + 0x58));
}

// Class 717, a race rival. pvars: its joints +0 / +0x80 / +0x100 / +0x180 (leans +0x68, +0xe4, +0x168, +0x1e8), its
// place on the course +0x200 (height +0x208), the landing +0x210 (height +0x218), the main path +0x220 and side paths
// +0x224.., the path index +0x234, its top speeds (riding +0x238, at the start +0x23c), its speed +0x240, the path
// +0x244, its turn rate +0x248, its fall speed +0x24c, its number +0x250 (odd ones mirrored), the point +0x258, the
// start's timer +0x25a, the boost pads' wait +0x264, the board +0x268, the air frames +0x26c, the trick +0x272, the bob's
// phase +0x274, the landing's point +0x27c. States: 0 set up (at the path's first point, its speeds slowed by the race's
// attempts unless it was won); 1 waits for the race (the hero's state 0x16): the board; 2 races (back to 0 when the
// hero is out of the race).
// orig: L5:0x00307910
void ril_class717_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L5_ADDR(0x001671c0u))) < 29.0f) {  // near the camera
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }
    const uint32_t state = lbu(m + 0x20);
    const uint32_t sp = ee_stack_alloc(0x120);
    switch (state) {
    case 0: {  // (0x003079d0) set up
        sb(m + 0x30, 0x18);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        const uint32_t path = lvd::path(lw(pv + 0x234));
        sw(pv + 0x244, path);
        ril_306d30_l5(m, 0, sp);
        lq_sq(m + 0x10, sp);
        lq_sq(pv + 0x200, m + 0x10);
        sw(m + 0x40, 0);
        sw(m + 0x44, 0);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
        const float n = static_cast<float>(lw(pv + 0x250));
        const float d = dt();
        const float per = fpu_mul(n, fpu_mul(d, u2f(0x3fc66666u)));  // (1.55 a racer)
        const float start = fpu_add(fpu_mul(d, 24.0f), per);
        const float ride = fpu_add(fpu_mul(d, 17.0f), per);
        swc1(pv + 0x23c, start);
        swc1(pv + 0x238, ride);
        if (lbu(0x0013d388) == 0) {  // not won yet: 1% slower an attempt past the fourth, down to 77%
            float f = static_cast<float>(lw(0x0015ee38) - 4);
            if (f < 0.0f) f = 0.0f;
            f = fpu_sub(1.0f, fpu_mul(f, u2f(0x3c23d70au)));
            if (f < u2f(0x3f451eb8u)) f = u2f(0x3f451eb8u);
            swc1(pv + 0x23c, fpu_mul(start, f));
            swc1(pv + 0x238, fpu_mul(ride, f));
        }
        swc1(pv + 0x274, rand_angle());
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x00307b30) waits for the race: shown, its board spawned
        if (lw(kHero + 0x208c) != 0x16) break;
        sb(m + 0x30, 0xff);
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sh(pv + 0x258, 0);
        sh(pv + 0x25a, static_cast<uint32_t>(frame_scale_i(0xb4)));
        sw(pv + 0x24c, 0);
        sw(pv + 0x248, 0);
        sw(pv + 0x26c, 0);
        sw(pv + 0x264, 0);
        sw(pv + 0x240, 0);
        if (lwu(pv + 0x268) == 0) {
            const uint32_t b = ril_2f8718_l5(m);
            sw(pv + 0x268, b);
            if (b != 0) sw(lwu(b + 0x78) + 0x50, state);
        }
        const int32_t n = lw(pv + 0x250);
        if (((n + static_cast<int32_t>(static_cast<uint32_t>(n) >> 31)) >> 1 << 1) == n - 1) {  // an odd one: mirrored
            sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
            const uint32_t b = lwu(pv + 0x268);
            if (b != 0) sh(b + 0x34, lhu(b + 0x34) | 0x8000u);
        }
        sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (0x00307bec) races
        if (lw(kHero + 0x208c) != 0x16) {
            sb(m + 0x20, 0);
            break;
        }
        const uint32_t at = pv + 0x200, step = sp + 0x10, ahead = sp + 0x20;
        ril_306e10_l5(m);
        hoverboard_boost_pads(m, 0x200u, 0x25au, 0x264u);  // (Rilgar's copy: its pvars' offsets as constants)
        ril_307358_l5(m);
        if (lw(pv + 0x26c) != 0) {  // in the air: the frames counted; early on, the rise fitted to reach the landing
            sw(pv + 0x26c, lwu(pv + 0x26c) + 1);
            if (lw(pv + 0x26c) < frame_scale_i(0x11)) {
                float len = 0.0f;
                int32_t i = lh(pv + 0x258);
                if (i < lw(pv + 0x27c)) {
                    do {
                        const int32_t j = step_index(lwu(pv + 0x244), i, 1, 1);
                        const uint32_t p = lwu(pv + 0x244);
                        const float d = vec2_distance(V(p + 0x10 + (static_cast<uint32_t>(i) << 4)), V(p + 0x10 + (static_cast<uint32_t>(j) << 4)));
                        i = j;
                        len = fpu_add(len, d);
                    } while (i < lw(pv + 0x27c));
                }
                const uint32_t p = lwu(pv + 0x244);
                const float z0 = lwc1(p + (static_cast<uint32_t>(lh(pv + 0x258)) << 4) + 0x18);
                const float time = fpu_div(len, lwc1(pv + 0x240));
                const float z1 = lwc1(p + (lwu(pv + 0x27c) << 4) + 0x18);
                const float rise = fpu_add(fpu_div(fpu_sub(z1, z0), time), fpu_mul(fpu_mul(gravity(), time), 0.5f));
                if (lwc1(pv + 0x24c) < 0.0f) swc1(pv + 0x24c, 0.0f);
                approach_f(pv + 0x24c, rise, fpu_mul(dt2(), 110.0f));
            }
        }
        ril_307570_l5(m);
        ril_306d30_l5(m, lh(pv + 0x258), sp);
        const float a = atan2_f(fpu_sub(lwc1(sp), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 4), lwc1(m + 0x14)));
        spring_angle(m + 0x48, pv + 0x248, 0, a, fpu_mul(dt64(), u2f(0x3d0f5c29u)), fpu_mul(dt64(), u2f(0x3e99999au)),
                     fpu_mul(dt(), u2f(0x40df66f3u)));  // (0.035, 0.3; 400 degrees a second)
        if (lwc1(pv + 0x208) < fpu_sub(lwc1(sp + 8), 5.0f)) swc1(pv + 0x208, lwc1(sp + 8));  // (fallen 5 below: back up)
        swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x240)));
        sw(step + 8, 0);
        swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x240)));
        vec3_add(V(at), V(at), V(step));
        // what is 4 steps ahead: knocked (0x30000) along 7 steps when it is a class-500 kind of moby
        vec3_scale(V(ahead), V(step), 4.0f);
        vec3_add(V(ahead), V(ahead), V(at));
        const int32_t n = mobys_in_sphere(ahead, 1.0f, 0, m, 0);
        if (n != 0) {
            const uint32_t push = sp + 0x30;
            vec3_scale(V(push), V(step), 7.0f);
            for (int32_t k = 0; k < n; ++k) {
                const uint32_t o = lwu(L5_ADDR(0x00178300u) + static_cast<uint32_t>(k) * 4);
                if (moby_in_range_class_500(o) != 0) sub_00259bc8(lwu(L5_ADDR(0x00178300u) + static_cast<uint32_t>(k) * 4), m, 0x30000u, at, push, 1.0f);
            }
        }
        // the ground: falling while above it or rising, landed within 0.5
        const float floor = floor_probe(0.5f, at, 0);
        const float above = fpu_sub(lwc1(pv + 0x208), floor);
        if (0.0f < above || 0.0f < lwc1(pv + 0x24c)) {
            swc1(pv + 0x24c, fpu_sub(lwc1(pv + 0x24c), gravity()));
        } else if (abs_f(above) < 0.5f) {
            swc1(pv + 0x24c, 0.0f);
            swc1(pv + 0x208, floor);
            sw(pv + 0x26c, 0);
        }
        swc1(pv + 0x208, fpu_add(lwc1(pv + 0x208), lwc1(pv + 0x24c)));
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x25a))) != 0) {  // racing: its speed by its place
            float target = floor < lwc1(pv + 0x208) ? fpu_mul(dt(), 19.0f) : lwc1(pv + 0x238);
            if (lw(kHero + 0x894) < frame_scale_i(0x2bc)) target = fpu_add(target, fpu_mul(dt(), 3.0f));  // (the first 700 frames)
            const int32_t place = lh(kHero + 0x7c0 + (lwu(pv + 0x250) << 1));
            if (place < lw(kHero + 0x8c0)) target = fpu_sub(target, fpu_mul(dt(), 1.5f));  // (ahead of the hero: easing off)
            else target = fpu_add(target, dt());
            approach_f(pv + 0x240, target, fpu_mul(dt2(), target < lwc1(pv + 0x240) ? 7.0f : 15.0f));
        } else {  // the start
            approach_f(pv + 0x240, lwc1(pv + 0x23c), fpu_mul(dt2(), 20.0f));
        }
        if (above < u2f(0x3e4ccccdu) && lwc1(pv + 0x24c) < 0.0f) {  // (within 0.2 and falling: on the ground)
            swc1(pv + 0x208, floor);
            sw(pv + 0x26c, 0);
        }
        // the leans from the turn rate (at most 150 degrees a second), mirrored on the odd ones
        moby_set_70(pv + 0x180, 3.0f);
        const float lim = fpu_mul(dt(), u2f(0x40278d36u));
        float turn = lwc1(pv + 0x248);
        if (lim < turn) turn = lim;
        else if (turn < -lim) turn = -lim;
        if ((lhu(m + 0x34) & 0x8000u) != 0) turn = -turn;
        swc1(pv + 0x1e8, fpu_mul(turn, 30.0f));
        swc1(pv + 0x68, fpu_mul(turn, 20.0f));
        swc1(pv + 0x168, fpu_mul(turn, 50.0f));
        swc1(pv + 0xe4, fpu_mul(turn, 40.0f));
        const float k = u2f(0x3ca3d70au), damp = u2f(0x3e99999au);  // (0.02, 0.3)
        joint_node_spring(m, pv + 0x180, 3, fpu_mul(dt64(), k), fpu_mul(dt64(), damp));
        joint_node_spring(m, pv, 0, fpu_mul(dt64(), k), fpu_mul(dt64(), damp));
        joint_node_spring(m, pv + 0x100, 1, fpu_mul(dt64(), k), fpu_mul(dt64(), damp));
        joint_node_spring(m, pv + 0x80, 2, fpu_mul(dt64(), u2f(0x3c75c28fu)), fpu_mul(dt64(), damp));  // (0.015)
        // the moby at its place, 0.25 up, bobbing 0.08; the board with it
        lq_sq(m + 0x10, at);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.25f));
        swc1(pv + 0x274, angle_add(lwc1(pv + 0x274), fpu_mul(dt(), u2f(0x40c37a14u))));  // (350 degrees a second)
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(sin_vu(lwc1(pv + 0x274)), u2f(0x3da3d70au))));
        const uint32_t b = lwu(pv + 0x268);
        if (b != 0) {
            sd(b + 0x38, ld(m + 0x38));
            lq_sq(lwu(pv + 0x268) + 0x10, m + 0x10);
            lq_sq(lwu(pv + 0x268) + 0x40, m + 0x40);
            if (lh(pv + 0x25a) != 0) sb(lwu(pv + 0x268) + 0xbc, 1);
        }
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x120);
}

}  // namespace rac1
