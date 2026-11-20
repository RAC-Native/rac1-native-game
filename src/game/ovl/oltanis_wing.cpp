// Oltanis (level 14), class 81: a bomber's wing, shed when the bomber is shot down (class31_spawn_debris in
// oltanis_bomber.cpp spawns it). It falls with its velocity (pvar +0x00, damped and pulled down each frame), spins (+0x14
// .. +0x1c), trails sparks and a smoke spray from its first joint and carries a light; it explodes when it meets the world
// or comes within 10 of the hero across the ground, and is retired when its timer (+0x10) runs out or it leaves the level's
// cube (2 .. 1021). Pvars: +0x00 velocity, +0x10 the life timer, +0x12 its light (-1: none yet), +0x20 the bomber.
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

#include <cstdint>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350u;

// orig: L14:0x002bb4d8 -- the wing's end: its light freed, the moby retired
void wing_retire(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int16_t light = lh(pv + 0x12);
    if (light != -1) {
        region_clear(static_cast<uint32_t>(static_cast<int32_t>(light)));
        sh(pv + 0x12, 0xffffu);
    }
    moby_retire(m);
}

// a velocity lane damped by the level's drag (0x1615d4) once its speed is past 0x1615d8 (both per frame at 60 Hz)
float wing_damped(float v, float fscale) {
    const float drag = lwc1(L14_ADDR(0x001615d4));
    return fpu_mul(v, fpu_add(fpu_mul(fpu_sub(drag, 1.0f), fscale), 1.0f));
}

bool wing_outside_level(const Vec4& p) {
    return p.x < 2.0f || 1021.0f < p.x || p.y < 2.0f || 1021.0f < p.y || p.z < 2.0f || 1021.0f < p.z;
}

}  // namespace

// orig: L14:0x002bac78
void olt_class81_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x10))) != 0) {
        wing_retire(m);
        return;
    }
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));

    // the fall: x / y damped past the speed limit, z damped while it rises, then gravity
    {
        const float fs = lwc1(0x0015ed60u);
        if (fpu_mul(lwc1(L14_ADDR(0x001615d8)), fs) < abs_f(lwc1(pv + 0x0))) swc1(pv + 0x0, wing_damped(lwc1(pv + 0x0), fs));
    }
    {
        const float fs = lwc1(0x0015ed60u);
        if (fpu_mul(lwc1(L14_ADDR(0x001615d8)), fs) < abs_f(lwc1(pv + 0x4))) swc1(pv + 0x4, wing_damped(lwc1(pv + 0x4), fs));
    }
    {
        const float vz = lwc1(pv + 0x8);
        if (0.0f < vz) swc1(pv + 0x8, wing_damped(vz, lwc1(0x0015ed60u)));
    }
    swc1(pv + 0x8, fpu_sub(lwc1(pv + 0x8), fpu_mul(lwc1(L14_ADDR(0x001615d0)), lwc1(0x0015ed64u))));

    if (wing_outside_level(V(m + 0x10))) {
        wing_retire(m);
        return;
    }

    bool explode = sphere_collide(m + 0x10, 0.75f, 0, 0) != 0;
    if (!explode) {
        explode = abs_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80))) < 10.0f &&
                  abs_f(fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84))) < 10.0f;
    }
    if (explode) {
        const uint32_t zero = ee_stack_alloc(0x10);
        vec_clear(V(zero));
        moby_sound_play(1, 0, m, static_cast<uint32_t>(static_cast<int32_t>(lh(lwu(pv + 0x20) + 0xa6))));
        explosion_effect(m, zero, m + 0x10, 5, 2, 4, -1, 0, 2, -1, 0, 2.0f, 4.0f, 1.0f, 0.5f, 9.0f, 1.0f, 5.0f);
        ee_stack_free(0x10);
        wing_retire(m);
        return;
    }

    // the spin
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x14)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x18)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x1c)));

    const uint32_t at = ee_stack_alloc(0x10);  // (the sparks' place: spawn_particle_17 and light_add take it by address)
    Vec4 joint{};
    moby_chain_point_v(m, 0, joint);
    Vec4 off{};
    vec3_scale(off, V(m + 0xc0), rand_range_signed_f(0.01f, 0.5f));
    vec3_add(V(at), joint, off);
    // the smoke: from the joint, backward from the wing's centre
    Vec4 back{};
    vec3_sub(back, joint, V(m + 0x10));
    vec3_set_length(back, back, 0.05f);
    {
        const int32_t lo = frame_scale_i(10);
        const int32_t hi = frame_scale_i(20);
        const int32_t life = rand_range(lo, hi);
        spawn_particle_15_v(V(at), back, 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(life), 1, 40000.0f);
    }
    // three sparks: the first two about the wing's centre (along its second row; the first row's offset is drawn too and
    // overwritten, as the original does), the third at the second's place
    for (int k = 0; k < 3; ++k) {
        if (k != 2) {
            vec3_scale(off, V(m + 0xc0), rand_range_signed_f(0.01f, 0.5f));
            vec3_add(V(at), V(m + 0x10), off);
            vec3_scale(off, V(m + 0xd0), rand_range_signed_f(0.01f, 0.5f));
            vec3_add(V(at), V(m + 0x10), off);
        }
        int32_t spin = rand_mod(6);
        if (rand_mod(2) != 0) spin = -spin;
        const float size = rand_range_f(20000.0f, 100000.0f);
        const uint32_t c = static_cast<uint32_t>(rand_range(0x10, 0xff));
        const uint32_t p = spawn_particle_17(at, spin, L14_ADDR(0x0015f580), 0x60000000u | (c << 16) | (c << 8) | c, 0.025f,
                                             1.0f, 1.0f, size);
        if (p != 0) {
            sb(p + 0x3, 0x44);
            sh(p + 0xa, static_cast<uint16_t>(frame_scale_i(60)));
            sb(p + 0x2a, 0x60);
            sw(p + 0x24, 2);
            sb(p + 0x2b, lbu(p + 0xa));
        }
    }
    // the light: 2.5 times the joint's offset out from the centre, one unit along the third row, either side
    Vec4 side{};
    vec3_sub(side, joint, V(m + 0x10));
    vec3_scale(side, side, 2.5f);
    vec3_add(V(at), V(m + 0x10), side);
    if (rand_mod(2) != 0) vec3_scale(side, V(m + 0xe0), -1.0f);
    else side = V(m + 0xe0);
    vec3_add(V(at), V(at), side);
    const int16_t light = lh(pv + 0x12);
    if (light == -1) {
        sh(pv + 0x12, static_cast<uint16_t>(light_add(at, 3.0f, 0.0f, 1.0f, u2f(0x3f4ccccdu), u2f(0x3e4ccccdu))));
    } else {
        const uint32_t rec = L14_ADDR(0x001809c0) + (static_cast<uint32_t>(static_cast<int32_t>(light)) << 5);
        V(rec + 0x10) = V(at);
        swc1(rec + 0x8, u2f(0x3e4ccccdu));
        swc1(rec + 0x1c, 3.0f);
        swc1(rec + 0x0, 1.0f);
        swc1(rec + 0x4, u2f(0x3f4ccccdu));
    }
    ee_stack_free(0x10);
}

}  // namespace rac1
