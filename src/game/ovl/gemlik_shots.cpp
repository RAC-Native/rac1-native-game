// Gemlik Base's (level 13) classes 1233 and 1238: the gun drone's shot and the wall turret's shot. Level 13 only: the
// addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in
// gemlik_mobys.cpp.
//
// Class 1233, the drone's shot (class 101 holds two, gml_3058a8_l13 spawns them, gml_305988_l13 fires one). pvars +0
// its velocity (a frame's move), +0x10 where it was fired from, +0x20 its drone, +0x24 1 (a halfword: 0 makes the
// world test take flag 1), +0x26 the drone's chain point it is held at, +0x28 its range, +0x2c its target. State 1:
// held (it follows the chain point, the drone's hidden flag; retired when the drone is gone); any other state: it
// flies, a trail of four glows behind it; a hit by a class 0x1df moby (or meeting one) ends it in a small burst
// (0x306148), the end of its range or the target's distance from its start in a burst of sparks and an explosion
// (0x3060b8), what it meets on its step (the world, a moby but its drone) in sparks thrown off the surface and the
// explosion.
// Class 1238, the wall turret's shot (gml_3061c8_l13 spawns it). pvars +0 its velocity (+0xc 31337.6 once turned back),
// +0x10 its start, +0x20 the turret, +0x24 1 (0: the world test's flag 1), +0x28 its range, +0x2c its frames left.
// While the hero's byte 0x141346 is set and the shot comes within 2 of him, it is stopped half a unit back, given 5
// frames and marked, and the first of the hero's six mobys 0x141370 .. is hit (one more damage on a hit of this frame).
// At its range's end or its frames' end: sparks (when visible) and retired; what it meets on its step: sparks off the
// surface, a sound when that is the hero's moby (class 0), retired. Both are retired outside the space's cube (2 ..
// 1021).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/devastator.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {

// the shot `m` outside the space's cube (2 .. 1021 on each axis)
bool out_of_space(uint32_t m) {
    const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
    return x < 2.0f || 1021.0f < x || y < 2.0f || 1021.0f < y || z < 2.0f || 1021.0f < z;
}

// three sparks (particle 0x33) thrown at random from `pos`, slow; `rnd` and `vec` the original's two stack vectors
void end_sparks(uint32_t pos, uint32_t vec, uint32_t rnd) {
    for (int k = 0; k < 3; ++k) {
        sq_zero(rnd);
        swc1(rnd + 0, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 4, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 8, rand_range_f(-1.0f, 1.0f));
        lq_sq(vec, rnd);
        vec3_set_length(V(vec), V(vec), fpu_mul(vec3_length(V(rnd)), 0.1f));
        vec3_add(V(vec), V(rnd), V(vec));
        const float fs = dt();
        vec3_set_length(V(vec), V(vec), rand_range_f(fpu_add(fs, fs), fpu_mul(fs, 4.0f)));
        particle_33_spawn(pos, vec);
    }
}

// five sparks (particle 0x1b) thrown off the surface the collision met, about the reflection of `vel`; `rnd` and `vec`
// the original's two stack vectors
void hit_sparks(uint32_t pos, uint32_t vel, uint32_t rnd, uint32_t vec) {
    for (int k = 0; k < 5; ++k) {
        sq_zero(rnd);
        swc1(rnd + 0, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 4, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 8, rand_range_f(-1.0f, 1.0f));
        lq_sq(vec, rnd);
        vec3_reflect(V(rnd), V(vel), coll_hit_normal());
        vec3_set_length(V(vec), V(vec), fpu_mul(vec3_length(V(rnd)), 0.5f));
        vec3_add(V(vec), V(rnd), V(vec));
        const float fs = dt();
        vec3_set_length(V(vec), V(vec), rand_range_f(fpu_mul(fs, 3.0f), fpu_mul(fs, 6.0f)));
        const int32_t lo = frame_scale_i(10);
        const int32_t hi = frame_scale_i(15);
        spawn_particle_1b(pos, vec, 0x7f2f4f6fu, static_cast<uint32_t>(rand_range(lo, hi)), 30000.0f);
    }
}

}  // namespace

// The drone's shot `m` explodes (12 pieces, a flash).
// orig: L13:0x003060b8
void gml_3060b8_l13(uint32_t m) {
    explosion_effect(m, 0, m + 0x10, 0xc, 4, 0x12, 0, 1, 1, -1, 0, 2.0f, 1.0f, 4.0f, 3.0f, 1.0f, 2.0f, 20.0f);
}

// The drone's shot `m` ends in a small burst (on a class 0x1df moby).
// orig: L13:0x00306148
void gml_306148_l13(uint32_t m) {
    explosion_effect(m, 0, m + 0x10, 5, 1, 5, 0, 0, 1, -1, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
}

// orig: L13:0x00305a58
void gml_class1233_update_l13(uint32_t m) {
    const Frame f(320);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, from = sp, heading = sp + 0x10, rec = sp + 0x30;
    lq_sq(from, pos);
    if (lbu(m + 0x20) == 1) {  // held by its drone
        const uint32_t owner = lwu(pv + 0x20);
        if (owner == 0 || lh(owner + 0xa6) != 0x65 || !alive(owner)) {
            moby_retire(m);
            return;
        }
        lq_sq(m + 0x40, owner + 0x40);
        lq_sq(m + 0xc0, owner + 0xc0);
        lq_sq(m + 0xd0, owner + 0xd0);
        lq_sq(m + 0xe0, owner + 0xe0);
        moby_chain_point_v(lwu(pv + 0x20), static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x26))), V(pos));
        lq_sq(pv + 0x10, pos);
        const uint32_t o = lwu(pv + 0x20);
        if (o != 0 && alive(o) && (lhu(o + 0x34) & 1u) != 0) sh(m + 0x34, lhu(m + 0x34) | 1u);
        else sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        if (out_of_space(m)) moby_retire(m);
        return;
    }
    // flying: a hit by a class 0x1df moby ends it
    if (const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000), 0); hit != 0) {
        const uint32_t by = lwu(hit + 0x20);
        if (by != 0 && lh(by + 0xa6) == 0x1df) {
            gml_306148_l13(m);
            moby_retire(m);
            return;
        }
    }
    vec3_add(V(pos), V(pos), V(pv));
    for (int32_t k = 0; k < 4; ++k) {  // its trail: four glows along its step
        vec3_scale(V(sp + 0x20), V(pv), fpu_mul(-int_to_float(k), 0.25f));
        vec3_add(V(sp + 0x10), V(pos), V(sp + 0x20));
        const float size = rand_range_f(25000.0f, 35000.0f);
        const uint32_t p = static_cast<uint32_t>(dev_2712b8(sp + 0x10, L13_ADDR(0x0015f580), static_cast<uint32_t>(frame_scale_i(15)),
                                                            0x60, 0x4040ff, 3, size, 500.0f, 1.0f, u2f(0xb9d1b717u), 0.0f));
        if (p != 0) sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) - 0x80));
    }
    // its hit record: along its heading (flat, unit, z 1)
    lq_sq(heading, pv);
    sw(heading + 8, 0);
    vec2_set_length(V(heading), V(heading), 1.0f);
    swc1(heading + 8, 1.0f);
    sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, heading);
    const uint32_t flags = lhu(pv + 0x24) == 0 ? 1u : 0u;
    const float flown = vec2_distance(V(pos), V(pv + 0x10));
    const float target = vec2_distance(V(lwu(pv + 0x2c) + 0x10), V(pv + 0x10));
    if (lwc1(pv + 0x28) < flown || target < flown) {  // its range's end, or past its target
        if (lbu(m + 0x31) != 0) end_sparks(pos, sp + 0x20, sp + 0x60);
        gml_3060b8_l13(m);
        moby_retire(m);
        return;
    }
    if (segment_collide_v(V(from), V(pos), flags, m, static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0) {
        const uint32_t met = coll_hit_moby();
        if (met == 0 || met != lwu(pv + 0x20)) {  // (its own drone: it flies on)
            V(pos) = coll_hit_point();
            if (lbu(m + 0x31) != 0) hit_sparks(pos, pv, sp + 0x20, sp + 0x60);
            const uint32_t at = coll_hit_moby();
            if (at != 0 && lh(at + 0xa6) == 0x1df) gml_306148_l13(m);
            else gml_3060b8_l13(m);
            moby_retire(m);
            return;
        }
    }
    if (out_of_space(m)) moby_retire(m);
}

// orig: L13:0x00306300
void gml_class1238_update_l13(uint32_t m) {
    const Frame f(304);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, from = sp, rec = sp + 0x10, heading = sp + 0x40;
    const float mark = u2f(0x46f4d334u);  // (31337.6: turned back)
    lq_sq(from, pos);
    vec3_add(V(pos), V(pos), V(pv));
    lq_sq(heading, pv);
    sw(heading + 8, 0);
    vec2_set_length(V(heading), V(heading), 1.0f);
    swc1(heading + 8, 1.0f);
    // the hero's byte 0x141346 set and the shot within 2 of him: turned back, and the first of his six mobys hit
    if (lbu(0x00141346) != 0 && lwc1(pv + 0xc) != mark && vec3_distance(V(pos), V(0x0013f3d0)) < 2.0f) {
        vec3_set_length(V(pv), V(pv), 0.5f);
        vec3_sub(V(pos), V(pos), V(pv));
        sq_zero(pv);
        sw(pv + 0x2c, 5);
        swc1(pv + 0xc, mark);
        for (uint32_t k = 0; k < 6; ++k) {
            const uint32_t o = lwu(0x00141370 + k * 4);
            if (o == 0) continue;
            sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, heading);
            sb(rec + 0x18, 1);
            sh(rec + 0x1a, lhu(m + 0xa6));
            sb(rec + 0x19, 1);
            const uint32_t idx = lbu(o + 0xa4);
            const uint32_t e = coll_contact(idx);
            if (idx != 0xff && lwu(e + 0x34) == o) {  // (a hit of this frame on it: one more damage)
                swc1(e + 0x2c, fpu_add(lwc1(e + 0x2c), 1.0f));
                sb(e + 0x28, 1);
                sb(e + 0x29, 1);
                sh(e + 0x2a, lhu(m + 0xa6));
                sw(e + 0x24, 0x10001);
            } else {
                moby_hit_record(o, rec);
            }
            break;
        }
    }
    sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, heading);
    sb(rec + 0x19, 1);
    sh(rec + 0x1a, lhu(m + 0xa6));
    sb(rec + 0x18, 1);
    const uint32_t flags = lwu(pv + 0x24) == 0 ? 1u : 0u;
    bool ends = lwc1(pv + 0x28) < vec2_distance(V(pos), V(pv + 0x10));  // its range's end
    if (!ends) ends = tick32(pv + 0x2c) != 0;                              // or its frames'
    if (ends) {
        if (lbu(m + 0x31) != 0) end_sparks(pos, sp + 0x50, sp + 0x60);
        moby_retire(m);
        return;
    }
    if (segment_collide_v(V(from), V(pos), flags, lwu(pv + 0x20), static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0) {
        V(pos) = coll_hit_point();
        if (lbu(m + 0x31) != 0) hit_sparks(pos, pv, sp + 0x60, sp + 0x50);
        const uint32_t met = coll_hit_moby();
        if (met != 0 && lh(met + 0xa6) == 0) moby_sound_start(0, 0, m);
        moby_retire(m);
        return;
    }
    if (out_of_space(m)) moby_retire(m);
}

}  // namespace rac1
