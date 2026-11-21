// Class 679, a current along the level's paths that carries the hero: one port of the copies four levels carry (Novalis,
// Rilgar, Batalia, Quartu). The copies are the same code relinked (compared with the relocated fields masked); what they
// name is the engine's (OV: the level's path table 0x1b04b0) or the base ELF's (bare: the hero 0x13f350, the frame words
// 0x15ed60 .. 0x15ed70, the level's number 0x15ed84). Header shared_class679.h.
//
// The current's pvars: its paths' indices +0 (the first -1: none), the speed it gives +0x60, its reach +0x64, its set-up
// flag +0x68, its path count +0x70. A path: its point count +0, its points from +0x10 (16 bytes; their w the length of
// the leg to the next one, the last's back to the first, which the current writes once).
#include <game/ovl/shared_class679.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position, +0xf0 its velocity)
}  // namespace

// Set up once: each path's leg lengths, the moby's draw distance 0xff. Then, with the hero in state 0x10: the nearest of
// its paths' closest points to him; within the reach +0x64 he is carried: on each frame the speed is set by how much of
// the path is left ahead (under 8: 3, 15: 4, 30: 5 units a second, else 9; only on Novalis, level 1), his
// speed +0x9dc eases toward it, his velocity goes along the path at that speed (his heading +0x9d0 / pitch +0x9d4 along
// it) plus a pull toward the path that eases in (+0x9e0, sprung, never longer than his distance to it); at the path's
// end his kept velocity +0x150 slows and is his velocity.
// orig: L1:0x002f6328 L5:0x003067b8 L8:0x002f74c0 L15:0x002d95e8
void class679_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0 || lw(pv) == -1 || lw(pv + 0x70) == 0) return;
    auto path_of = [pv](int32_t i) { return lwu(OV(0x001b04b0) + (lwu(pv + static_cast<uint32_t>(i) * 4) << 2)); };
    if (lw(pv + 0x68) == 0) {
        sw(pv + 0x68, 1);
        for (int32_t i = 0; i < lw(pv + 0x70); ++i) {
            const uint32_t path = path_of(i);
            uint32_t pt = path + 0x10;
            for (int32_t k = 0; k < lw(path) - 1; ++k, pt += 0x10) swc1(pt + 0xc, vec3_distance(V(pt), V(pt + 0x10)));
            swc1(pt + 0xc, vec3_distance(V(pt), V(path + 0x10)));
        }
        sb(m + 0x30, 0xff);
    }
    if (lw(HERO + 0x208c) != 0x10) return;

    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t nearest = sp, cand = sp + 0x10, along = sp + 0x20, pull = sp + 0x30;
    const uint32_t cand_leg = sp + 0x40, cand_t = sp + 0x44, leg = sp + 0x48, leg_t = sp + 0x4c, spring = sp + 0x50;
    float best = 9999.0f;
    uint32_t path = 0;
    for (int32_t i = 0; i < lw(pv + 0x70); ++i) {
        const uint32_t p = path_of(i);
        int32_t k = lw(cand_leg);
        float t = lwc1(cand_t);
        path_closest_point_v(p, V(HERO + 0x80), V(cand), &k, &t, 0, 999.0f, 5.0f, 0.0f);
        sw(cand_leg, static_cast<uint32_t>(k));
        swc1(cand_t, t);
        const float d = vec2_distance(V(HERO + 0x80), V(cand));
        if (!(d < best)) continue;
        path = p;
        lq_sq(nearest, cand);
        best = d;
        sw(leg, lwu(cand_leg));
        swc1(leg_t, lwc1(cand_t));
    }
    if (path == 0) {
        ee_stack_free(0x110);
        return;
    }
    int32_t at_end;
    {
        int32_t k = lw(leg);
        float t = lwc1(leg_t);
        at_end = path_advance_v(path, V(cand), &k, &t, 0, 0.3f);
        sw(leg, static_cast<uint32_t>(k));
        swc1(leg_t, t);
    }
    if (lwc1(pv + 0x64) < best) {
        ee_stack_free(0x110);
        return;
    }
    if (lw(0x0015ed84) == 1) {
        const int32_t last = lw(path) - 1;
        float left = 0.0f;
        for (int32_t k = lw(leg); k < last; ++k) left = fpu_add(left, lwc1(path + 0x1c + (static_cast<uint32_t>(k) << 4)));
        const float dt = lwc1(0x0015ed6c);
        float speed;
        if (left < 8.0f) speed = fpu_mul(dt, 3.0f);
        else if (left < 15.0f) speed = fpu_mul(dt, 4.0f);
        else if (!(left < 30.0f)) speed = fpu_mul(dt, 9.0f);
        else speed = fpu_mul(dt, 5.0f);
        swc1(pv + 0x60, speed);
    }
    if (at_end != 0) {
        const float slow = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbc23d700u)), 1.0f);
        vec3_scale(V(HERO + 0x150), V(HERO + 0x150), slow);
        lq_sq(HERO + 0xf0, HERO + 0x150);
        sw(HERO + 0xfc, 0);
        ee_stack_free(0x110);
        return;
    }
    vec3_sub(V(along), V(cand), V(nearest));
    swc1(HERO + 0x9d4, atan2_f(vec2_length(V(along)), lwc1(along + 8)));
    swc1(HERO + 0x9d0, atan2_f(lwc1(along), lwc1(along + 4)));
    sw(along + 8, 0);
    approach_f(HERO + 0x9dc, lwc1(pv + 0x60), fpu_mul(lwc1(0x0015ed70), 6.0f));
    if (u2f(0x3a83126fu) < vec3_length(V(along))) vec3_set_length(V(HERO + 0xf0), V(along), lwc1(HERO + 0x9dc));
    vec3_sub(V(pull), V(nearest), V(HERO + 0x80));
    swc1(pull + 8, 0.0f);
    const float rate = fpu_mul(lwc1(0x0015ed6c), 5.0f);
    const float g = lwc1(0x0015ed64);
    swc1(spring, -vec2_length(V(pull)));
    spring_to(spring, HERO + 0x9e0, 0.0f, fpu_mul(g, u2f(0x3c03126fu)), fpu_mul(g, u2f(0x3e99999au)), rate);
    const float dist = vec3_length(V(pull));
    if (dist < lwc1(HERO + 0x9e0)) swc1(HERO + 0x9e0, dist);
    if (lwc1(HERO + 0x9e0) < dist) vec3_set_length(V(pull), V(pull), lwc1(HERO + 0x9e0));
    vec3_add(V(HERO + 0xf0), V(HERO + 0xf0), V(pull));
    swc1(HERO + 0xfc, 0.0f);
    lq_sq(HERO + 0x150, HERO + 0xf0);
    if (lh(HERO + 0x1e0) < frame_scale_i(0x1e)) sh(HERO + 0x1e0, static_cast<uint32_t>(frame_scale_i(0x1e)));
    ee_stack_free(0x110);
}

}  // namespace rac1
