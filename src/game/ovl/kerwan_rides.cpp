// Kerwan's own moby classes: the hover platforms the hero rides along the level's paths, class 816 (a platform with up to
// five calls: a box the hero stands in calls it along its path in, it waits for him, carries him along its path out and
// goes back along its path home) and class 1012 (a ferry between two ends). Header kerwan_mobys.h, registered in
// kerwan_mobys.cpp. Addresses are Kerwan's (`orig: L3:`); data only Kerwan has is L3_ADDR, the engine's words shared
// with Veldin are the level's own (OV), the base ELF's (the hero 0x13f350, the frame times) are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l3.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position (0x13f350 + 0x80)

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
const float kDeg = u2f(0x3c8efa35u);        // one degree in radians
const float kFourPi = u2f(0x41490fdbu);

// (the rides' tuning is Kerwan's own words: L3:0x161b08 the speed (units a frame at 60 Hz), 0x161b0c its acceleration,
// 0x161b10 / 0x161b14 / 0x161b18 the turn rates (degrees) of the pitch, the heading and the roll, 0x161b1c the roll per unit
// of heading rate and speed)

// the moby's pitch and roll back to level (a ride's waits)
void ride_level_out(uint32_t m, uint32_t pv, uint32_t angles) {
    yaw_seek(m + 0x44, pv + 0x6c, 0.0f, fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161b10)), kDeg), dt_sq()),
             fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161b10)), kDeg), dt_sq()), fpu_mul(dt(), kFourPi));
    yaw_seek(angles, pv + 0x70, 0.0f, fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161b18)), kDeg), dt_sq()),
             fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161b18)), kDeg), dt_sq()), fpu_mul(dt(), kFourPi));
}

// the moby at its path's first point, facing its second
void ride_place_at_start(uint32_t m, int32_t path_idx) {
    const uint32_t path = lvd::path(path_idx);
    lq_sq(m + 0x10, path + 0x10);
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(m + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(m + 0x14))));
}

// the moby's index (+0xb2) taken (its byte in the level's table 0x1bb230 + 0x454, its bit of 0x14c190's level row)
bool ride_taken(uint32_t m) {
    const uint32_t idx16 = lhu(m + 0xb2);
    const int32_t idx = static_cast<int16_t>(idx16);
    if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) return true;
    const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
    return ((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0;
}
}  // namespace

// Class 816 riding (states 2 .. 6) for its talker word.
// orig: L3:0x002d30d8
uint32_t platform_riding_l3(uint32_t m) {
    if (lh(m + 0xa6) != 0x330) return 0;
    return lbu(m + 0x20) - 2u < 5u ? 1u : 0u;
}

// Class 816's help message while the hero stands on it (both of its kinds show the same text).
// orig: L3:0x002d3100
void platform_help_l3(uint32_t m, int32_t kind) {
    (void)m;
    if (kind != 1) hud_message_request(8, TXT(0xbc9));
    else hud_message_request(8, TXT(0xbc9));
}

// A ride along the level's path `path_idx` (the pvars: +0x60 the speed, +0x64 the point it heads for, +0x68 / +0x6c / +0x70
// the heading's / pitch's / roll's rates): the point reached within two frames' steps, the next one; the speed eased
// toward the distance left to the path's end; pitched toward the point (within 15 degrees), turned toward it, rolled into
// the turn; its engine sound (in the word `sound`) kept playing, stopped at the path's end. 1 at the path's end.
// orig: L3:0x002d3918
uint32_t ride_path_l3(uint32_t m, int32_t path_idx, uint32_t sound) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(path_idx);
    const uint32_t to = sp, step = sp + 0x10, seek = sp + 0x20;
    uint32_t done = 0;
    sw(seek, 0);
    lq_sq(to, path + (lwu(pv + 0x64) << 4) + 0x10);
    vec3_sub(V(step), V(to), V(m + 0x10));
    const float d = vec3_length(V(step));
    {
        const float reach = fpu_mul(lwc1(L3_ADDR(0x00161b08)), dt());
        if (d < fpu_add(reach, reach)) {
            sw(pv + 0x64, lwu(pv + 0x64) + 1);
            done = lwu(pv + 0x64) == lwu(path) ? 1u : 0u;
        }
    }
    {
        const float left = vec3_distance(V(m + 0x10), V(path + (lwu(path) << 4)));
        const float accel = fpu_mul(lwc1(L3_ADDR(0x00161b0c)), dt_sq());
        seek_1d(seek, pv + 0x60, left, accel, accel, fpu_mul(lwc1(L3_ADDR(0x00161b08)), dt()));
    }
    vec3_set_length(V(step), V(step), lwc1(pv + 0x60));
    vec3_add(V(m + 0x10), V(step), V(m + 0x10));
    float pitch = -atan2_f(vec2_length(V(step)), lwc1(step + 8));
    const float lim = u2f(0x3e860a92u);
    if (lim < pitch) pitch = lim;
    else if (pitch < -lim) pitch = -lim;
    {
        const float k = fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161b10)), kDeg), dt_sq());
        yaw_seek(m + 0x44, pv + 0x6c, pitch, k, k, fpu_mul(dt(), kFourPi));
    }
    {
        const float heading = atan2_f(fpu_sub(lwc1(to), lwc1(m + 0x10)), fpu_sub(lwc1(to + 4), lwc1(m + 0x14)));
        const float k = fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161b14)), kDeg), dt_sq());
        yaw_seek(m + 0x48, pv + 0x68, heading, k, k, fpu_mul(dt(), kFourPi));
    }
    {
        const float k = fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161b18)), kDeg), dt_sq());
        const float roll = fpu_mul(fpu_mul(lwc1(pv + 0x68), lwc1(pv + 0x60)), lwc1(L3_ADDR(0x00161b1c)));
        yaw_seek(m + 0x40, pv + 0x70, roll, k, k, fpu_mul(dt(), kFourPi));
    }
    const int32_t slot = lw(sound);
    if (done == 0) {
        if (sub_0028d8c0(m, slot) == 0) sw(sound, static_cast<uint32_t>(moby_sound_play(0, 4, m, 0x330)));
    } else {
        if (slot != -1) {
            const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
            if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
        }
        sw(sound, 0xffffffffu);
    }
    ee_stack_free(0xc0);
    return done;
}

// Class 816 (pvars: +0x20 the attach frame, +0x60 .. +0x70 the ride's, +0x74 the call it waits on, +0x78 its sound, +0x7c
// called at all, +0x80 the five calls' boxes, +0x94 / +0xa8 / +0xbc their paths in / out / home, +0xd0 their help kinds;
// the moby's +0xbc the call it serves). States: 0 placed at its first path's start (gone when its index is taken); 1 waits
// for the hero in a call's box (or the call +0x74 it was given) and starts along that call's path in (2); 2 rides in (3);
// 3 levels out; the hero on it (the moby he stands on 0x13f64c, no ride lock 0x13f65e) gets the help message, and with
// the button 0x10 (0x13cae4) in gameplay (mode 8) the camera behind (4); 4 waits for the camera (5); 5 carries the hero
// (his move state 4 while on it) along the path out (6); 6 levels out; the hero 15 away: home along the path home (7);
// without one, the hero 5 away: 1; 7 rides home (1). Every frame its turn (0x2d3c40) and the attach frame from its move.
// With L3:0x161b04 set its paths are drawn as debug particles.
// orig: L3:0x002d3198 (the jump table L3:0x1e33f0 on the state, in order)
void kerwan_class816_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t pv = lwu(m + 0x78);
    talker_word_set(m, platform_riding_l3(m));
    const uint32_t old_pos = sp, old_ang = sp + 0x10, moved = sp + 0x20;
    sw(sp + 0x34, old_ang);
    if (lw(L3_ADDR(0x00161b04)) != 0) {  // the debug view: the three paths of call 0
        static const uint32_t colours[] = {0x800000ffu, 0x8000ff00u, 0x80ff0000u};
        for (uint32_t k = 0; k < 3; ++k) {
            const uint32_t word = pv + 0x94 + k * 0x14;
            for (int32_t i = 0; i < lw(lvd::path(lw(word))); ++i)
                debug_particle_l1(lvd::path(lw(word)) + 0x10 + static_cast<uint32_t>(i) * 0x10, m, 4, colours[k], 0x7f, 2, 0xff,
                                  u2f(0x3eaa7efau));
        }
    }
    lq_sq(old_pos, m + 0x10);
    lq_sq(lwu(sp + 0x34), m + 0x40);
    sw(sp + 0x30, m + 0x40);
    const uint32_t call = pv + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2);
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002d33e0)
        if (lw(pv + 0x94) != -1) ride_place_at_start(m, lw(pv + 0x94));
        else if (lw(pv + 0xa8) != -1) ride_place_at_start(m, lw(pv + 0xa8));
        sh(m + 0x32, 0);
        sb(m + 0x30, 0xff);
        sw(pv + 0x74, 0xffffffffu);
        sb(m + 0x20, 1);
        sw(m + 0x94, 0);
        if (ride_taken(m)) {
            moby_retire(m);
            ee_stack_free(0x100);
            return;
        }
        break;
    }
    case 1:  // (0x002d34c4)
        if (lw(pv + 0x7c) == 0) break;
        for (int32_t c = 0; c < 5; ++c) {
            const int32_t box = lw(pv + 0x80 + static_cast<uint32_t>(c) * 4);
            if (box == -1 && lw(pv + 0x74) != c) continue;
            if (point_in_box(HERO_POS, box) == 0 && lw(pv + 0x74) != c) continue;
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            const uint32_t pc = pv + static_cast<uint32_t>(c) * 4;
            if (lw(pc + 0x94) != -1) ride_place_at_start(m, lw(pc + 0x94));
            else if (lw(pc + 0xa8) != -1) ride_place_at_start(m, lw(pc + 0xa8));
            sb(m + 0xbc, static_cast<uint32_t>(c));
            sb(m + 0x20, 2);
            sh(m + 0x32, 0x80);
            sw(pv + 0x64, 1);
            sw(pv + 0x74, 0xffffffffu);
            swc1(pv + 0x60, fpu_mul(lwc1(L3_ADDR(0x00161b08)), dt()));
            break;
        }
        break;
    case 2:  // (0x002d35c8)
        if (lw(call + 0x94) == -1 || ride_path_l3(m, lw(call + 0x94), pv + 0x78) != 0) {
            sb(m + 0x20, 3);
            sw(pv + 0xe4, 0xffffffffu);
        }
        break;
    case 3:  // (0x002d3604)
        ride_level_out(m, pv, lwu(sp + 0x30));
        if (lwu(0x0013f64c) != m || lh(0x0013f65e) != 0) break;
        platform_help_l3(m, lw(pv + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2) + 0xd0));
        if ((lwu(0x0013cae4) & 0x10u) == 0 || lw(OV(0x0015f594)) != 8) break;
        hero_cam_target_set(m + 0x10, lwc1(m + 0x48), 0);
        sb(m + 0x20, 4);
        sw(pv + 0x64, 1);
        break;
    case 4:  // (0x002d36f0)
        if (sub_00233f38() == 0) sb(m + 0x20, 5);
        break;
    case 5:  // (0x002d3708)
        if (moby_hero_on_l1(m) != 0) {
            sh(0x0013f544, 4);
            sh(0x0013f542, 4);
        }
        if (ride_path_l3(m, lw(call + 0xa8), pv + 0x78) != 0) sb(m + 0x20, 6);
        break;
    case 6:  // (0x002d3754)
        ride_level_out(m, pv, lwu(sp + 0x30));
        if (lw(pv + 0xbc + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2)) != -1) {
            if (15.0f < vec3_distance(V(m + 0x10), V(HERO_POS))) {
                sb(m + 0x20, 7);
                sw(pv + 0x64, 1);
            }
        } else if (5.0f < vec2_distance(V(m + 0x10), V(HERO_POS))) {
            sb(m + 0x20, 1);
        }
        break;
    case 7:  // (0x002d3878)
        if (ride_path_l3(m, lw(call + 0xbc), pv + 0x78) != 0) {
            sh(m + 0x32, 0);
            sb(m + 0x20, 1);
            sw(m + 0x94, 0);
        }
        break;
    default:
        break;
    }
    kerwan_class816_turn(m);
    vec3_sub(V(moved), V(m + 0x10), V(old_pos));
    attach_frame_set(pv + 0x20, moved, lwu(sp + 0x34), lwu(sp + 0x30));
    ee_stack_free(0x100);
}

// Class 1012 at a ferry's end (the hero not yet on it) or while it is busy, for its talker word.
// orig: L3:0x002dcb30
uint32_t ferry_waiting_l3(uint32_t m) {
    if (lh(m + 0xa6) != 0x3f4) return 0;
    if (lbu(m + 0x20) == 0) return 0;
    return lw(lwu(m + 0x78) + 0x7c) == 1 ? 1u : 0u;
}

// Class 1012's help message by its end's kind (1 .. 4: four texts, else none), and its camera (asked for once, then held).
// orig: L3:0x002dcbc8
void ferry_help_l3(uint32_t m, int32_t kind) {
    const uint32_t pv = lwu(m + 0x78);
    switch (kind) {
    case 1: hud_message_request(8, TXT(0xbce)); break;
    case 2: hud_message_request(8, TXT(0xbcf)); break;
    case 3: hud_message_request(8, TXT(0xbcb)); break;
    case 4: hud_message_request(8, TXT(0xbcd)); break;
    default: hud_message_request(8, -1); break;
    }
    if (lw(pv + 0xa4) == -1) {
        sw(pv + 0xa4, static_cast<uint32_t>(camera_request(0xc, 0, L3_ADDR(0x00226960), L3_ADDR(0x002269b0), L3_ADDR(0x002269d0), 0, 0)));
    } else {
        sub_00235e18(lwu(pv + 0xa4), 10);
    }
}

// Class 1012, a ferry between the starts of its two paths (pvars: +0x20 the attach frame, +0x60 .. +0x78 the ride's,
// +0x7c started, +0x80 the end it starts at, +0x84 the hero gone from it, +0x88 a wait, +0x8c called (class 909 sets it),
// +0x90 / +0x94 its two paths, +0x98 / +0x9c the ends' help kinds, +0xa0 the box that calls it, +0xa4 its camera; the
// moby's +0xbc the path it rides). State 0 at its start end (1); 1 waits: first called (+0x8c, or the hero in its box) it
// goes to the end nearer the hero; the hero on it after he left it once gets the help message and with the button 0x10 in
// gameplay the camera (2); else, when the hero is nearer the other end than it is, it goes there (3); 2 waits for the
// camera (3); 3 rides (the hero's move state 4 on it), then waits 15 frames (1). Every frame its turn (0x2dd0f0), the
// attach frame, and the hero off it and 2 away after the wait marks him gone.
// orig: L3:0x002dccc0
void kerwan_class1012_update_l3(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x90) == -1 || lw(pv + 0x94) == -1) return;
    const uint32_t sp = ee_stack_alloc(0xd0);
    talker_word_set(m, ferry_waiting_l3(m));
    lq_sq(sp, m + 0x10);
    lq_sq(sp + 0x10, m + 0x40);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x88)));
    auto end_of = [pv](uint32_t which) { return lvd::path(lw(pv + 0x90 + (which << 2))) + 0x10; };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002dcd80)
        const uint32_t start = lbu(pv + 0x80);
        sb(m + 0xbc, start);
        lq_sq(m + 0x10, end_of(start & 0xffu));
        sw(pv + 0xa4, 0xffffffffu);
        sw(pv + 0x88, 0);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        break;
    }
    case 1: {  // (0x002dcdd4)
        const float ha = vec3_distance(V(HERO_POS), V(end_of(0)));
        const uint32_t hero_end = ha < vec3_distance(V(HERO_POS), V(end_of(1))) ? 1u : 0u;
        const float ma = vec3_distance(V(m + 0x10), V(end_of(0)));
        const uint32_t own_end = ma < vec3_distance(V(m + 0x10), V(end_of(1))) ? 1u : 0u;
        if (lw(pv + 0x7c) == 0) {
            if (lw(pv + 0x8c) == 0 && point_in_box(HERO_POS, lw(pv + 0xa0)) == 0) {
                sw(m + 0x94, 0);
                sh(m + 0x34, lhu(m + 0x34) | 0x41u);
                break;
            }
            sb(m + 0xbc, hero_end);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            lq_sq(m + 0x10, end_of(hero_end));
            sb(m + 0x20, 3);
            sw(pv + 0x7c, 1);
            sw(pv + 0x64, 1);
            sw(pv + 0x84, 0);
            break;
        }
        if (lw(pv + 0x84) != 0 && moby_hero_on_l1(m) != 0) {
            ferry_help_l3(m, lw(pv + ((lbu(m + 0xbc) ^ 1u) << 2) + 0x98));
            if ((lwu(0x0013cae4) & 0x10u) == 0 || lw(OV(0x0015f594)) != 8) break;
            hero_cam_target_set(m + 0x10, lwc1(m + 0x48), 0);
            sb(m + 0xbc, lbu(m + 0xbc) ^ 1u);
            sw(pv + 0x64, 1);
            sw(pv + 0x84, 0);
            sb(m + 0x20, 2);
            break;
        }
        if (hero_end == own_end) break;
        sb(m + 0x20, 3);
        sb(m + 0xbc, lbu(m + 0xbc) ^ 1u);
        sw(pv + 0x64, 1);
        sw(pv + 0x84, 0);
        break;
    }
    case 2:  // (0x002dcfd0)
        if (sub_00233f38() == 0) sb(m + 0x20, 3);
        break;
    case 3:  // (0x002dcfe8)
        if (moby_hero_on_l1(m) != 0) {
            sh(0x0013f544, 4);
            sh(0x0013f542, 4);
        }
        if (ride_path_l3(m, lw(pv + 0x90 + (static_cast<uint32_t>(lbu(m + 0xbc)) << 2)), pv + 0x78) != 0) {
            sb(m + 0x20, 1);
            sw(pv + 0x88, static_cast<uint32_t>(frame_scale_i(0xf)));
        }
        break;
    default:
        break;
    }
    kerwan_class1012_turn(m);
    vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
    attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
    if (moby_hero_on_l1(m) == 0 && 2.0f < vec2_distance(V(m + 0x10), V(HERO_POS)) && lw(pv + 0x88) == 0) sw(pv + 0x84, 1);
    ee_stack_free(0xd0);
}

}  // namespace rac1
