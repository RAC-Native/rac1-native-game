// Oltanis (level 14), class 386: an electric arc across three grind rails (pvars +0x1f4 / +0x1f8 / +0x1fc: rail
// indices). While the hero grinds one of the three (hero state 0xf, his rail's path +0x560) or stands within 16 of it
// across the ground, it buzzes (sound 6, set 0xd3, its channel +0x204) and shocks the hero inside its box (+0x200: a hit
// record of damage 5627.92 handed to the hero's moby); when the arc's middle (3.125 ahead) is on screen it moves its two
// strings of points (+0x00: 10 points, +0xa0: 20 points, phases +0x1e0 .. +0x1e8, spacing +0x1f0) and queues their draw
// (L14:0x2df458, still generated). Away from it the sound stops.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

constexpr uint32_t HERO = 0x0013f350u;

// orig: L14:0x002df080 -- the two strings laid out straight: 10 points +0x1f0 apart, 20 points 0.39 apart, the phases 0
void class386_strings_init(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (int32_t k = 0; k < 10; ++k) {
        const uint32_t p = pv + static_cast<uint32_t>(k) * 0x10u;
        const float x = fpu_mul(lwc1(pv + 0x1f0), static_cast<float>(k));
        swc1(p + 0x8, 0.0f);
        swc1(p + 0x4, 0.0f);
        swc1(p + 0xc, 1.0f);
        swc1(p + 0x0, x);
    }
    for (int32_t k = 0; k < 20; ++k) {
        const uint32_t p = pv + 0xa0u + static_cast<uint32_t>(k) * 0x10u;
        swc1(p + 0x8, 0.0f);
        swc1(p + 0x4, 0.0f);
        swc1(p + 0xc, 1.0f);
        swc1(p + 0x0, fpu_mul(static_cast<float>(k), u2f(0x3ec7ae14u)));
    }
    sw(pv + 0x1ec, 0);
    sw(pv + 0x1e0, 0);
    sw(pv + 0x1e4, 0);
    sw(pv + 0x1e8, 0);
}

// orig: L14:0x002df138 -- the strings' points moved: the first string a sine wave mirrored about its middle, the second
// two waves multiplied, each with a random jitter; the draw callback queued
void class386_strings_move(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    swc1(pv + 0x1e0, angle_add(lwc1(pv + 0x1e0), u2f(0x3e32b8c2u)));
    swc1(pv + 0x1e4, angle_sub(lwc1(pv + 0x1e4), u2f(0x3f4d87acu)));
    swc1(pv + 0x1e8, angle_add(lwc1(pv + 0x1e8), u2f(0x3d567770u)));
    const float step = fpu_mul(fpu_div(360.0f, fpu_mul(int_to_float(10), 0.5f)), u2f(0x3c8efa35u));
    for (int32_t k = 0; k < 5; ++k) {
        const uint32_t p = pv + static_cast<uint32_t>(k) * 0x10u;
        float along = 0.0f, z = 0.0f;
        if (k != 0) {
            along = static_cast<float>(k);
            z = fpu_mul(sin_vu(angle_wrap_mod(fpu_add(lwc1(pv + 0x1e0), fpu_mul(along, step)))), 0.1f);
            z = fpu_add(z, rand_range_signed_f(0.0f, u2f(0x3e4ccccdu)));
        }
        swc1(p + 0x8, z);
        sw(p + 0x4, 0);
        swc1(p + 0xc, 1.0f);
        swc1(p + 0x0, fpu_add(fpu_mul(lwc1(pv + 0x1f0), along), u2f(0x3fcccccdu)));
    }
    for (int32_t k = 5; k < 10; ++k) {  // (the second half mirrors the first: point k takes point 9 - k's height)
        const uint32_t p = pv + static_cast<uint32_t>(k) * 0x10u;
        const float z = lwc1(pv + static_cast<uint32_t>(9 - k) * 0x10u + 0x8u);
        sw(p + 0x4, 0);
        swc1(p + 0x8, z);
        swc1(p + 0xc, 1.0f);
        swc1(p + 0x0, fpu_add(fpu_mul(lwc1(pv + 0x1f0), static_cast<float>(k)), u2f(0x3fcccccdu)));
    }
    for (int32_t k = 0; k < 19; ++k) {  // (the 20th point keeps its place)
        const uint32_t p = pv + 0xa0u + static_cast<uint32_t>(k) * 0x10u;
        const float along = static_cast<float>(k);
        sw(p + 0x8, 0);
        const float a = angle_wrap_mod(fpu_add(lwc1(pv + 0x1e4), fpu_mul(along, u2f(0x3f7eadaeu))));
        const float b = angle_wrap_mod(fpu_add(lwc1(pv + 0x1e8), fpu_mul(along, u2f(0x3e567770u))));
        float z = fpu_mul(sin_vu(b), 0.75f);
        const float wave = sin_vu(a);
        const float jitter = rand_range_signed_f(0.0f, u2f(0x3e4ccccdu));
        z = fpu_add(fpu_mul(z, wave), jitter);
        swc1(p + 0x8, fpu_add(lwc1(p + 0x8), z));
        swc1(p + 0xc, 1.0f);
        swc1(p + 0x4, fpu_mul(cos_vu(a), 0.5f));
        swc1(p + 0x0, fpu_mul(along, u2f(0x3ec7ae14u)));
    }
    draw_callback_add(L14_ADDR(0x002df458), m);
}

// orig: L14:0x002dfc58 -- the hero inside the arc's box: shocked (a hit record on his moby: damage 5627.92, kinds 5 / 4)
void class386_shock(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (point_in_box(HERO + 0x80, lw(pv + 0x200)) == 0) return;
    const uint32_t rec = ee_stack_alloc(0x30);
    sw(rec + 0x14, NOT_ADDR(0x00200001u));
    swc1(rec + 0x1c, 100.0f);
    sw(rec + 0x20, 1);
    sw(rec + 0x10, m);
    vec_clear(V(rec));
    swc1(rec + 0x8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));
    sb(rec + 0x18, 5);
    sb(rec + 0x19, 4);
    sh(rec + 0x1a, lhu(m + 0xa6));
    sw(HERO + 0x22a8, 0);
    moby_hit_record(lwu(0x001413d0u), rec);
    ee_stack_free(0x30);
}

// the buzz stopped (the channel +0x204, if it is still the arc's and playing)
void class386_sound_stop(uint32_t m, uint32_t pv) {
    const int32_t ch = lw(pv + 0x204);
    if (ch < 0) return;
    const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
    if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) sub_0028d918(ch);
    sw(pv + 0x204, 0xffffffffu);
}

// the hero on one of the arc's rails: grinding (state 0xf) with his rail's path the rail's
bool hero_grinds_rail_of(uint32_t pv) {
    if (lw(HERO + 0x208c) != 0xf) return false;
    const uint32_t on = lwu(HERO + 0x560);
    return on == lwu(lvd::rail(lw(pv + 0x1f4)) + 0x10) || on == lwu(lvd::rail(lw(pv + 0x1f8)) + 0x10) ||
           on == lwu(lvd::rail(lw(pv + 0x1fc)) + 0x10);
}

}  // namespace

// orig: L14:0x002dee28
void olt_class386_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint8_t st = lbu(m + 0x20);
    if (st == 0) {
        if (lw(pv + 0x1f4) == -1 || lw(pv + 0x1f8) == -1 || lw(pv + 0x1fc) == -1 || lw(pv + 0x200) < 0) {
            moby_retire(m);
            return;
        }
        class386_strings_init(m);
        sb(m + 0x20, 1);
        sw(pv + 0x204, 0xffffffffu);
        class386_sound_stop(m, pv);
        return;
    }
    if (st != 1) {
        class386_sound_stop(m, pv);
        return;
    }
    const bool near = hero_grinds_rail_of(pv) || (!(16.0f < abs_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10)))) &&
                                                  !(16.0f < abs_f(fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14)))));
    if (!near) {
        class386_sound_stop(m, pv);
        return;
    }
    if (sub_0028d8c0(m, lw(pv + 0x204)) == 0) sw(pv + 0x204, static_cast<uint32_t>(moby_sound_play(6, 4, m, 0xd3)));
    class386_shock(m);
    const uint32_t mid = ee_stack_alloc(0x20);
    {
        const float c = fpu_mul(cos_vu(lwc1(m + 0x48)), 3.125f);
        swc1(mid + 0x0, c);
        swc1(mid + 0x4, fpu_mul(sin_vu(lwc1(m + 0x48)), 3.125f));
        sw(mid + 0x8, 0);
        vec3_add(V(mid), V(m + 0x10), V(mid));
        V(mid + 0x10) = V(mid);
        swc1(mid + 0x1c, 4.5f);
    }
    const bool shown = point_on_screen(mid + 0x10, 64.0f) >= 0;
    ee_stack_free(0x20);
    if (shown) class386_strings_move(m);
}

}  // namespace rac1
