// Oltanis (level 14), class 685: a gondola between stations on two paths (pvar +0xa0: the main path, +0xb8: the branch;
// m+0xbc picks the path, +0xac the way along it: 1 forward, -1 back). It waits at a station (1) pulsing its colour while the
// hero is within 40; the hero stepping into one of its three boxes (+0x118 / +0x11c / +0x120) sets which path and way the
// next ride takes. Once he stands on it within 1 of its centre (on the PAL 2.00 disc, and with 1.02 fix 49, only while its
// up row and his agree) it starts (2): it hums (sound 0, channel +0x12c, volume +0x130), slides along the path (+0xa4 /
// +0xa8 the cursor, +0xb0 the distance run, +0xb4 / +0xbc the paths' lengths) springing toward the path's point (speeds
// +0xc0) and turning from its start angles (+0xd0) toward the path's end angles (+0xe0 / +0xf0), carries the hero (attach
// frame +0x60) and puts the level's camera path +0x114 (11 points) about itself. At the path's end it slows its hum (3)
// and settles (4, or 1 once the hero has stepped off). Pvars: +0x110 the save's byte it reads, +0x124 the ride already
// done (the save bit by uid), +0x126 / +0x128 the hum's volume spring, +0x134 the arrival sound's volume, +0x138 the
// pulse's phase.
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350u;

// a path's last segment: the cursor (+0xa4 segment, +0xa8 its length) put at its end
void cursor_at_end(uint32_t pv, uint32_t path) {
    const int32_t last = lw(path) - 2;
    sw(pv + 0xa4, static_cast<uint32_t>(last));
    swc1(pv + 0xa8, lwc1(path + 0x1cu + (static_cast<uint32_t>(last) << 4)));
}

// a looping path's segments measured into its points' w lanes (the last one back to the first); their sum
float measure_path(uint32_t path, uint32_t total) {
    int32_t k = 0;
    for (; k < lw(path) - 1; ++k) {
        const uint32_t p = path + 0x10u + (static_cast<uint32_t>(k) << 4);
        const float d = vec3_distance(V(p), V(p + 0x10));
        swc1(p + 0xc, d);
        swc1(total, fpu_add(lwc1(total), d));
    }
    swc1(path + 0x1cu + (static_cast<uint32_t>(k) << 4), vec3_distance(V(path + 0x10u + (static_cast<uint32_t>(k) << 4)), V(path + 0x10)));
    return lwc1(total);
}

// orig: L14:0x002eef60 -- the set-up: the two paths measured (+0xb4 / +0xbc), the way back (-1); retired without its main
// path or a camera path of 11 points
void gondola_init(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0xa0) == -1) {
        moby_retire(m);
        return;
    }
    const uint32_t main_path = lvd::path(lw(pv + 0xa0));
    if (lw(main_path) == 0 || lw(pv + 0x114) == -1 || lw(lvd::path(lw(pv + 0x114))) != 0xb) {
        moby_retire(m);
        return;
    }
    sw(pv + 0xb4, 0);
    measure_path(main_path, pv + 0xb4);
    const uint32_t branch = lvd::path(lw(pv + 0xb8));
    sw(pv + 0xbc, 0);
    measure_path(branch, pv + 0xbc);
    swc1(pv + 0xac, -1.0f);
    sw(pv + 0xa4, 0);
    sw(pv + 0xa8, 0);
}

// the current path (m+0xbc: the main path or the branch) and its end angles
uint32_t ride_path(uint32_t m, uint32_t pv) { return lvd::path(lw(pv + (lbu(m + 0xbc) == 0 ? 0xa0u : 0xb8u))); }
uint32_t end_angles(uint32_t m, uint32_t pv) { return pv + (lbu(m + 0xbc) == 0 ? 0xe0u : 0xf0u); }

// orig: L14:0x002ef150 -- one step of the ride: along the path, the springs toward its point, the angles turned by the
// distance run (the pitch only in the ride's middle half); true at the path's end (the cursor put at the end it reached)
bool gondola_ride_step(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float total = lwc1(pv + (lbu(m + 0xbc) == 0 ? 0xb4u : 0xbcu));
    const uint32_t path = ride_path(m, pv);
    const Vec4 goal = V(end_angles(m, pv));
    const float step = fpu_mul(lwc1(L14_ADDR(0x00161d14)), lwc1(0x0015ed6cu));
    Vec4 at{};
    int32_t seg = lw(pv + 0xa4);
    float frac = lwc1(pv + 0xa8);
    const int32_t ended = path_advance_v(path, at, &seg, &frac, 0, fpu_mul(step, lwc1(pv + 0xac)));
    sw(pv + 0xa4, static_cast<uint32_t>(seg));
    swc1(pv + 0xa8, frac);
    spring_to(m + 0x10, pv + 0xc0, at.x, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0x14, pv + 0xc4, at.y, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0x18, pv + 0xc8, at.z, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    const float run = fpu_add(lwc1(pv + 0xb0), step);
    float t = fpu_div(run, total);
    swc1(pv + 0xb0, run);
    if (1.0f < t) t = 1.0f;
    if (ended != 0) t = 1.0f;
    const float way = lwc1(pv + 0xac);
    if (way < 0.0f) t = fpu_sub(1.0f, t);
    float pitch_t = 1.0f;
    if (0.0f < way) {  // (forward: the pitch turns in the ride's second half; back: in its first)
        const float past = fpu_sub(t, 0.5f);
        pitch_t = 0.5f < t ? fpu_add(past, past) : 0.0f;
    } else if (t <= 0.5f) {
        pitch_t = fpu_add(t, t);
    }
    if (1.0f < pitch_t) pitch_t = 1.0f;
    swc1(m + 0x40, angle_lerp(lwc1(pv + 0xd0), goal.x, pitch_t));
    swc1(m + 0x44, angle_lerp(lwc1(pv + 0xd4), goal.y, t));
    swc1(m + 0x48, angle_lerp(lwc1(pv + 0xd8), goal.z, t));
    if (ended == 0) return false;
    if (0.0f < lwc1(pv + 0xac)) {
        cursor_at_end(pv, path);
    } else {
        swc1(pv + 0xa8, 0.0f);
        sw(pv + 0xa4, 0);
    }
    return true;
}

// orig: L14:0x002ef3e0 -- at a station: springing to the path's end (forward) or start (back), and (forward) its angles to
// the end angles (speeds +0x100)
void gondola_hold(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 goal = V(end_angles(m, pv));
    const uint32_t path = ride_path(m, pv);
    const Vec4 at = 0.0f < lwc1(pv + 0xac) ? V(path + (static_cast<uint32_t>(lw(path)) << 4)) : V(path + 0x10);
    spring_to(m + 0x10, pv + 0xc0, at.x, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0x14, pv + 0xc4, at.y, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0x18, pv + 0xc8, at.z, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    if (!(0.0f < lwc1(pv + 0xac))) return;
    spring_to(m + 0x40, pv + 0x100, goal.x, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0x44, pv + 0x104, goal.y, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    spring_to(m + 0x48, pv + 0x108, goal.z, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
}

// orig: L14:0x002ef630 -- the level's camera path +0x114 (11 points) put about the gondola: the level's offsets
// (0x1e03b0) turned by its rows and moved to its place; its index into 0x14162a
void gondola_camera_path(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cam = lvd::path(lw(pv + 0x114));
    for (uint32_t k = 0; k < 11; ++k) {
        const uint32_t p = cam + 0x10u + (k << 4);
        mat3_transform(V(p), V(L14_ADDR(0x001e03b0) + (k << 4)), M(m + 0xc0));
        vec3_add(V(p), V(p), V(m + 0x10));
        swc1(p + 0xc, 1.0f);
    }
    sh(0x0014162au, lhu(pv + 0x114));
}

// orig: L14:0x002ef6f0 -- the colour: a pulse (once a second) while the hero is within 40 across the ground, else back to
// grey (0x80303030)
void gondola_pulse(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t colour = 0x80303030u;
    if (!(40.0f < abs_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)))) && !(40.0f < abs_f(fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84))))) {
        swc1(pv + 0x138, angle_add(lwc1(pv + 0x138), fpu_mul(lwc1(0x0015ed6cu), u2f(0x40c90fdbu))));
        int32_t level = float_to_int(fpu_mul(fpu_sub(fpu_mul(sin_vu(lwc1(pv + 0x138)), 4.0f), 3.0f), 128.0f));
        if (!(level < 129)) level = 0x80;
        else if (!(0x1f < level)) level = 0x20;
        const uint32_t c = static_cast<uint32_t>(level);
        colour = 0x80000000u | (c << 16) | (c << 8) | c;
    }
    sw(m + 0x90, rgba_lerp(lwu(m + 0x90), colour, 0.1f));
}

// the hum or the arrival sound stopped if the channel is still the gondola's and playing
void sound_stop(uint32_t m, int32_t ch) {
    if (ch == -1) return;
    const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
    if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) sub_0028d918(ch);
}

// the hero carried by the gondola's move since the frame's start
void carry_hero(uint32_t m, uint32_t pv, uint32_t old_pos, uint32_t old_ang, uint32_t moved) {
    vec3_sub(V(moved), V(m + 0x10), V(old_pos));
    attach_frame_set(pv + 0x60, moved, old_ang, m + 0x40);
}

}  // namespace

// orig: L14:0x002ee8d8
void olt_class685_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) != 0 && lbu(m + 0xbc) == 2 && lwc1(pv + 0xac) < 0.0f) {  // (the branch's return: forward from its end)
        swc1(pv + 0xac, -lwc1(pv + 0xac));
        cursor_at_end(pv, lvd::path(lw(pv + 0xb8)));
        sb(m + 0xbc, 1);
    }
    const uint32_t frame = ee_stack_alloc(0x40);
    const uint32_t old_pos = frame, old_ang = frame + 0x10, moved = frame + 0x20, volume = frame + 0x30;  // (by address)
    V(old_pos) = V(m + 0x10);
    V(old_ang) = V(m + 0x40);
    // jump table L14:0x001fcb30 (5 entries)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002ee9c4) the set-up, at the main path's start; the ride done before when the uid's byte or save bit says so
        sb(m + 0xbc, 0);
        sb(m + 0x20, 1);
        gondola_init(m);
        V(m + 0x10) = V(lvd::path(lw(pv + 0xa0)) + 0x10);
        V(m + 0x40) = V(pv + 0xd0);
        const int16_t uid = lh(m + 0xb2);
        if (lbu(L14_ADDR(0x001bb930) + static_cast<uint32_t>(static_cast<int32_t>(uid)) + 0x454u) != 0) {
            sh(pv + 0x124, 0);
        } else {
            const uint32_t lvl = lwu(0x0015ed84u);
            const uint32_t word = lwu(0x0014c190u + (lvl << 8) + (static_cast<uint32_t>(uid >> 5) << 2));
            const bool saved = ((static_cast<uint32_t>(static_cast<int32_t>(word) >> (lhu(m + 0xb2) & 0x1fu)) ^ 1u) & 1u) == 0;
            if (saved) sh(pv + 0x124, 0);
            else sh(pv + 0x124, lbu(lwu(pv + 0x110) + (lvl << 4) + 0x0014c050u) == 0xff ? 1u : 0u);
        }
        sb(m + 0x30, 0xff);
        sw(m + 0x90, 0x80303030u);
        sw(pv + 0x134, 0xffffffffu);
        sw(pv + 0x12c, 0xffffffffu);
        sw(pv + 0x130, 0xffffffffu);
        sw(pv + 0x138, 0);
        break;
    }
    case 1: {  // (0x002eeacc) at a station: the boxes set the next ride; the hero aboard starts it
        gondola_pulse(m);
        if (lw(pv + 0x118) >= 0 && point_in_box(HERO + 0x80, lw(pv + 0x118)) != 0) {  // back along the main path
            sb(m + 0xbc, 0);
            swc1(pv + 0xac, -1.0f);
            sw(pv + 0xa4, 0);
            sw(pv + 0xa8, 0);
        }
        if (lw(pv + 0x11c) >= 0 && point_in_box(HERO + 0x80, lw(pv + 0x11c)) != 0) {  // forward along the main path
            sb(m + 0xbc, 0);
            cursor_at_end(pv, lvd::path(lw(pv + 0xa0)));
            swc1(pv + 0xac, 1.0f);
        }
        if (lw(pv + 0x120) >= 0 && point_in_box(HERO + 0x80, lw(pv + 0x120)) != 0) {  // along the branch
            sb(m + 0xbc, 1);
            cursor_at_end(pv, lvd::path(lw(pv + 0xb8)));
            swc1(pv + 0xac, 1.0f);
        }
        if (lwu(HERO + 0x2fc) != m || lh(HERO + 0x30c) != 0) break;
        // 1.02 fix 49 (the PAL 2.00 disc's own L14:0x2f0050): the ride starts only while the gondola's up row and the
        // hero's moby's agree
        if (fixes102_oltanis_gondola_upright() && !(0.0f < vec3_dot(V(m + 0xe0), V(lwu(HERO + 0x2080) + 0xe0)))) break;
        if (!(vec2_distance(V(HERO + 0x80), V(m + 0x10)) < 1.0f)) break;
        const int32_t ch = moby_sound_start(1, 0, m);
        if (ch >= 0) {
            if (lw(pv + 0x134) < 0) sw(pv + 0x134, lwu(0x0013e640u));
            sound_slot_volume_set(ch, lwu(pv + 0x134));
        }
        if (lwc1(pv + 0xac) == -1.0f) sb(m + 0xbc, lh(pv + 0x124) != 0 ? 2u : 0u);
        sb(m + 0x20, 2);
        sw(pv + 0xb0, 0);
        swc1(pv + 0xac, -lwc1(pv + 0xac));
        break;
    }
    case 2: {  // (0x002eec9c) riding
        if (sub_0028d8c0(m, lw(pv + 0x12c)) == 0) {
            const int32_t ch = moby_sound_start(0, 4, m);
            sw(pv + 0x12c, static_cast<uint32_t>(ch));
            if (ch != 0) {
                if (lw(pv + 0x130) < 0) sw(pv + 0x130, lwu(0x0013e5d0u));
                sound_slot_volume_set(lw(pv + 0x12c), lwu(pv + 0x130));
            }
        }
        sw(m + 0x90, rgba_lerp(lwu(m + 0x90), 0x80808040u, 0.1f));
        if (gondola_ride_step(m)) {
            sb(m + 0x20, 3);
            sh(pv + 0x126, lhu(pv + 0x130));
        }
        gondola_camera_path(m);
        carry_hero(m, pv, old_pos, old_ang, moved);
        break;
    }
    case 3: {  // (0x002eed60) arriving: the hum dies down while the springs still move it
        if (u2f(0x3a83126fu) <= vec3_length(V(pv + 0xc0))) {
            if (sub_0028d8c0(m, lw(pv + 0x12c)) == 0) sw(pv + 0x12c, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
            swc1(volume, int_to_float(lh(pv + 0x126)));
            spring_to(volume, pv + 0x128, fpu_mul(int_to_float(lw(pv + 0x130)), 0.25f), u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
            const int32_t now = float_to_int(lwc1(volume));
            sh(pv + 0x126, static_cast<uint16_t>(now));
            if (lw(pv + 0x12c) >= 0) sound_slot_volume_set(lw(pv + 0x12c), static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(now))));
            gondola_camera_path(m);
            break;
        }
        gondola_pulse(m);
        sound_stop(m, lw(pv + 0x12c));
        sw(pv + 0x12c, 0xffffffffu);
        {
            const int32_t ch = moby_sound_start(1, 0, m);
            if (ch >= 0) sound_slot_volume_set(ch, static_cast<uint32_t>(ee_div(lw(pv + 0x134), 3)));
        }
        sb(m + 0x20, 4);
        if (lwu(HERO + 0x2fc) != m) sb(m + 0x20, 1);
        break;
    }
    case 4:  // (0x002eeec4) settled: back to waiting once the hero has stepped off
        gondola_pulse(m);
        if (lwu(HERO + 0x2fc) != m) sb(m + 0x20, 1);
        break;
    default:
        break;
    }
    const uint8_t st = lbu(m + 0x20);
    if (st != 2 && st != 0) {
        gondola_hold(m);
        if (lwu(HERO + 0x2fc) == m) carry_hero(m, pv, old_pos, old_ang, moved);
    }
    ee_stack_free(0x40);
}

}  // namespace rac1
