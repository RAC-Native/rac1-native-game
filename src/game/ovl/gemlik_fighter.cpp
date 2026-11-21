// Gemlik Base's (level 13) class 111: a fighter of a squadron in the space fight. Placed hidden, it waits for its
// squadron's leader (the moby of its +0xbc) to call it (the leader's +0xbc 4), starts at the point of its path nearest a
// point 40 behind the camera and flies along the path; Ratchet flying his ship near it, it attacks for ten seconds at
// most (fewer than three of the squadron at a time), firing class 1017 shots (shared_ship_shots.h) a little ahead of the
// ship with a spread that narrows; hit, it shakes, its health gone it explodes (a pickup now and then) and hides until
// the leader calls again. Level 13 only: the addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the
// base ELF's bare. Registered in gemlik_mobys.cpp.
//
// Its pvars: +0x20 its hit record (+0x20 its health, +0x24 its full health, +0x26 its shake's time), +0x60 its colour
// pulse, +0x70 its velocity, +0x84 / +0x88 its pitch's and its yaw's speeds, +0x90 its start, +0xa0 its path, +0xa4 its
// shots' cooldown, +0xa8 its frames out of sight, +0xac / +0xb0 its spread and the spread's angle, +0xb4 the point it
// flies to and +0xb6 the one it comes from, +0xb8 its attack's time, +0xbc its leader (a moby index, -1 none), +0xcc
// its speed, +0xd0 its direction along the path (+1 / -1), +0xd2 the time before it may turn round, +0xd4 its engine's
// hum (a sound slot, -1 none). The leader's pvars: +0x10e the squadron's fighters alive, +0x10f the ones attacking this
// frame (each attacker counts itself). States (the table L13:0x1f4730): 0 set up, 1 wait for the leader's call, 2 fly
// (its +0xbc: 0 approach, 1 attack, 2 cool down), 3 explode, 4 hidden until the leader's +0xbc leaves 4. Its place is
// kept inside the space's bounds (20 .. 1003 on each axis) but on retiring and on hiding.
#include <game/lib/libc.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
// the level's moby of index `idx` (-1 none): 0 when none, gone or retiring
uint32_t live_moby(int32_t idx) {
    if (idx == -1) return 0;
    const uint32_t t = lwu(L13_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
    if (t == 0 || !alive(t)) return 0;
    return t;
}
// point `i` of the path `path` (+0 its count, its points from +0x10)
uint32_t path_point(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }
// the sound slot `slot` plays for `m`
bool slot_playing_for(int32_t slot, uint32_t m) {
    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
    return lwu(e + 0x88) == m && lbu(e + 0x74) != 0;
}
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position (+0x80)
}  // namespace

// The fighter's hits (none while it explodes): its health taken (shaken a little along the hit and at random, its
// pulse), or, gone, it explodes (state 3).
// orig: L13:0x002c37e0
void gml_2c37e0_l13(uint32_t m, uint32_t pv, uint32_t hs) {
    const Frame f(144);
    const uint32_t sp = f.sp;
    if (lbu(m + 0x20) != 3) {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        int32_t st = lw(sp + 0x10);
        const int32_t react = moby_hit_react_r(m, hit, hs, 0, st, nullptr, nullptr, 4);
        sw(sp + 0x10, static_cast<uint32_t>(st));
        if (react == 1 || react == 2) sw(hs, 0);  // (the table L13:0x1f46d0: 1 / 2 0x002c3874, the rest 0x002c3878)
        if (st >= 2) {
            const float health = lwc1(hs), dmg = lwc1(hit + 0x2c);
            if (health <= dmg) {
                sw(hs, 0);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                sb(pv + 0x67, 0x78);
                color_pulse_start(m, pv + 0x60);
                sb(m + 0x20, 3);
            } else {
                swc1(hs, fpu_sub(health, dmg));
                sb(pv + 0x67, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pv + 0x60);
                vec3_set_length(V(sp), V(hit + 0x10), 0.1f);
                vec3_add(V(m + 0x10), V(m + 0x10), V(sp));
                rand_sphere_point(sp, -0.1f, 0.1f);
                vec3_add(V(m + 0x10), V(m + 0x10), V(sp));
            }
        }
        sb(m + 0xa4, 0xff);
    }
    color_pulse_update(m, pv + 0x60);
}

// The point of the path `path` nearest `at` at the distance `bias` (the fighter passes 0), looked for from `start` both
// ways, half of the path each way: a point k steps away must be nearer by 0.4 k (the score kept is the distance itself,
// plus 0.1 k on the way forward). The point's index.
// orig: L13:0x002c3ac8
int32_t gml_2c3ac8_l13(uint32_t at, uint32_t path, int32_t start, float bias) {
    float best = u2f(0x51ba43b7u);  // (1e11)
    int32_t found = start;
    for (int32_t k = 0; k < lw(path) / 2; ++k) {
        const int32_t n = lw(path);
        int32_t i = ee_rem(start + k, n);
        float d = vec3_distance(V(path_point(path, i)), V(at));
        float score = fpu_add(abs_f(fpu_sub(d, bias)), fpu_mul(int_to_float(k), 0.4f));
        if (score < fpu_add(best, fpu_mul(int_to_float(k), 0.1f))) {
            best = d;
            found = i;
        }
        i = ee_rem(n + start - k, n);
        d = vec3_distance(V(path_point(path, i)), V(at));
        score = fpu_add(abs_f(fpu_sub(d, bias)), fpu_mul(int_to_float(k), 0.4f));
        if (score < best) {
            best = d;
            found = i;
        }
    }
    return found;
}

// The fighter turned toward `yaw` / `pitch` (yaw_turn_damped, angle_seek_rate: 4 turns a second at most), rolled into
// its turn (less the more it rolls already, within a quarter turn), the roll eased back by 2 % a frame.
// orig: L13:0x002c3988
void gml_2c3988_l13(uint32_t m, uint32_t pv, float yaw, float pitch) {
    const float k8 = u2f(0x41c90fdbu), k2 = u2f(0x40c90fdbu);  // (8 pi, 2 pi)
    yaw_turn_damped(m, pv + 0x88, yaw, fpu_mul(dt_sq(), k8), fpu_mul(dt_sq(), k2), fpu_mul(dt(), k8));
    swc1(m + 0x44, angle_seek_rate(pv + 0x84, lwc1(m + 0x44), pitch, fpu_mul(dt_sq(), k8), fpu_mul(dt_sq(), k2), fpu_mul(dt(), k8)));
    const float quarter = u2f(0x3fc90fdbu);
    const float room = fpu_div(fpu_sub(quarter, abs_f(lwc1(m + 0x40))), quarter);
    const float roll = fpu_add(lwc1(m + 0x40), fpu_mul(-lwc1(pv + 0x88), room));
    swc1(m + 0x40, quarter < roll ? quarter : roll);
    if (lwc1(m + 0x40) < -quarter) swc1(m + 0x40, -quarter);
    swc1(m + 0x40, fpu_mul(lwc1(m + 0x40), u2f(0x3f7ae148u)));  // (0.98)
    (void)angle_wrap_mod(lwc1(m + 0x40));  // (the original returns it; none of its callers reads it)
}

// The fighter flown one frame from the path's point +0xb6 toward +0xb4 (the next point once it is within its speed of
// it, wrapping at the path's ends), its velocity turned toward the point the more the nearer it is, and, `turn` set,
// turned toward the hero. 1 when the point wrapped.
// orig: L13:0x002c3c88
int32_t gml_2c3c88_l13(uint32_t m, uint32_t pv, bool turn) {
    const Frame f(160);
    const uint32_t sp = f.sp;
    const uint32_t path = lvd::path(lw(pv + 0xa0));
    const float done = vec3_distance(V(path_point(path, lh(pv + 0xb6))), V(m + 0x10));
    const float seg = vec3_distance(V(path_point(path, lh(pv + 0xb6))), V(path_point(path, lh(pv + 0xb4))));
    const float left = fpu_sub(seg, done);
    const float speed = lwc1(pv + 0xcc);
    int32_t wrapped = 0;
    float k;
    if ((0.0f <= left && left < speed) || seg < speed) {
        const uint32_t to = lhu(pv + 0xb4);
        sh(pv + 0xb6, to);
        sh(pv + 0xb4, to + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu(pv + 0xd0)))));
        if (!(lh(pv + 0xb4) < lw(path))) {
            sh(pv + 0xb4, 0);
            wrapped = 1;
        }
        if (lh(pv + 0xb4) < 0) {
            sh(pv + 0xb4, lhu(path) - 1);
            wrapped = 1;
        }
        k = 0.0f;
    } else {
        k = fpu_div(done, seg);
    }
    if (k < 0.05f) k = 0.05f;
    if (1.0f < k) k = 1.0f;
    vec3_scale(V(pv + 0x70), V(pv + 0x70), fpu_sub(1.0f, k));
    vec3_sub(V(sp), V(path_point(path, lh(pv + 0xb4))), V(m + 0x10));
    vec3_set_length(V(sp), V(sp), lwc1(pv + 0xcc));
    vec3_scale(V(sp), V(sp), k);
    vec3_add(V(pv + 0x70), V(pv + 0x70), V(sp));
    vec3_set_length(V(pv + 0x70), V(pv + 0x70), lwc1(pv + 0xcc));
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x70));
    if (turn) {
        const float yaw = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
        const float d = vec2_distance(V(m + 0x10), V(HERO_POS));
        const float pitch = -atan2_f(d, fpu_sub(lwc1(HERO_POS + 8), lwc1(m + 0x18)));
        gml_2c3988_l13(m, pv, yaw, pitch);
    }
    return wrapped;
}

// The point `dist` ahead of Ratchet's ship along its heading (its yaw / pitch with its turn speeds +0x90 / +0x94, times
// its speed +0x64) written to `out` (an EE vector); the hero's place when he flies none.
// orig: L13:0x002c3f08
void gml_2c3f08_l13(uint32_t out, float dist) {
    const uint32_t ship = lwu(HERO + 0x15f0);
    if (ship == 0) {
        lq_sq(out, HERO_POS);
        return;
    }
    const uint32_t spv = lwu(ship + 0x78);
    const float yaw = angle_add(lwc1(ship + 0x48), lwc1(spv + 0x90));
    const float pitch = angle_add(lwc1(ship + 0x44), lwc1(spv + 0x94));
    vec_from_spherical_v(V(out), fpu_mul(dist, lwc1(spv + 0x64)), yaw, -pitch);
    vec3_add(V(out), V(out), V(ship + 0x10));
}

// The fighter's path flight: its aim (Ratchet's ship halfway to the leader, 100 from it at most, or 140 ahead of the
// ship without a leader; the leader or the hero when Ratchet flies no ship of class 0x45), the path's point after the
// one nearest the aim (5 ahead or behind, whichever is further from the hero, then on while the aim is within 30 of
// it), the way to it (the shorter one): along the way it flies, its speed eased toward a 60th of the distance (100 a
// second at most); the other way, every ten frames 4 % slower, and turned round when it is near the hero (40), slow or
// out of sight; then flown one frame (`turn`: toward the hero). 1 when its point wrapped, 0 without a path.
// orig: L13:0x002c3fc0
int32_t gml_2c3fc0_l13(uint32_t m, uint32_t pv, bool turn) {
    const Frame f(224);
    const uint32_t sp = f.sp;
    const uint32_t path = lvd::path(lw(pv + 0xa0));
    if (path == 0) return 0;
    const uint32_t aim = sp, ahead_pt = sp + 0x10, behind_pt = sp + 0x20;
    const uint32_t ship = lwu(HERO + 0x15f0);
    const uint32_t lead = live_moby(lw(pv + 0xbc));
    if (ship != 0 && lh(ship + 0xa6) == 0x45) {
        if (lead != 0) {
            vec3_sub(V(aim), V(ship + 0x10), V(lead + 0x10));
            vec3_scale(V(aim), V(aim), 0.5f);
            vec3_clamp_length_vu(V(aim), V(aim), 100.0f);
            vec3_add(V(aim), V(aim), V(lead + 0x10));
        } else {
            gml_2c3f08_l13(aim, 140.0f);
        }
    } else {
        lq_sq(aim, lead != 0 ? lead + 0x10 : HERO_POS);
    }
    const int32_t near = gml_2c3ac8_l13(aim, path, lh(pv + 0xb4), 0.0f);
    int32_t ahead = near + 5;
    if (!(ahead < lw(path))) ahead -= lw(path);
    lq_sq(ahead_pt, path_point(path, ahead));
    const float da = vec3_distance(V(ahead_pt), V(HERO_POS));
    int32_t behind = near - 5;
    if (behind < 0) behind += lw(path);
    lq_sq(behind_pt, path_point(path, behind));
    const float db = vec3_distance(V(behind_pt), V(HERO_POS));
    int32_t step, idx;
    if (db < da) {
        step = 1;
        idx = ahead;
    } else if (da < db || static_cast<int8_t>(lbu(pv + 0xd0)) != 1) {
        step = -1;
        idx = behind;
    } else {
        step = 1;
        idx = ahead;
    }
    for (int32_t k = 0; vec3_distance(V(aim), V(path_point(path, idx))) < 30.0f && k < lw(path); ++k)
        idx = ee_rem(idx + step, lw(path));
    // the shorter way from its point to that one
    const int32_t at = lh(pv + 0xb4);
    int32_t dir;
    if (at < idx) dir = idx - at < lw(path) - idx + at ? 1 : -1;
    else dir = at - idx < lw(path) - at + idx ? -1 : 1;
    const float dist = vec3_distance(V(m + 0x10), V(path_point(path, idx)));
    if (dir == static_cast<int8_t>(lbu(pv + 0xd0))) {
        const float speed = lwc1(pv + 0xcc);
        const float eased = fpu_add(speed, fpu_mul(fpu_sub(fpu_div(dist, 60.0f), speed), fpu_mul(step60(), u2f(0x3d8f5c29u))));  // (0.07)
        const float top = fpu_mul(dt(), 100.0f);
        swc1(pv + 0xcc, top < eased ? top : eased);
    } else if (tick16(pv + 0xd2) != 0) {
        sh(pv + 0xd2, 10);
        swc1(pv + 0xcc, fpu_mul(lwc1(pv + 0xcc), fpu_add(fpu_mul(step60(), u2f(0xbd23d710u)), 1.0f)));  // (-0.04)
        const float dh = vec2_distance(V(m + 0x10), V(HERO_POS));
        const bool slow = lwc1(pv + 0xcc) < fpu_mul(step60(), 0.1f);
        if (dh < 40.0f || slow || lbu(m + 0x31) == 0) {  // turned round
            const uint32_t to = lhu(pv + 0xb4);
            sb(pv + 0xd0, static_cast<uint32_t>(dir));
            sh(pv + 0xb4, lhu(pv + 0xb6));
            sh(pv + 0xb6, to);
        }
    }
    return gml_2c3c88_l13(m, pv, turn);
}

// orig: L13:0x002c4428
void gml_class111_update_l13(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t ship = lwu(HERO + 0x15f0);
    const uint32_t path = lw(pv + 0xa0) == -1 ? 0u : lvd::path(lw(pv + 0xa0));
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4730)
    case 0: {  // (0x002c44c4): set up at its path's second point; retired when it is never drawn nor updated, or has no path
        const bool never = lbu(m + 0x30) == 0 && lhu(m + 0x32) == 0;  // (its +0x30 .. +0x37 masked with 0xffff00ff)
        if (never || path == 0) {
            if (!never) debug_printf_stub();  // (the format L13:0x1f4700 with its uid: the release build prints nothing)
            moby_retire(m);
            return;
        }
        lq_sq(m + 0x10, path_point(path, 0));
        sh(pv + 0xb4, 1);
        sh(pv + 0xb6, lhu(path) - 1);
        lq_sq(pv + 0x90, path_point(path, 1));
        lq_sq(m + 0x10, pv + 0x90);
        sw(pv + 0xa8, static_cast<uint32_t>(frame_scale_i(0x14) + 5));
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sh(m + 0x34, (lhu(m + 0x34) | 1u) & 0xefffu);
        sb(m + 0x20, 1);
        sb(m + 0xbc, 0);
        sw(m + 0x94, 0);
        sb(m + 0x31, 0);
        sb(pv + 0xd0, 1);
        break;
    }
    case 1: {  // (0x002c45a4): wait for the leader's call (its +0xbc 4; or no leader), then start at the path's point
               // nearest a point 40 behind the camera, drawn
        const uint32_t lead = live_moby(lw(pv + 0xbc));
        if (lead != 0 && lbu(lead + 0xbc) != 4) break;
        const uint32_t from = sp + 0x30;
        vec3_set_length(V(from), V(L13_ADDR(0x001672d0)), 40.0f);
        vec3_sub(V(from), V(L13_ADDR(0x001670c0)), V(from));
        const int32_t near = path_nearest_l1(from, path, 0.0f);
        sh(pv + 0xb4, static_cast<uint32_t>(near));
        sh(pv + 0xb6, static_cast<uint32_t>(near));
        lq_sq(pv + 0x90, path_point(path, static_cast<int16_t>(near)));
        lq_sq(m + 0x10, pv + 0x90);
        sh(m + 0x34, (lhu(m + 0x34) & 0xfffeu) | 0x1000u);
        sb(m + 0x31, 1);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (0x002c4684): fly
        gml_2c37e0_l13(m, pv, pv + 0x20);
        const uint32_t target = sp + 0x20;  // Ratchet's ship (or the hero)
        uint32_t spv = 0;                   // the ship's pvars (0: no ship)
        bool near = false;                  // Ratchet flies it (his state 0x32), a ship of class 0x45 / 0x4da within 110
        if (ship != 0 && alive(ship)) {
            spv = lwu(ship + 0x78);
            lq_sq(target, ship + 0x10);
            const float d = vec3_distance(V(m + 0x10), V(target));
            if (lw(HERO + 0x2084) == 0x32) {
                const int32_t cls = lh(ship + 0xa6);
                if ((cls == 0x45 || cls == 0x4da) && d < 110.0f) near = true;
            }
        } else {
            lq_sq(target, HERO_POS);
        }
        if (lbu(m + 0x31) == 0) sw(pv + 0xa8, lwu(pv + 0xa8) + 1);
        else if (near) sw(pv + 0xa8, 0);
        const uint32_t lead = live_moby(lw(pv + 0xbc));
        if (lead == 0 || lbu(lead + 0x20) == 1) {  // (0x002c47c4): no leader, or the leader waits: it hides (no bounds)
            sb(m + 0x20, 4);
            sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
            sw(m + 0x94, 0);
            return;
        }
        const uint32_t lpv = lwu(lead + 0x78);
        if (lw(pv + 0xd4) == -1 || !slot_playing_for(lw(pv + 0xd4), m)) {  // its engine's hum (started again when it stopped)
            sw(pv + 0xd4, 0xffffffffu);
            sw(pv + 0xd4, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        }
        gml_2c3fc0_l13(m, pv, true);
        switch (lbu(m + 0xbc)) {
        case 0:  // (0x002c487c): approach; it attacks the ship flying (its state 4) near it, in sight, fewer than 3 attacking
            if (spv == 0 || lbu(ship + 0x20) != 4 || lbu(m + 0x31) == 0 || !near || lbu(lpv + 0x10f) >= 3) break;
            sw(pv + 0xb8, static_cast<uint32_t>(frame_scale_i(0x258)));
            swc1(pv + 0xac, 1.0f);
            sw(pv + 0xa8, 0);
            sb(m + 0xbc, 1);
            break;
        case 1: {  // (0x002c48e0): attack (counted by the leader: not as the fourth); out of sight or of reach for 20 frames, or
                   // its time out, it cools down
            sb(lpv + 0x10f, lbu(lpv + 0x10f) + 1);
            if (lbu(lpv + 0x10f) >= 4) break;
            if (lbu(m + 0x31) == 0) sw(pv + 0xa8, lwu(pv + 0xa8) + 1);
            if (lbu(m + 0x31) == 0 || !near || !(lw(pv + 0xa8) < frame_scale_i(0x14))) {
                sw(pv + 0xb8, static_cast<uint32_t>(frame_scale_i(0x1e)));
                sb(m + 0xbc, 2);
            }
            if (tick32(pv + 0xb8) != 0) {
                sw(pv + 0xb8, static_cast<uint32_t>(frame_scale_i(0x78)));
                sb(m + 0xbc, 2);
            }
            if (tick32(pv + 0xa4) == 0) break;
            // a shot when the frame's load allows (both factors below 0.9), the next one later the higher the load
            const float load_a = lwc1(L13_ADDR(0x0015f5d0)), limit = u2f(0x3f666666u);
            if (!(load_a < limit) || !(lwc1(L13_ADDR(0x0015f5d4)) < limit)) break;
            const int32_t wait = frame_scale_i(6);
            sw(pv + 0xa4, static_cast<uint32_t>(wait + float_to_int(fpu_mul(fpu_add(lwc1(L13_ADDR(0x0015f5d0)), lwc1(L13_ADDR(0x0015f5d4))), u2f(0x3fa66666u)))));
            if (spv != 0) {
                // aimed at the ship moved by the spread (+0xac times 5, at the angle +0xb0 about its heading), jittered,
                // led by the ship's travel in the shot's time
                const uint32_t up = sp + 0x60, side = sp + 0x70, aim = sp + 0x30, lead_v = sp + 0x50, muzzle = sp + 0x40;
                sq_zero(up);
                swc1(up + 8, 1.0f);
                vec3_cross_ba(V(side), V(spv + 0x10), V(up));
                vec3_cross_ba(V(up), V(side), V(spv + 0x10));
                vec3_set_length(V(up), V(up), fpu_mul(fpu_mul(lwc1(pv + 0xac), 5.0f), sin_vu(lwc1(pv + 0xb0))));
                vec3_set_length(V(side), V(side), fpu_mul(fpu_mul(lwc1(pv + 0xac), 5.0f), cos_vu(lwc1(pv + 0xb0))));
                vec3_add(V(aim), V(target), V(up));
                vec3_add(V(aim), V(aim), V(side));
                metal_25f8e0(aim, u2f(0x3f19999au));  // (0.6)
                const float d = vec2_distance(V(m + 0x10), V(HERO_POS));
                const float sspeed = lwc1(spv + 0x64);
                vec3_set_length(V(lead_v), V(spv + 0x10), fpu_mul(sspeed, fpu_div(d, fpu_add(sspeed, fpu_mul(dt(), 50.0f)))));
                vec3_add(V(aim), V(aim), V(lead_v));
                vec3_sub(V(aim), V(aim), V(m + 0x10));
                vec3_set_length(V(muzzle), V(aim), u2f(0x40266666u));  // (2.6)
                vec3_add(V(muzzle), V(muzzle), V(m + 0x10));
                class1017_launch(m, aim, muzzle, 121.0f, fpu_mul(dt(), 50.0f), 5.0f);
                moby_sound_start(0, 0, m);
            }
            // the spread narrowed by 6 to 8 % (a frame's worth), its angle turned 20 to 40 degrees a second (back below a
            // quarter turn)
            const float r = rand_range_f(u2f(0x3f6b851fu), u2f(0x3f70a3d7u));  // (0.92, 0.94)
            const bool back = u2f(0x3fc90fdbu) < lwc1(pv + 0xb0);
            swc1(pv + 0xac, fpu_mul(lwc1(pv + 0xac), fpu_add(fpu_mul(fpu_sub(r, 1.0f), step60()), 1.0f)));
            const float turn = fpu_mul(fpu_mul(rand_range_f(20.0f, 40.0f), u2f(0x3c8efa35u)), dt());
            swc1(pv + 0xb0, back ? fpu_sub(lwc1(pv + 0xb0), turn) : fpu_add(lwc1(pv + 0xb0), turn));
            break;
        }
        case 2:  // (0x002c4c74): cool down
            if (tick32(pv + 0xb8) != 0) sb(m + 0xbc, 0);
            break;
        default:
            break;
        }
        break;
    }
    case 3: {  // (0x002c4c8c): explode (its hum stopped), a pickup in 7 of 10, one fewer alive in the squadron, then hide
        const int32_t snd = lw(pv + 0xd4);
        if (snd != -1) {
            if (slot_playing_for(snd, m)) sub_0028d918(snd);
            sw(pv + 0xd4, 0xffffffffu);
        }
        const uint32_t to_cam = sp + 0x30, at = sp + 0x40;
        vec3_sub(V(to_cam), V(L13_ADDR(0x001670c0)), V(m + 0x10));
        explosion_effect(m, to_cam, 0, 5, 3, 0xa, -1, 0, 1, -1, 0, 0.0f, 0.0f, 4.0f, 3.0f, 9.0f, 1.1f, 20.0f);
        vec3_set_length(V(to_cam), V(to_cam), 4.0f);
        vec3_sub(V(at), V(m + 0x10), V(to_cam));
        vec3_scale(V(to_cam), V(pv + 0x70), 0.5f);  // (left unused by the original)
        switch (static_cast<uint32_t>(ee_rem(libc_rand(), 10))) {  // (the table L13:0x1f4750)
        case 0:  // (0x002c4dd8)
            spawn_at_place(at, 0xe0);
            break;
        case 1: case 2: case 3: case 4: case 5: case 6:  // (0x002c4dec)
            spawn_at_place(at, 0xe4);
            break;
        default:  // (7 .. 9: 0x002c4df8)
            break;
        }
        if (const uint32_t lead = live_moby(lw(pv + 0xbc)); lead != 0) {
            const uint32_t lpv = lwu(lead + 0x78);
            if (lbu(lpv + 0x10e) != 0) sb(lpv + 0x10e, lbu(lpv + 0x10e) - 1);
        }
        sb(m + 0x20, 4);
        sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
        sw(m + 0x94, 0);
        break;
    }
    case 4: {  // (0x002c4e78): hidden; once the leader's +0xbc leaves 4, its health back and set up again
        const uint32_t lead = live_moby(lw(pv + 0xbc));
        if (lead == 0 || lbu(lead + 0xbc) == 4) break;
        swc1(pv + 0x20, static_cast<float>(lh(pv + 0x24)));
        sb(m + 0x20, 0);
        break;
    }
    default:
        break;
    }
    // the space's bounds
    for (uint32_t k = 0x10; k < 0x1c; k += 4) {
        if (lwc1(m + k) < 20.0f) swc1(m + k, 20.0f);
        if (1003.0f < lwc1(m + k)) swc1(m + k, 1003.0f);
    }
}

}  // namespace rac1
