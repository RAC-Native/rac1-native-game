// Novalis's own moby classes, part 2: the platforms, doors and lifts. Header
// novalis_mobys.h. Addresses are Novalis's (`orig: L1:`); Novalis-
// only data is L1_ADDR, the globals the engine shares with Veldin are the level's own (OV).
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// Whether the moby's sound channel `slot` (-1: none), if it is this moby's and playing, is stopped:
// the idiom Novalis's classes inline (the channel record is 0x70 bytes at 0x13e550: owner +0x88,
// playing +0x74).
// orig: (helper) repeated in 0x002b9eb0, 0x002f4710, 0x002fb8a8, ...
void moby_sound_release_l1(uint32_t m, int32_t slot) {
    if (slot == -1) return;
    const uint32_t c = 0x0013e550 + static_cast<uint32_t>(slot) * 0x70;
    if (lwu(c + 0x88) != m) return;
    if (lbu(c + 0x74) == 0) return;
    sub_0028d918(slot);
}

// Whether the hero is carried by / standing on the moby: while the run flag (0x1413dc) is 3 or the
// locomotion state (0x1413d4) is 0x1c the moby the hero holds (0x13f848); otherwise, when
// the camera-side half 0x13f65e is 0, the platform record 0x13f64c.
// orig: L1:0x00277fb8
int32_t moby_hero_on_l1(uint32_t m) {
    const uint32_t P = g::PLAYER;
    if (lw(P + 0x208c) == 3 || lw(P + 0x2084) == 0x1c) return lwu(P + 0x4f8) == m ? 1 : 0;
    if (lh(P + 0x30e) != 0) return 0;
    return lwu(P + 0x2fc) == m ? 1 : 0;
}

// Class 726, Novalis's lift: a platform running back and forth along a path (pvars: the path
// index +0xb4, the hero-near boxes +0xcc / +0xc0, the wait +0xba, the speed +0xb0 in
// 60ths of a path per second, the phase +0xa4 in 0..1, its rate +0xa8 / +0xac, the sound slot +0xc4,
// the attach frame +0x60). m+0xbc: 0 at the first point, 1 at the last, 2 on its way. It sets off
// when the hero rides it or comes nearer to its far end than to its near one.
// orig: L1:0x002b9eb0
void lift_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    bool ready = false;
    if (lw(pv + 0xb4) == -1) return;
    const uint32_t path = lwu(OV(0x001b04b0) + static_cast<uint32_t>(lw(pv + 0xb4)) * 4);
    const uint32_t hero = 0x0013f3d0;
    const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
    if (lw(pv + 0xcc) != -1) {
        if (point_in_box(hero, lw(pv + 0xcc)) == 0) {  // (the hero is not in the lift's box: hidden)
            sb(m + 0x30, 0xff);
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            return;
        }
        if (point_in_box(hero, lw(pv + 0xcc)) != 0) {
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sw(pv + 0xcc, 0xffffffffu);
        }
    }
    const int32_t c0 = lw(pv + 0xc0);
    if (c0 == -1) {
        ready = true;
    } else {
        bool seen = point_in_box(hero, c0) != 0;
        if (!seen) {
            const uint32_t idx16 = lhu(m + 0xb2);
            const int32_t idx = static_cast<int16_t>(idx16);
            if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) seen = true;
            else if (((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0) seen = true;
        }
        if (seen) {
            if (lw(pv + 0xc0) != -1) {
                const uint32_t idx16 = lhu(m + 0xb2);
                const uint32_t w = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(idx16)) >> 5) << 2;
                sw(0x0014c190 + (level << 8) + w, lwu(0x0014c190 + (level << 8) + w) | (1u << (idx16 & 0x1f)));
                const uint32_t idx16b = lhu(m + 0xb2);
                const uint32_t wb = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(idx16b)) >> 5) << 2;
                sw(OV(0x001ba4d0) + wb, lwu(OV(0x001ba4d0) + wb) | (1u << (idx16b & 0x1f)));
                sw(pv + 0xc0, 0xffffffffu);
            }
            ready = true;
        }
    }
    const Vec4 start = V(m + 0x10);  // (the position before this frame)
    timer_tick_i16(ee_ref<int16_t>(pv + 0xba));
    if (lbu(m + 0x20) == 0) {
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 5);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        const uint32_t home = lbu(pv + 0xa0);
        sb(m + 0xbc, home);
        const float phase = home == 0 ? 1.0f : 0.0f;
        swc1(pv + 0xa4, phase);
        uint32_t at = path + 0x10;
        if (float_to_int(phase) != 0) at = at + static_cast<uint32_t>(lw(path) - 1) * 16;
        lq_sq(m + 0x10, at);
        sh(pv + 0xba, 0);
        sw(pv + 0xa8, 0);
        sw(pv + 0xbc, 0);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sw(pv + 0xc4, 0xffffffffu);
    }
    const uint32_t where = lbu(m + 0xbc);
    const uint32_t last = path + static_cast<uint32_t>(lw(path)) * 16;  // (the last point: the points start at +0x10)
    // the lift sets off for the far end (+1.0) or back (-1.0): the time over the path's length in frames
    auto set_off = [&](float dir) {
        sb(m + 0xbc, 2);
        const float f0 = frame_scale_f(lwc1(pv + 0xb0) * 60.0f);
        sh(pv + 0xb8, 0);
        swc1(pv + 0xac, fpu_div(dir, f0));
    };
    if (where == 0 || where == 1) {
        // at an end: when the hero has been in the box (or the hero rides it / has not left it) and is
        // nearer the lift's end than the other one
        if (ready) {
            const bool riding = lh(pv + 0xb8) != 0 && moby_hero_on_l1(m) != 0;
            bool go = riding;
            if (!go) {
                const float d_last = vec3_distance(V(hero), V(last));
                const float d_first = vec3_distance(V(hero), V(path + 0x10));
                go = where == 0 ? d_first < d_last : d_last < d_first;
            }
            if (go) set_off(where == 0 ? -1.0f : 1.0f);
        }
    } else if (where == 2) {
        if (lw(pv + 0xc8) == 0 && moby_hero_on_l1(m) != 0) {
            sh(g::PLAYER + 0x1f4, 4);
            sh(g::PLAYER + 0x1f2, 4);
        }
        bool waiting = lh(pv + 0xba) != 0;
        bool stop_sound = false;
        if (!waiting) {
            bool hop = false;  // the hero stepping on a descending lift (pvar +0xbc < 0) from just above it
            if (!(lwc1(pv + 0xbc) < 0.0f)) {
            } else if (!(vec2_distance(V(m + 0x10), V(hero)) < u2f(0x40351eb8u))) {
            } else if (!(abs_f(lwc1(g::PLAYER + 0x88) - lwc1(m + 0x18)) < 4.0f)) {
            } else if (!(u2f(0x3f666666u) < lwc1(m + 0x18) - lwc1(g::PLAYER + 0x88))) {
            } else {
                hop = true;
            }
            if (hop) {
                if (lh(pv + 0xba) == 0) sh(pv + 0xba, static_cast<uint32_t>(frame_scale_i(0x1e)));
                waiting = true;
            }
        }
        if (waiting) {
            stop_sound = true;
        } else {
            const int32_t cur = lw(pv + 0xc4);
            float scale = lwc1(0x0015ed6c);
            if (sub_0028d8c0(m, cur) == 0) {
                sw(pv + 0xc4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
                scale = lwc1(0x0015ed6c);
            }
            float rate = lwc1(pv + 0xa8) + lwc1(pv + 0xac) * scale;
            swc1(pv + 0xa8, rate);
            const float rate_abs = abs_f(rate);
            if (abs_f(lwc1(pv + 0xac)) < rate_abs) swc1(pv + 0xa8, lwc1(pv + 0xac));
            const float phase = lwc1(pv + 0xa4) + lwc1(pv + 0xa8);
            swc1(pv + 0xa4, phase);
            const float half = u2f(0x3f000000u);
            if (half < abs_f(phase - half)) {  // (past an end of the path)
                if (0.0f < lwc1(pv + 0xac)) {
                    sb(m + 0xbc, 0);
                    swc1(pv + 0xa4, 1.0f);
                } else {
                    sb(m + 0xbc, 1);
                    swc1(pv + 0xa4, 0.0f);
                }
                sw(pv + 0xa8, 0);
                sh(pv + 0xba, static_cast<uint32_t>(frame_scale_i(0xf)));
                moby_sound_release_l1(m, lw(pv + 0xc4));
                sw(pv + 0xc4, 0xffffffffu);
            }
            const int32_t count = lw(path);
            const int32_t seg = float_to_int(int_to_float(count - 1) * lwc1(pv + 0xa4));
            const float frac = int_to_float(count - 1) * lwc1(pv + 0xa4) - int_to_float(seg);
            if (seg == lw(path) - 1) {
                lq_sq(m + 0x10, path + static_cast<uint32_t>(seg) * 16 + 0x10);
            } else {
                const uint32_t p0 = path + static_cast<uint32_t>(seg) * 16 + 0x10;
                Vec4 d;
                vec3_sub(d, V(p0 + 0x10), V(p0));
                vec3_scale(d, d, frac);
                vec3_add(V(m + 0x10), d, V(p0));
            }
            swc1(pv + 0xbc, lwc1(m + 0x18) - start.z);
        }
        if (stop_sound) {
            sw(pv + 0xa8, 0);
            moby_sound_release_l1(m, lw(pv + 0xc4));
            sw(pv + 0xc4, 0xffffffffu);
        }
    }
    // the platform's attach frame: where the lift moved to this frame, relative to its start (pvar +0x60)
    Vec4 moved;
    vec3_sub(moved, V(m + 0x10), start);
    attach_frame_set_v(pv + 0x60, moved, V(m + 0x40), V(m + 0x40));
    if (moby_hero_on_l1(m) == 0 && 2.0f < vec2_distance(V(m + 0x10), V(hero)) && lh(pv + 0xba) == 0) sh(pv + 0xb8, 1);
}

// Class 665, a door half: slides between its start (pvars +4 / +8, set on the first frame) and the point
// +0xc / +0x10 as far as the crank (class 0x118, moby index +0) has turned (its pvar +0, 0..1), with
// the door sound on while it moves and a stop sound at an end.
// orig: L1:0x002f4710
void door_half_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        swc1(pv + 4, lwc1(m + 0x10));
        sw(pv + 0x14, 0xffffffffu);
        swc1(pv + 8, lwc1(m + 0x14));
        sw(pv + 0x18, 0);
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;
    if (lw(pv) == -1) return;
    if (lwc1(pv + 0xc) == 0.0f) return;
    if (lwc1(pv + 0x10) == 0.0f) return;
    const uint32_t crank = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(lw(pv)) << 8);
    if (lh(crank + 0xa6) != 0x118) return;
    const uint32_t q = lwu(crank + 0x78);
    swc1(m + 0x10, lwc1(pv + 4) + (lwc1(pv + 0xc) - lwc1(pv + 4)) * lwc1(q));
    swc1(m + 0x14, lwc1(pv + 8) + (lwc1(pv + 0x10) - lwc1(pv + 8)) * lwc1(q));
    const float f = lwc1(q);
    if (f == lwc1(pv + 0x18)) {
        if (sub_0028d8c0(m, lw(pv + 0x14)) != 0) {
            moby_sound_release_l1(m, lw(pv + 0x14));
            sw(pv + 0x14, 0xffffffffu);
        }
    } else if (f == 1.0f || f == 0.0f) {
        if (sub_0028d8c0(m, lw(pv + 0x14)) != 0) {
            moby_sound_release_l1(m, lw(pv + 0x14));
            sw(pv + 0x14, 0xffffffffu);
        }
        moby_sound_start(1, 0, m);
    } else if (sub_0028d8c0(m, lw(pv + 0x14)) == 0) {
        sw(pv + 0x14, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }
    swc1(pv + 0x18, lwc1(q));
}

// Classes 768 / 769, the double door's halves (class 0x300 = 768 the left one): slide out along the
// moby's angle (+0x48) by pvar +0x10 (0..1) times 2 (the left one) or -2, opening when the hero comes
// within +0x14 (xy) and 2 up (the sound on class 0x300 only), closing when he and the camera are 1.1
// times that away.
// orig: L1:0x002fed68
void door_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hero = 0x0013f3d0;
    uint32_t next = lbu(m + 0x20);  // (the state this frame ends in)
    switch (lbu(m + 0x20)) {
    case 0:
        sw(pv, lwu(m + 0x10));
        sw(pv + 4, lwu(m + 0x14));
        sw(pv + 8, lwu(m + 0x18));
        sw(pv + 0xc, lwu(m + 0x1c));
        swc1(pv + 0x10, 0.0f);
        next = 1;
        break;
    case 1:
        if (!(vec2_distance(V(pv), V(hero)) < lwc1(pv + 0x14))) break;
        if (!(abs_f(lwc1(m + 0x18) - lwc1(hero + 8)) < 2.0f)) break;
        next = 2;
        if (lh(m + 0xa6) == 0x300) moby_sound_start(0, 0, m);
        break;
    case 2: {
        const float t = lwc1(pv + 0x10) + fpu_div(1.0f, frame_scale_f(20.0f));
        swc1(pv + 0x10, t);
        if (t < 1.0f) break;
        swc1(pv + 0x10, 1.0f);
        next = 3;
        break;
    }
    case 3:
        if (vec2_distance(V(pv), V(hero)) <= lwc1(pv + 0x14) * u2f(0x3f8ccccdu)) break;
        if (vec2_distance(V(pv), V(OV(0x00166dc0))) <= lwc1(pv + 0x14) * u2f(0x3f8ccccdu)) break;
        next = 4;
        if (lh(m + 0xa6) == 0x300) moby_sound_start(0, 0, m);
        break;
    case 4:
        if (vec2_distance(V(pv), V(hero)) < lwc1(pv + 0x14)) {
            if (abs_f(lwc1(m + 0x18) - lwc1(hero + 8)) < 2.0f) {
                next = 2;
                break;
            }
        }
        {
            const float t = lwc1(pv + 0x10) - fpu_div(1.0f, frame_scale_f(20.0f));
            swc1(pv + 0x10, t);
            if (0.0f < t) break;
            swc1(pv + 0x10, 0.0f);
            next = 1;
        }
        break;
    default:
        break;
    }
    if (next != lbu(m + 0x20)) sb(m + 0x20, next);
    float dx = cos_vu(lwc1(m + 0x48)) * lwc1(pv + 0x10);
    dx = lh(m + 0xa6) == 0x300 ? dx + dx : dx * -2.0f;
    float dy = sin_vu(lwc1(m + 0x48)) * lwc1(pv + 0x10);
    dy = lh(m + 0xa6) == 0x300 ? dy + dy : dy * -2.0f;
    swc1(m + 0x10, lwc1(pv) + dx);
    swc1(m + 0x14, lwc1(pv + 4) + dy);
}

// Classes 703 / 715, the piston platforms: a platform that waits (pvar +0xa6 frames), then rises or
// sinks by pvar +0xa8 over pvar +0xb0 * 60 frames (the direction in m+0xbc: 0 or 1 is the end it
// rests at), with the piston sound; it keeps its attach frame (pvar +0x60) for what rides it.
// orig: L1:0x002f95c0
void piston_platform_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 start = V(m + 0x10);
    timer_tick_i16(ee_ref<int16_t>(pv + 0xa6));
    if (lbu(m + 0x20) == 0) {
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 0xd);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        sw(pv + 0xa0, lwu(m + 0x18));
        sb(m + 0xbc, lbu(pv + 0xa4));
        sh(pv + 0xa6, static_cast<uint32_t>(frame_scale_i(0x78)));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sw(pv + 0xb4, 0xffffffffu);
    }
    const uint32_t b = lbu(m + 0xbc);
    if (b == 1) {
        if (lh(pv + 0xa6) == 0) {
            sb(m + 0xbc, 2);
            const float f = int_to_float(frame_scale_i(float_to_int(lwc1(pv + 0xb0) * 60.0f)));
            swc1(pv + 0xac, -fpu_div(lwc1(pv + 0xa8), f));
            sh(pv + 0xa6, static_cast<uint32_t>(frame_scale_i(float_to_int(lwc1(pv + 0xb0) * 60.0f))));
            moby_sound_start(1, 0, m);
        }
    } else if (b > 1) {
        if (b == 2) {
            swc1(m + 0x18, lwc1(m + 0x18) - lwc1(pv + 0xac));
            if (lh(pv + 0xa6) == 0) {
                sb(m + 0xbc, lwc1(pv + 0xac) < 0.0f ? 0u : 1u);
                if (lbu(m + 0xbc) == lbu(pv + 0xa4)) swc1(m + 0x18, lwc1(pv + 0xa0));
                sh(pv + 0xa6, static_cast<uint32_t>(frame_scale_i(0x78)));
            }
        }
    } else if (b == 0 && lh(pv + 0xa6) == 0) {
        sb(m + 0xbc, 2);
        const float f = int_to_float(frame_scale_i(float_to_int(lwc1(pv + 0xb0) * 60.0f)));
        swc1(pv + 0xac, fpu_div(lwc1(pv + 0xa8), f));
        sh(pv + 0xa6, static_cast<uint32_t>(frame_scale_i(float_to_int(lwc1(pv + 0xb0) * 60.0f))));
        moby_sound_start(0, 0, m);
    }
    Vec4 moved;
    vec3_sub(moved, V(m + 0x10), start);
    attach_frame_set_v(pv + 0x60, moved, V(m + 0x40), V(m + 0x40));
}

// Class 705, a fan: on its first frame takes the hero's colour and picks a speed in -1..1 (pvar +0)
// and a direction (+4); each frame it pushes whatever is within 1.2 (mobys_in_sphere, flags 0x10),
// speeds up toward its direction at 4 a second, turns round every 90 frames when it has reached
// it, and spins the moby by -4 pi times the speed a second.
// orig: L1:0x002f9bf0
void fan_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        float dir = -1.0f;
        moby_copy_hero_color(m);
        sb(m + 0x20, 1);
        swc1(pv, rand_range_f(-1.0f, 1.0f));
        if ((libc_rand() & 1) != 0) dir = 1.0f;
        swc1(pv + 4, dir);
    }
    const uint32_t rec = ee_stack_alloc(0x30);
    sw(rec + 0x10, m);
    sw(rec + 0x14, 0x10001);
    sw(rec + 0x1c, 0);
    vec_clear(V(rec));
    sw(rec + 0x20, 0);
    mobys_in_sphere(m + 0x10, u2f(0x3f99999au), 0x10, m, rec);
    float f1 = lwc1(0x0015ed6c);
    if (lwc1(pv + 4) == lwc1(pv)) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) != 0) {
            swc1(pv + 4, -lwc1(pv + 4));
            sw(pv + 8, static_cast<uint32_t>(frame_scale_i(0x5a)));
        }
        f1 = lwc1(0x0015ed6c);
    }
    const float v = lwc1(pv) + f1 * 4.0f * lwc1(pv + 4);
    swc1(pv, v);
    if (1.0f < v) swc1(pv, 1.0f);
    if (lwc1(pv) < -1.0f) swc1(pv, -1.0f);
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(0x0015ed6c) * u2f(0xc1490fdbu) * lwc1(pv)));
    ee_stack_free(0x30);
}

// Class 746, a bridge half: swings down (m+0x44 = pvar +0x70 * 0.628 radians) about its hinge (pvar
// +0x60, 11 below, 11 long) once its flag byte is set (m+0xbc == 2, or the level's collected
// entry of m+0xb0 is 0xff); it sinks pvar +0x70 from 1 to 0 at half a second a frame with the
// sound on, then rests (state 1).
// orig: L1:0x002fb8a8
void bridge_half_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        if (lbu(m + 0xbc) == 2 || lbu(0x0014c050 + lbu(m + 0xb0) + (static_cast<uint32_t>(lw(0x0015ed84)) << 4)) == 0xff) {
            sb(m + 0x20, 2);
            sb(m + 0xbc, 0);
        }
    } else if (st == 0) {
        lq_sq(pv + 0x60, m + 0x10);
        swc1(pv + 0x70, 1.0f);
        sw(pv + 0x74, 0xffffffffu);
        sb(m + 0x20, 1);
    } else if (st == 2) {
        const float t = lwc1(pv + 0x70) - lwc1(0x0015ed6c) * u2f(0x3f000000u);
        swc1(pv + 0x70, t);
        if (t < 0.0f) swc1(pv + 0x70, 0.0f);
        if (lwc1(pv + 0x70) == 0.0f) {
            moby_sound_release_l1(m, lw(pv + 0x74));
            sw(pv + 0x74, 0xffffffffu);
            sb(m + 0x20, 1);
        } else if (sub_0028d8c0(m, lw(pv + 0x74)) == 0) {
            sw(pv + 0x74, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        }
    }
    Vec4 base = {};
    base.z = u2f(0xc1300000u);
    swc1(m + 0x44, lwc1(pv + 0x70) * u2f(0x3f20d97cu));
    vec3_add(base, base, V(pv + 0x60));
    const float r = u2f(0x41300000u);
    Vec4 out = {};
    out.x = cos_vu(lwc1(m + 0x48)) * r * sin_vu(lwc1(m + 0x44));
    out.y = sin_vu(lwc1(m + 0x48)) * r * sin_vu(lwc1(m + 0x44));
    out.z = cos_vu(lwc1(m + 0x44)) * r;
    vec3_add(V(m + 0x10), base, out);
}

}  // namespace rac1
