// Kalebo III's (level 16) class 556: the five racers of the hoverboard race (the same kind of code as Rilgar's race rivals,
// class 717 in rilgar_race_rivals.cpp, with Kalebo's own constants, pvars and a few changes: a racer can be shot off its
// board, its trick's frames, its sparks). They wait until the race starts (the hero's state 0x16), then ride the course's
// path, taking its side paths now and then and jumping its gaps, using the boost pads, speeding up or easing off by their
// place, with tricks in the air and sparks under the board. Level 16 only: `orig: L16:`, level data L16_ADDR / lvd::*, base
// ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class556, class556_cases) and batch 371 (the race's dumps).
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
using kalebo::Frame;

constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x7c0 the racers' places, +0x894 the race's frames, +0x8a8
                                        // the race's score, +0x8c0 his place); his moby's state word 0x1413dc (0x16: racing)
float dt() { return lwc1(0x0015ed6c); }    // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }   // its square
float dt64() { return lwc1(0x0015ed64); }  // (the springs' time scale)
float gravity() { return fpu_mul(dt2(), 21.0f); }  // (21 a second squared)

bool racing() { return lw(0x001413dc) == 0x16; }

// the racer's animation `set` from `frame` over `frames` (the level's helper is a bare call)
// orig: L16:0x002d0038
void racer_anim(uint32_t m, uint32_t set, int32_t frame, int32_t frames) { moby_anim_set(m, set, frame, frames); }

// the address of point `i` of the racer's path (+0x2c0)
uint32_t racer_point(uint32_t pv, int32_t i) { return lwu(pv + 0x2c0) + 0x10 + (static_cast<uint32_t>(i) << 4); }
}  // namespace

// Point `i` of the racer's path (+0x2c0) into `out`.
// orig: L16:0x002cf9d0
void kal_racer_point_l16(uint32_t m, int32_t i, uint32_t out) { V(out) = V(racer_point(lwu(m + 0x78), i)); }

// The racer's next point (+0x2f4 on its path +0x2c0): it moves on while the point is within 2 ahead (within 90 degrees) or
// within 4 behind. On the course's main path (+0x290) a point whose w lane is n > 10 sends it, 69 times in 100, onto the
// side path of its slot n - 11 (+0x294 ..); at a side path's last point it rejoins the main path 5 points past the nearest.
// orig: L16:0x002cf9f8
void kal_racer_next_point_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x60);
    const uint32_t sp = f.sp;
    for (;;) {
        kal_racer_point_l16(m, lh(pv + 0x2f4), sp);
        const float a = atan2_f(fpu_sub(lwc1(sp), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 4), lwc1(m + 0x14)));
        kal_racer_point_l16(m, lh(pv + 0x2f4), sp);
        const float d = vec2_distance(V(sp), V(pv + 0x260));
        if (angle_dist(a, lwc1(m + 0x48)) < u2f(0x3fc90fdbu)) {
            if (2.0f < d) break;
        } else if (4.0f < d) {
            break;  // (the original tests 2 < d after it: true)
        }
        const uint32_t path = lwu(pv + 0x2c0);
        const int32_t next = ee_rem(lh(pv + 0x2f4) + 1, lw(path));
        sh(pv + 0x2f4, static_cast<uint32_t>(next));
        const uint32_t main_path = lvd::path(lw(pv + 0x290));
        if (path == main_path) {  // a fork of the course: its side path, 69 times in 100
            if (!(10.0f < lwc1(path + (static_cast<uint32_t>(static_cast<int16_t>(next)) << 4) + 0x1c))) continue;
            if (rand_mod(0x64) < 0x1f) continue;
            const int32_t slot = float_to_int(lwc1(racer_point(pv, lh(pv + 0x2f4)) + 0xc)) - 0xb;
            const uint32_t side = lvd::path(lw(pv + 0x294 + (static_cast<uint32_t>(slot) << 2)));
            sh(pv + 0x2f4, 0);
            sw(pv + 0x2c0, side);
        } else if (static_cast<int16_t>(next) == lw(path) - 1) {  // the side path's end: back onto the main path
            sw(pv + 0x2c0, main_path);
            const int32_t i = path_nearest_l1(pv + 0x260, main_path, 0.0f);
            sh(pv + 0x2f4, static_cast<uint32_t>(i));
            sh(pv + 0x2f4, static_cast<uint32_t>(step_index(lwu(pv + 0x2c0), static_cast<int16_t>(i), 5, 1)));
        }
    }
}

// A jump: on the ground (its fall speed +0x2c8 not up), at a point whose w lane is 1 (a take-off), the landing (the next point
// whose w lane is 2) kept: its index +0x2f0, its place +0x270, the air frames +0x2e4 started.
// orig: L16:0x002cff48
void kal_racer_jump_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (!(lwc1(pv + 0x2c8) <= 0.0f)) return;
    int32_t i = lh(pv + 0x2f4);
    uint32_t path = lwu(pv + 0x2c0);
    if (lwc1(path + (static_cast<uint32_t>(i) << 4) + 0x1c) != 1.0f) return;
    // (the original then tests the same w lane against 2, which it cannot be)
    do {
        i = step_index(path, i, 1, 1);
        path = lwu(pv + 0x2c0);
    } while (lwc1(path + (static_cast<uint32_t>(i) << 4) + 0x1c) != 2.0f);
    sh(pv + 0x2f0, static_cast<uint32_t>(i));
    V(pv + 0x270) = V(racer_point(pv, static_cast<int16_t>(i)));
    sw(pv + 0x2e4, 1);
}

// The racer's pitch (+0x44, its rate +0x2f8: along the slope to the next point on the ground, level in the air) and its
// animations: on the ground set 0 at speed 1; in the air the jump (set 5 from frame 4), a trick (sets 1 to 3 from frame 3)
// when the time to land, the quadratic's root, is long enough, the trick's speed fitted to the time in the air, the landing
// (set 5 from frame 51 when near, from 28 when the trick has played out), the jump's pose held to frame 50 over the air time.
// orig: L16:0x002d0058
void kal_racer_anims_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x60);
    const uint32_t sp = f.sp;
    float pitch = 0.0f;
    if (lw(pv + 0x2e4) == 0) {  // on the ground: the slope to the next point
        const int32_t j = step_index(lwu(pv + 0x2c0), lh(pv + 0x2f4), 1, 1);
        const uint32_t path = lwu(pv + 0x2c0);
        const uint32_t pi = path + 0x10 + (static_cast<uint32_t>(lh(pv + 0x2f4)) << 4), pj = path + 0x10 + (static_cast<uint32_t>(j) << 4);
        const float run = vec2_distance(V(pi), V(pj));
        const uint32_t path2 = lwu(pv + 0x2c0);
        const float rise = fpu_sub(lwc1(path2 + (static_cast<uint32_t>(j) << 4) + 0x18),
                                   lwc1(path2 + (static_cast<uint32_t>(lh(pv + 0x2f4)) << 4) + 0x18));
        pitch = atan2_f(run, rise);
    }
    spring_angle(m + 0x44, pv + 0x2f8, 0, pitch, fpu_mul(dt64(), u2f(0x3ca3d70au)), fpu_mul(dt64(), u2f(0x3e99999au)),
                 fpu_mul(dt(), u2f(0x40490fdbu)));  // (0.02, 0.3; pi a second)
    if (lw(pv + 0x2e4) == 0) {  // on the ground
        swc1(m + 0x58, 1.0f);
        if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) != 0) racer_anim(m, 0, 0, frame_scale_i(8));
        return;
    }
    if (lbu(m + 0x53) != 5 && lw(pv + 0x2e4) < frame_scale_i(3)) {  // the take-off
        racer_anim(m, 5, 4, frame_scale_i(5));
        sh(pv + 0x2fe, 0xffffu);
    }
    // the time to land: the root of the fall to the landing's height
    const float a = fpu_mul(gravity(), -0.5f);
    const float b = fpu_sub(lwc1(pv + 0x2c8), a);
    const float c = fpu_sub(lwc1(pv + 0x268), lwc1(pv + 0x278));
    float r0 = lwc1(sp), r1 = lwc1(sp + 4);  // (words of the frame the original never writes first)
    const int32_t roots = solve_quadratic(&r0, &r1, a, b, c);
    swc1(sp, r0);
    swc1(sp + 4, r1);
    float t = 30.0f;
    if (roots > 0 && 0.0f < lwc1(sp)) t = static_cast<float>(float_to_int(lwc1(sp)));
    if (frame_scale_i(7) < lw(pv + 0x2e4) && lw(pv + 0x2e4) < frame_scale_i(0xb) && lbu(m + 0x53) == 5 &&
        static_cast<float>(frame_scale_i(0x1b)) < t) {  // a trick (sets 1 .. 3) when the air time allows
        const int32_t k = rand_range(0, 3) + 1;
        sh(pv + 0x2fe, static_cast<uint32_t>(k));
        racer_anim(m, static_cast<uint32_t>(static_cast<int16_t>(k)), 3, frame_scale_i(7));
    }
    if (lh(pv + 0x2fe) != -1) {
        if (lbu(m + 0x52) == lbu(m + 0x53)) {  // the trick's speed fitted to the air time (0.3 .. 1.7)
            const float at = moby_anim_time(m);
            if (7.0f < at && at < 20.0f && t != 0.0f) {
                float s = fpu_div(30.0f, t);
                if (u2f(0x3fd9999au) < s) s = u2f(0x3fd9999au);
                if (s < u2f(0x3e99999au)) s = u2f(0x3e99999au);
                swc1(m + 0x58, s);
            }
        }
        if (lh(pv + 0x2fe) != -1) {  // the trick ends: the landing pose (from frame 51 when near, 28 when played out)
            if (t < static_cast<float>(frame_scale_i(5))) {
                racer_anim(m, 5, 0x33, frame_scale_i(5));
                sh(pv + 0x2fe, 0xffffu);
            } else if ((lbu(m + 0x70) & 2) != 0) {
                racer_anim(m, 5, 0x1c, frame_scale_i(7));
                sh(pv + 0x2fe, 0xffffu);
            }
        }
    }
    const uint32_t set = lbu(m + 0x52);
    if (set == lbu(m + 0x53) && set == 5 && t != 0.0f)  // the jump's pose held to frame 50 over the air time
        swc1(m + 0x58, fpu_div(fpu_sub(50.0f, moby_anim_time(m)), t));
}

// Class 556, a racer. pvars: its joints +0x60 / +0xe0 / +0x160 / +0x1e0 (leans +0xc8, +0x144, +0x1c8, +0x248), its place on
// the course +0x260 (height +0x268), the landing +0x270 (height +0x278), its place a frame ago +0x280, the main path +0x290 and
// side paths +0x294 .., the path index +0x2b0, its top speeds (riding +0x2b4, at the start +0x2b8), its speed +0x2bc, the path
// +0x2c0, its turn rate +0x2c4, its fall speed +0x2c8, its number +0x2cc (odd ones mirrored), the boost pads' wait +0x2dc, the
// air frames +0x2e4, the bob's phase +0x2e8, the landing's point +0x2f0, the frames since it was shot +0x2f2, the point +0x2f4,
// the start's timer +0x2f6, the pitch rate +0x2f8, the trick +0x2fe. States: 0 set up (at the path's first point, its speeds
// slowed by the race's attempts unless it was won); 1 waits for the race (the hero's state 0x16); 2 races; 3 shot off (hit by
// 0x30000: an explosion, debris, 250 to the race's score; back on (2) four seconds later once the camera looks away). Out of
// the race (states 1 .. 3) it goes back to 0.
// orig: L16:0x002d04a0
void kal_class556_update_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x190);
    const uint32_t sp = f.sp;
    moby_set_70(pv + 0x1e0, 2.5f);
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002d053c) set up
        sb(m + 0x30, 0x18);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        const uint32_t path = lvd::path(lw(pv + 0x2b0));
        sw(pv + 0x2c0, path);
        kal_racer_point_l16(m, 0, sp + 0x20);
        V(m + 0x10) = V(sp + 0x20);
        V(pv + 0x260) = V(m + 0x10);
        sw(m + 0x40, 0);
        sw(m + 0x44, 0);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
        sb(pv + 0x2b, 1);
        const float n = static_cast<float>(lw(pv + 0x2cc));
        const float d = dt();
        const float per = fpu_mul(n, fpu_mul(d, u2f(0x3fc66666u)));  // (1.55 a racer)
        const float start = fpu_add(fpu_mul(d, 24.0f), per);
        const float ride = fpu_add(fpu_mul(d, 17.0f), per);
        swc1(pv + 0x2b8, start);
        swc1(pv + 0x2b4, ride);
        if (lbu(0x0013d4df) == 0) {  // not won yet: 0.8% slower an attempt past the fifth, down to 80%
            float k = static_cast<float>(lw(0x0015ee3c) - 5);
            if (k < 0.0f) k = 0.0f;
            k = fpu_sub(1.0f, fpu_mul(k, u2f(0x3c03126fu)));
            if (k < u2f(0x3f4ccccdu)) k = u2f(0x3f4ccccdu);
            swc1(pv + 0x2b8, fpu_mul(start, k));
            swc1(pv + 0x2b4, fpu_mul(ride, k));
        }
        swc1(pv + 0x2e8, rand_angle());
        sb(pv + 0x29, 0);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x002d06a8) waits for the race: shown
        if (!racing()) break;
        sb(m + 0x30, 0xff);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sh(pv + 0x2f4, 0);
        sh(pv + 0x2f6, static_cast<uint32_t>(frame_scale_i(0xb4)));
        sw(pv + 0x2c8, 0);
        sw(pv + 0x2c4, 0);
        sw(pv + 0x2e4, 0);
        sw(pv + 0x2dc, 0);
        sw(pv + 0x2bc, 0);
        const int32_t n = lw(pv + 0x2cc);
        if (n - ((n + static_cast<int32_t>(static_cast<uint32_t>(n) >> 31)) >> 1 << 1) == 1)  // an odd one: mirrored
            sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        sb(m + 0xa4, 0xff);
        sb(m + 0x20, 2);
        break;
    }
    case 3: {  // (0x002d0738) shot off: back on four seconds later, when the camera looks away from it
        if (!racing()) {
            sb(m + 0x20, 0);
            break;
        }
        const int16_t since = static_cast<int16_t>(lhu(pv + 0x2f2) + 1);
        sh(pv + 0x2f2, static_cast<uint16_t>(since));
        if (!(frame_scale_i(0xf0) < since)) break;
        const float a = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(L16_ADDR(0x001671c0))), fpu_sub(lwc1(m + 0x14), lwc1(L16_ADDR(0x001671c4))));
        if (!(u2f(0x3fc90fdbu) < angle_dist(a, lwc1(L16_ADDR(0x001671d8))))) break;
        sb(m + 0x20, 2);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sb(m + 0xa4, 0xff);
        break;
    }
    case 2: {  // (0x002d07e4) races
        if (!racing()) {
            sb(m + 0x20, 0);
            break;
        }
        if (sub_0025a420(m, 0x30000u, 0) != 0) {  // shot off its board
            const uint32_t at = sp + 0x20;
            V(at) = V(m + 0x10);
            swc1(at + 8, fpu_add(lwc1(at + 8), 0.5f));
            explosion_effect(m, L16_ADDR(0x0015f580), at, 5, 0xf, 0x19, -1, 0, 9, -1, 0, 0.0f, 0.0f, 2.5f, 1.5f, 4.0f, 1.0f,
                             7.0f);
            debris_burst(m, 0x655, 1, 0x656, 2, 2, 0);
            debris_burst(m, 0x6fa, 1, 0x6fb, 1, 1, 0);
            sh(pv + 0x2f2, 0);
            sw(kHero + 0x8a8, lwu(kHero + 0x8a8) + 0xfa);
            sb(m + 0x20, 3);
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            break;
        }
        sb(m + 0xa4, 0xff);
        const uint32_t at = pv + 0x260, step = sp + 0x30, move = sp + 0x40, spark = sp + 0x50, vel = sp + 0x60;
        kal_racer_next_point_l16(m);
        hoverboard_boost_pads(m, 0x260u, 0x2f6u, 0x2dcu);  // (Kalebo's copy: its pvars' offsets as constants)
        kal_racer_jump_l16(m);
        if (lw(pv + 0x2e4) != 0) {  // in the air: the frames counted; early on, the rise fitted to reach the landing
            sw(pv + 0x2e4, lwu(pv + 0x2e4) + 1);
            if (lw(pv + 0x2e4) < frame_scale_i(0x11)) {
                float len = 0.0f;
                int32_t i = lh(pv + 0x2f4);
                if (i < lh(pv + 0x2f0)) {
                    do {
                        const int32_t j = step_index(lwu(pv + 0x2c0), i, 1, 1);
                        const uint32_t p = lwu(pv + 0x2c0);
                        const float d = vec2_distance(V(p + 0x10 + (static_cast<uint32_t>(i) << 4)), V(p + 0x10 + (static_cast<uint32_t>(j) << 4)));
                        i = j;
                        len = fpu_add(len, d);
                    } while (i < lh(pv + 0x2f0));
                }
                const uint32_t p = lwu(pv + 0x2c0);
                const float z0 = lwc1(p + (static_cast<uint32_t>(lh(pv + 0x2f4)) << 4) + 0x18);
                const float time = fpu_div(len, lwc1(pv + 0x2bc));
                const float z1 = lwc1(p + (static_cast<uint32_t>(lh(pv + 0x2f0)) << 4) + 0x18);
                const float rise = fpu_add(fpu_div(fpu_sub(z1, z0), time), fpu_mul(fpu_mul(gravity(), time), 0.5f));
                if (lwc1(pv + 0x2c8) < 0.0f) swc1(pv + 0x2c8, 0.0f);
                approach_f(pv + 0x2c8, rise, fpu_mul(dt2(), 110.0f));
            }
        }
        kal_racer_anims_l16(m);
        kal_racer_point_l16(m, lh(pv + 0x2f4), sp + 0x20);
        const float a = atan2_f(fpu_sub(lwc1(sp + 0x20), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 0x24), lwc1(m + 0x14)));
        spring_angle(m + 0x48, pv + 0x2c4, 0, a, fpu_mul(dt64(), u2f(0x3d0f5c29u)), fpu_mul(dt64(), u2f(0x3e99999au)),
                     fpu_mul(dt(), u2f(0x40df66f3u)));  // (0.035, 0.3; 400 degrees a second)
        if (lwc1(pv + 0x268) < fpu_sub(lwc1(sp + 0x28), 5.0f)) swc1(pv + 0x268, lwc1(sp + 0x28));  // (fallen 5 below: back up)
        swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x2bc)));
        sw(step + 8, 0);
        swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x2bc)));
        vec3_add(V(at), V(at), V(step));
        // what is 3 steps ahead (0.7 up): knocked (0x30000) along 7 steps when it is a class-500 kind of moby
        vec3_scale(V(move), V(step), 3.0f);
        vec3_add(V(move), V(move), V(at));
        swc1(move + 8, fpu_add(lwc1(move + 8), u2f(0x3f333333u)));
        const int32_t n = mobys_in_sphere(move, u2f(0x3f333333u), 0, m, 0);
        if (n != 0) {
            vec3_scale(V(spark), V(step), 7.0f);
            for (int32_t k = 0; k < n; ++k) {
                if (moby_in_range_class_500(coll_hit_list_at(k)) != 0) sub_00259bc8(coll_hit_list_at(k), m, 0x30000u, at, spark, 1.0f);
            }
        }
        // the ground: falling while above it or rising, landed within 0.5
        const float floor = floor_probe_v(0.5f, V(at), 0);
        const float above = fpu_sub(lwc1(pv + 0x268), floor);
        if (0.0f < above || 0.0f < lwc1(pv + 0x2c8)) {
            swc1(pv + 0x2c8, fpu_sub(lwc1(pv + 0x2c8), gravity()));
        } else if (abs_f(above) < 0.5f) {
            swc1(pv + 0x2c8, 0.0f);
            swc1(pv + 0x268, floor);
            sw(pv + 0x2e4, 0);
        }
        swc1(pv + 0x268, fpu_add(lwc1(pv + 0x268), lwc1(pv + 0x2c8)));
        if (kalebo::tick16(pv + 0x2f6) != 0) {  // racing: its speed by its place
            float target = floor < lwc1(pv + 0x268) ? fpu_mul(dt(), 19.0f) : lwc1(pv + 0x2b4);
            if (lw(kHero + 0x894) < frame_scale_i(0x2bc)) target = fpu_add(target, fpu_mul(dt(), 3.0f));  // (the first 700 frames)
            const int32_t place = lh(kHero + 0x7c0 + (lwu(pv + 0x2cc) << 1));
            if (place < lw(kHero + 0x8c0)) target = fpu_sub(target, dt());  // (ahead of the hero: easing off)
            else target = fpu_add(target, dt());
            approach_f(pv + 0x2bc, target, fpu_mul(dt2(), target < lwc1(pv + 0x2bc) ? 7.0f : 15.0f));
        } else {  // the start
            approach_f(pv + 0x2bc, lwc1(pv + 0x2b8), fpu_mul(dt2(), 20.0f));
        }
        if (above < u2f(0x3e4ccccdu) && lwc1(pv + 0x2c8) < 0.0f) {  // (within 0.2 and falling: on the ground)
            swc1(pv + 0x268, floor);
            sw(pv + 0x2e4, 0);
        }
        // the leans from the turn rate (at most 150 degrees a second), mirrored on the odd ones
        const float lim = fpu_mul(dt(), u2f(0x40278d36u));
        float turn = lwc1(pv + 0x2c4);
        if (lim < turn) turn = lim;
        else if (turn < -lim) turn = -lim;
        if ((lhu(m + 0x34) & 0x8000u) != 0) turn = -turn;
        swc1(pv + 0x248, fpu_mul(turn, 30.0f));
        swc1(pv + 0xc8, fpu_mul(turn, 20.0f));
        swc1(pv + 0x1c8, fpu_mul(turn, 50.0f));
        swc1(pv + 0x144, fpu_mul(turn, 40.0f));
        const float k = u2f(0x3ca3d70au), damp = u2f(0x3e99999au);  // (0.02, 0.3)
        joint_node_spring(m, pv + 0x1e0, 3, fpu_mul(dt64(), k), fpu_mul(dt64(), damp));
        joint_node_spring(m, pv + 0x60, 0, fpu_mul(dt64(), k), fpu_mul(dt64(), damp));
        joint_node_spring(m, pv + 0x160, 1, fpu_mul(dt64(), k), fpu_mul(dt64(), damp));
        joint_node_spring(m, pv + 0xe0, 2, fpu_mul(dt64(), u2f(0x3c75c28fu)), fpu_mul(dt64(), damp));  // (0.015)
        // the moby at its place, 0.25 up, bobbing 0.08
        V(m + 0x10) = V(at);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.25f));
        swc1(pv + 0x2e8, angle_add(lwc1(pv + 0x2e8), fpu_mul(dt(), u2f(0x40c37a14u))));  // (350 degrees a second)
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(sin_vu(lwc1(pv + 0x2e8)), u2f(0x3da3d70au))));
        // the sparks under the board, at its joint 4 or 5 (by the frame), along its move this frame (+0x280 the last place)
        vec3_sub(V(move), V(m + 0x10), V(pv + 0x280));
        V(pv + 0x280) = V(m + 0x10);
        moby_chain_point_v(m, (lwu(L16_ADDR(0x0015f5cc)) & 1u) | 4u, V(spark));
        const float r = rand_angle();
        const float jitter = fpu_mul(dt(), 0.25f);
        const uint32_t side = sp + 0x70;
        vec3_set_length(V(vel), V(m + 0xe0), fpu_mul(jitter, cos_vu(r)));
        vec3_set_length(V(side), V(m + 0xd0), fpu_mul(jitter, sin_vu(r)));
        vec3_add(V(vel), V(vel), V(move));
        vec3_add(V(vel), V(vel), V(side));
        V(vel) = V(move);  // (the original then overwrites the jittered velocity with the move itself)
        swc1(vel + 0xc, u2f(0x47ea6000u));  // (120000)
        if (lh(pv + 0x2f6) == 0) {
            const int32_t life = rand_range(frame_scale_i(0xa), frame_scale_i(0xd));
            spawn_particle_4(spark, vel, lwu(L16_ADDR(0x00161a08)), lwu(L16_ADDR(0x00161a0c)), static_cast<uint32_t>(life), 0xf, 0xf, 1);
            break;
        }
        const uint32_t slow = sp + 0x80;  // (the start: a slower spray and a glow)
        vec3_scale(V(slow), V(move), u2f(0x3f6b851fu));  // (0.92)
        const int32_t life = rand_range(frame_scale_i(0xf), frame_scale_i(0x12));
        spawn_particle_4(spark, slow, 0x5032f0d2u, lwu(L16_ADDR(0x00161a0c)), static_cast<uint32_t>(life), 0x1e, 0x14, 1);
        const int32_t life2 = rand_range(frame_scale_i(8), frame_scale_i(0x11));
        spawn_particle_15_v(V(spark), V(vel), lwu(L16_ADDR(0x00161a08)), lwu(L16_ADDR(0x00161a0c)), static_cast<uint32_t>(life2), 1, 15000.0f);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
