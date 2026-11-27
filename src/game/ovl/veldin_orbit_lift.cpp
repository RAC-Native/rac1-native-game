// Drek's fleet's (level 17) class 1380, ported by hand from the level's own code: a lift that carries the hero between
// two heights (pvars +0x60 / +0x64), its tilt following the height (+0x7c zero), its walk path (pvars +0x80: a path of 11
// points) laid out from the level's template L17:0x1d9f70 on the moby he stands on (0x2eea70); at its first stop the
// camera eased from where it is to beside the lift (0x2ee9c0). The original's order of calls and float operations kept.
// Level 17 only: `orig: L17:`, level data through lvd::*, the level's own data L17_ADDR, base ELF bare. Registered in
// veldin_orbit_mobys.cpp.
//
// The pvars: +0x20 its attach frame (attach_frame_set: the hero carried), +0x60 / +0x64 its upper and lower heights, +0x68
// the height's speed, +0x6c its sound (-1 none), +0x70 / +0x74 the camera's ease and its speed, +0x78 the lights' phase,
// +0x7c nonzero: no tilt, and it waits while the hero walks; +0x80 its path. The moby: +0xbc the state after the camera's
// ease (and nonzero: the hero's state reset at a stop), +0x90 its colour.
//
// States (the table L17:0x1e6ea0): 0 set up (its path of 11 points, else it goes), 1 the camera's ease, 2 / 3 up / down
// with the hero (his path laid out), 4 / 5 up / down without him, 6 / 7 waiting below / above (the lights pulse; the hero
// on it at its centre starts it, the hero near below / above calls it), 8 / 9 stopped (until the hero leaves it).
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/veldin_orbit_mobys.h>
#include <game/ovl/veldin_orbit_own.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his place, +0x2d8 his ground's height, +0x2fc the moby he stands
                                    // on, +0x308 nonzero: walking, +0x30e)

struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// its sound (+0x6c) stopped when it plays
void sound_stop(uint32_t m, uint32_t pv) {
    if (sub_0028d8c0(m, lw(pv + 0x6c)) == 0) return;
    sub_0028d918(lw(pv + 0x6c));
    sw(pv + 0x6c, 0xffffffffu);
}
}  // namespace

// The camera's place beside the lift (`pos`: 2.5 from it toward the camera, 1.5 up) and its angles (`ang`: level, toward
// the hero's point +0xd0).
// orig: L17:0x002ee9c0
void vo_2ee9c0_l17(uint32_t m, uint32_t pos, uint32_t ang) {
    vec3_sub(V(pos), V(L17_ADDR(0x001676c0)), V(m + 0x10));
    vec2_set_length(V(pos), V(pos), 2.5f);
    swc1(pos + 8, 1.5f);
    vec3_add(V(pos), V(pos), V(m + 0x10));
    vec_clear(V(ang));
    swc1(ang + 8, atan2_f(fpu_sub(lwc1(P + 0xd0), lwc1(pos + 0)), fpu_sub(lwc1(P + 0xd4), lwc1(pos + 4))));
    sw(ang + 4, 0);
}

// The hero walking on the moby he stands on, the lift: its path (+0x80, 11 points) laid out from the template
// L17:0x1d9f70 on that moby (turned by its matrix +0xc0, moved to its place), the path's index noted (+0x22da).
// orig: L17:0x002eea70
void vo_2eea70_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lh(P + 0x308) == 0) return;
    const uint32_t on = lwu(P + 0x2fc);
    if (on != m) return;
    uint32_t pt = lvd::path(lw(pv + 0x80)) + 0x10;
    uint32_t tmpl = L17_ADDR(0x001d9f70);
    for (int k = 0; k < 11; ++k, pt += 0x10, tmpl += 0x10) {
        mat3_transform(V(pt), V(tmpl), M(on + 0xc0));
        vec3_add(V(pt), V(pt), V(on + 0x10));
        swc1(pt + 0xc, 1.0f);
        lvd::path_rows_written(pt);
    }
    sh(P + 0x22da, lhu(pv + 0x80));
}

// orig: L17:0x002ee2b0
void vo_class1380_update_l17(uint32_t m) {
    const Frame f(224);
    const uint32_t sp = f.sp, moved = sp, ang0 = sp + 0x10;
    const uint32_t pv = lwu(m + 0x78);
    vec3_scale(V(moved), V(m + 0x10), -1.0f);
    lq_sq(ang0, m + 0x40);
    if (lw(pv + 0x7c) == 0) {  // its tilt by the height: 0 at the top, pi at the bottom
        const float lo = lwc1(pv + 0x64);
        float k = fpu_sub(1.0f, fpu_div(fpu_sub(lwc1(m + 0x18), lo), fpu_sub(lwc1(pv + 0x60), lo)));
        if (1.0f < k) k = 1.0f;
        else if (k < 0.0f) k = 0.0f;
        swc1(m + 0x44, angle_wrap_mod(fpu_mul(k, u2f(0x40490fdbu))));
    }
    // (the table L17:0x1e6ea0)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002ee3b4): set up: its path of 11 points, else it goes
        swc1(pv + 0x78, rand_angle());
        const int32_t path = lw(pv + 0x80);
        sw(pv + 0x6c, 0xffffffffu);
        if (path != -1 && lw(lvd::path(path)) == 0xb) {
            sb(m + 0x20, 6);
            break;
        }
        moby_retire(m);
        return;
    }
    case 1: {  // (0x002ee404): the camera eased from where it is to beside the lift, then the state kept in +0xbc
        const float was = lwc1(pv + 0x70);
        const float t = fpu_mul(dt2(), 4.0f);
        seek_1d(pv + 0x70, pv + 0x74, 1.0f, t, t, fpu_mul(dt(), 4.0f));
        const float now = lwc1(pv + 0x70);
        if (now < 1.0f) {
            const uint32_t pos = sp + 0x30, ang = sp + 0x40, d = sp + 0x20;
            const float k = fpu_div(fpu_sub(1.0f, now), fpu_sub(1.0f, was));
            vo_2ee9c0_l17(m, pos, ang);
            vec3_sub(V(d), V(L17_ADDR(0x001676c0)), V(pos));
            vec3_scale(V(d), V(d), k);
            vec3_add(V(pos), V(pos), V(d));
            swc1(ang + 4, angle_add(fpu_mul(angle_sub(lwc1(L17_ADDR(0x001676d4)), lwc1(ang + 4)), k), lwc1(ang + 4)));
            swc1(ang + 8, angle_add(fpu_mul(angle_sub(lwc1(L17_ADDR(0x001676d8)), lwc1(ang + 8)), k), lwc1(ang + 8)));
            uid5_set_pvar_80(pos);
            uid5_set_pvar_90(ang);
        } else {
            const uint32_t next = lbu(m + 0xbc);
            sb(m + 0xbc, 1);
            sb(m + 0x20, next);
        }
        break;
    }
    case 2: case 3:  // (0x002ee51c): with the hero: his path on it
        vo_2eea70_l17(m);
        [[fallthrough]];
    case 4: case 5: {  // (0x002ee524): moving up (2, 4) or down (3, 5); the hero off it: on without him
        sw(m + 0x90, rgba_lerp(lwu(m + 0x90), 0x80808040u, u2f(0x3dcccccdu)));
        if (lbu(m + 0x20) == 2 && lwu(P + 0x2fc) != m) sb(m + 0x20, 4);
        if (lbu(m + 0x20) == 3 && lwu(P + 0x2fc) != m) sb(m + 0x20, 5);
        if (sub_0028d8c0(m, lw(pv + 0x6c)) == 0) sw(pv + 0x6c, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        const uint32_t st = lbu(m + 0x20);
        const float target = (st == 2 || st == 4) ? lwc1(pv + 0x60) : lwc1(pv + 0x64);
        const float t = fpu_mul(dt2(), u2f(0x3e99999au));
        seek_1d(m + 0x18, pv + 0x68, target, t, t, fpu_mul(dt(), 3.0f));
        if (abs_f(fpu_sub(lwc1(m + 0x18), target)) != 0.0f) break;
        // arrived: 4 -> 6, 5 -> 7, 2 -> 8, 3 -> 9
        const uint32_t s = lbu(m + 0x20);
        sb(m + 0x20, s == 4 ? 6 : (s == 5 ? 7 : (s == 2 ? 8 : 9)));
        if (lbu(m + 0xbc) != 0) hero_set_state(0, 0);
        sound_stop(m, pv);
        break;
    }
    case 6: case 7: {  // (0x002ee6f4): waiting below (6) / above (7): the lights pulse
        const float phase = angle_add(lwc1(pv + 0x78), fpu_mul(dt(), u2f(0x40c90fdbu)));
        swc1(pv + 0x78, phase);
        const float s = sin_vu(phase);
        const int32_t v = float_to_int(fpu_mul(fpu_sub(fpu_mul(s, 4.0f), 3.0f), 128.0f));
        const uint32_t c = v >= 129 ? 0x80u : (0x1f < v ? static_cast<uint32_t>(v) : 0x20u);
        sw(m + 0x90, (c << 16) | ((c << 8) | 0x80000000u) | c);
        if (lh(P + 0x308) == 0 && lw(pv + 0x7c) != 0) break;
        if (lwu(P + 0x2fc) == m && lh(P + 0x30e) == 0 && vec2_distance(V(P + 0x80), V(m + 0x10)) < 0.5f) {
            moby_sound_start(1, 0, m);  // the hero at its centre: it starts
            sb(m + 0x20, lbu(m + 0x20) == 7 ? 2 : 3);
            break;
        }
        // the hero near (2 .. 32), on the ground at its other end (within half its travel): it comes
        const uint32_t st = lbu(m + 0x20);
        const float d = vec2_distance(V(m + 0x10), V(P + 0x80));
        const float other = st == 6 ? lwc1(pv + 0x64) : lwc1(pv + 0x60);
        const float dz = abs_f(fpu_sub(lwc1(P + 0x2d8), other));
        const float half = fpu_mul(abs_f(fpu_sub(lwc1(pv + 0x60), lwc1(pv + 0x64))), 0.5f);
        if (dz < half && d < 32.0f && 2.0f < d) {
            sb(m + 0xbc, 0);
            sb(m + 0x20, st == 6 ? 3 : 2);
            sw(pv + 0x68, 0);
        }
        break;
    }
    case 8: case 9: {  // (0x002ee6a0): stopped, until the hero leaves it (8 -> 6, 9 -> 7)
        sound_stop(m, pv);
        if (lwu(P + 0x2fc) == m) break;
        sb(m + 0x20, lbu(m + 0x20) == 9 ? 7 : 6);
        break;
    }
    default:  // (10 and beyond)
        break;
    }
    // what it carries moved with it: by its move and its turn
    vec3_add(V(moved), V(moved), V(m + 0x10));
    attach_frame_set(pv + 0x20, moved, ang0, m + 0x40);
}

}  // namespace rac1
