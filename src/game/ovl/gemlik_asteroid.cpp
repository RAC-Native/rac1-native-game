// Gemlik Base's (level 13) class 212: an asteroid of the space fight. It waits at its home point until Ratchet flies
// his ship, then drifts and spins; a hit or a crash breaks it (a blast and one or two smaller pieces, class 1412, 0x584,
// which break in turn for a few generations); once out of sight it comes back to its home point. Level 13 only: the
// addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in
// gemlik_mobys.cpp.
//
// Its pvars: +0x00 its velocity, +0x10 its home point, +0x1c 20, +0x20 its drift's direction (its matrix's first row),
// +0x2c its scale at the start, +0x30 / +0x34 / +0x38 its spin, +0x40 a piece (1: it goes when it is out of sight), +0x44
// the frames it lasts out of sight, +0x48 the asteroid a piece broke from, +0x4c the frames before it can break again
// (16 bits), +0x4e the generations it can still break into (16 bits). States (the table L13:0x1f4ca0): 0 set up, 1
// waiting, 2 drifting, 3 back home (spins anew), 4 hidden until its home point is off screen. The space's bounds
// (20 .. 1003) and the spin are kept every frame.
#include <game/collision/collision.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
// A spin about one axis chosen at random (+0x30 / +0x34 / +0x38 of `pv`): `deg` degrees a second either way.
void spin_axis(uint32_t pv, float deg) {
    const float r = fpu_mul(fpu_mul(rand_range_f(-deg, deg), u2f(0x3c8efa35u)), dt());
    switch (ee_rem(libc_rand(), 3)) {
    case 0: swc1(pv + 0x30, r); break;
    case 1: swc1(pv + 0x34, r); break;
    case 2: swc1(pv + 0x38, r); break;
    default: break;
    }
}
}  // namespace

// A piece (class 1412) broken from `parent` at `pos` with the velocity `vel` (EE vectors), `gen` generations left,
// scale `scale`: drifting (state 2), spinning at random. The piece, or 0.
// orig: L13:0x002e1140
uint32_t gml_2e1140_l13(uint32_t parent, uint32_t pos, uint32_t vel, int32_t gen, float scale) {
    const uint32_t s = moby_spawn(0x584);
    if (s == 0) return 0;
    const uint32_t pv = lwu(s + 0x78);
    sb(s + 0x30, 0xff);
    sb(s + 0x31, 1);
    sh(s + 0x32, lhu(parent + 0x32));
    sb(s + 0x20, 2);
    sb(s + 0xbc, 0);
    sb(s + 0x72, 0);
    lq_sq(s + 0x10, pos);
    lq_sq(pv + 0x00, vel);
    swc1(s + 0x2c, scale);
    sw(pv + 0x48, parent);
    sw(pv + 0x40, 1);
    sh(pv + 0x4c, static_cast<uint32_t>(frame_scale_i(0x1e)));
    sh(pv + 0x4e, static_cast<uint32_t>(gen));
    sw(s + 0x94, 0);
    spin_axis(pv, 30.0f);
    spin_axis(pv, 180.0f);
    swc1(s + 0x40, rand_angle());
    swc1(s + 0x44, rand_angle());
    swc1(s + 0x48, rand_angle());
    moby_update_transform(s);
    moby_copy_hero_color(s);
    return s;
}

// The asteroid `m` breaks (`k` its scale relative to its class's): a blast (its size by `k`; none of its push and
// shake beyond 20 of the hero), and while generations are left one or two pieces flung out (each taking its share of
// the scale, the last while the asteroid is still more than a fifth of its class's scale); else it hides (a piece goes).
// orig: L13:0x002e1348
void gml_2e1348_l13(uint32_t m, uint32_t pv, float k) {
    const Frame f(176);
    const uint32_t sp = f.sp;
    float push = fpu_mul(k, 7.0f);
    float shake = fpu_mul(k, 10.0f);
    if (20.0f < vec3_distance(V(m + 0x10), V(0x0013f3d0))) {
        shake = 0.0f;
        push = 0.0f;
    }
    moby_sound_start(0, 0, m);
    if (lh(pv + 0x4c) == 0) {
        const int32_t n1 = float_to_int(fpu_mul(k, 10.0f));
        const int32_t a = float_to_int(fpu_mul(k, 3.0f));
        const int32_t b = float_to_int(fpu_mul(k, 16.0f));
        explosion_effect(m, L13_ADDR(0x00160680), 0, n1, a, b, -1, 0, 1, -1, 0, push, shake, fpu_mul(k, 4.0f), fpu_add(k, k), 9.0f, 1.0f, fpu_mul(k, 15.0f));
    }
    sb(m + 0x72, 0);
    if (lh(pv + 0x4e) <= 0) {  // no generation left
        if (lw(pv + 0x40) != 0) {
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 4);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        sw(m + 0x94, 0);
        sb(m + 0x31, 0);
        swc1(m + 0x2c, lwc1(pv + 0x2c));
        return;
    }
    const uint32_t vel = sp + 0x20, at = sp + 0x30;
    for (int32_t n = 0; n < rand_range(1, 2);) {
        const float share = rand_range_f(0.25f, 0.75f);
        const float lo = fpu_mul(dt(), 5.0f), hi = fpu_mul(dt(), 10.0f);
        const float scale = fpu_mul(lwc1(m + 0x2c), share);
        rand_sphere_point(vel, lo, hi);
        vec3_scale(V(pv), V(pv), 0.75f);
        vec3_add(V(vel), V(vel), V(pv));
        vec3_set_length(V(at), V(vel), 0.5f);
        vec3_add(V(at), V(at), V(m + 0x10));
        vec3_add(V(at), V(at), V(vel));
        gml_2e1140_l13(m, at, vel, static_cast<int32_t>(lh(pv + 0x4e)) - 1, scale);
        swc1(m + 0x2c, fpu_sub(lwc1(m + 0x2c), scale));
        const uint32_t left = (lhu(pv + 0x4e) - 1u) & 0xffffu;
        sh(pv + 0x4e, left);
        if (left == 0) break;
        ++n;
        if (fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24)) <= 0.2f) break;
    }
    sw(m + 0x94, 0);
    sh(pv + 0x4c, static_cast<uint32_t>(frame_scale_i(0x1e)));
}

// orig: L13:0x002e1638
void gml_class212_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float d = vec3_distance(V(m + 0x10), V(0x0013f3d0));
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4ca0)
    case 0:  // (0x002e1698)
        lq_sq(pv + 0x10, m + 0x10);
        swc1(pv + 0x1c, 20.0f);
        lq_sq(pv + 0x20, m + 0xc0);
        swc1(pv + 0x2c, lwc1(m + 0x2c));
        sh(pv + 0x4e, static_cast<uint32_t>(rand_range(2, 3)));
        if (lwc1(m + 0x2c) < lwc1(lwu(m + 0x24) + 0x24)) sb(m + 0x72, 0);
        sb(m + 0x20, 3);
        break;
    case 1: {  // (0x002e1704): waits for the hero to fly his ship (near it, or while it is drawn)
        const uint32_t ship = lwu(HERO + 0x15f0);
        if (lw(0x001413d4) != 0x32 || ship == 0 || lh(ship + 0xa6) != 0x45) break;
        if (lbu(m + 0x31) == 0 && !(d < 40.0f)) break;
        vec3_set_length(V(pv), V(pv + 0x20), fpu_mul(rand_range_f(1.0f, 10.0f), dt()));
        sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (0x002e179c): drifts
        tick16(pv + 0x4c);
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        if (lbu(m + 0x31) == 0 && !(d < 40.0f)) {  // out of sight: it lasts +0x44 frames
            if (tick32(pv + 0x44) == 0) break;
            if (lw(pv + 0x40) == 1) {
                moby_retire(m);
                return;
            }
            sb(m + 0x20, 4);
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
            sb(m + 0x31, 0);
            swc1(m + 0x2c, lwc1(pv + 0x2c));
            break;
        }
        sw(pv + 0x44, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x40) != 0 ? 0x78 : 0x168)));
        if (lh(pv + 0x4c) != 0) break;
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        const float k = fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24));
        // a crash, tested on every other frame (by the moby's slot), bounces it off and breaks it (not against the
        // asteroid it broke from; another asteroid only bounces it)
        const uint32_t slot = static_cast<uint32_t>(static_cast<int32_t>(m - lwu(L13_ADDR(0x0015ffd8))) >> 8) & 1u;
        if ((lw(L13_ADDR(0x0015f5cc)) & 1) == static_cast<int32_t>(slot) && sphere_collide(m + 0x10, fpu_mul(k, u2f(0x40c66666u)), 0, m) != 0) {
            if (coll_hit_moby() == lwu(pv + 0x48)) break;
            const Vec4 n = coll_hit_normal();
            vec3_reflect(V(pv), V(pv), n);
            V(m + 0x10) = coll_hit_pushed();
            const uint32_t hm = coll_hit_moby();
            if (hm != 0 && lh(hm + 0xa6) == 0xd4) break;
            gml_2e1348_l13(m, pv, k);
            break;
        }
        // a hit (not by another asteroid) pushes it and breaks it
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        if (hit != 0 && lh(lwu(hit + 0x20) + 0xa6) != 0xd4) {
            const float k2 = fpu_div(lwc1(m + 0x2c), lwc1(lwu(m + 0x24) + 0x24));
            vec3_clamp_length_vu(V(hit + 0x10), V(hit + 0x10), fpu_mul(dt(), 20.0f));
            vec3_add(V(pv), V(pv), V(hit + 0x10));
            gml_2e1348_l13(m, pv, k2);
        }
        sb(m + 0xa4, 0xff);
        break;
    }
    case 3:  // (0x002e19a4): back home, spinning anew
        lq_sq(m + 0x10, pv + 0x10);
        sq_zero(pv + 0x30);
        spin_axis(pv, 30.0f);
        spin_axis(pv, 180.0f);
        swc1(m + 0x40, rand_angle());
        swc1(m + 0x44, rand_angle());
        swc1(m + 0x48, rand_angle());
        sq_zero(pv + 0x00);
        swc1(m + 0x2c, lwc1(pv + 0x2c));
        if (lwc1(lwu(m + 0x24) + 0x24) < lwc1(pv + 0x2c)) sb(m + 0x72, 0x64);
        sb(m + 0x20, 1);
        break;
    case 4:  // (0x002e1b14): hidden until its home point is off screen
        if (point_on_screen(pv + 0x10, int_to_float(static_cast<int32_t>(lh(m + 0x32)))) >= 0) break;
        lq_sq(m + 0x10, pv + 0x10);
        sb(m + 0x31, 1);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        sb(m + 0x20, 3);
        break;
    default:  // (beyond 4: the bounds and the spin only)
        break;
    }
    // inside the space; the spin
    for (uint32_t a = 0; a < 0xc; a += 4) {
        if (lwc1(m + 0x10 + a) < 20.0f) swc1(m + 0x10 + a, 20.0f);
        if (1003.0f < lwc1(m + 0x10 + a)) swc1(m + 0x10 + a, 1003.0f);
    }
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x30)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x34)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x38)));
}

}  // namespace rac1
