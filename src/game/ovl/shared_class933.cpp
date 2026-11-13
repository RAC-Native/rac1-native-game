// Class 933 (Oltanis and Kalebo III), a glowing object that spins and bursts when it is hit, its spawn, and three helpers of
// the carrier that brings them (class 923 on Oltanis, 1401 on Kalebo III): one port of the copies both levels carry (the
// same code relinked; compared with the relocated fields masked, and normalised: no other difference). Their level-own
// words are the rate of the glow's pulse and the carrier's tuning (below); the rest is the engine's (OV: the first spawned
// moby 0x15ffdc, the level's paths 0x1b04b0, a zero vector 0x15f580) or the base ELF's (bare: the hero 0x13f350, the
// level 0x15ed84, the frame time 0x15ed64). Header shared_class933.h.
//
// Its pvars: the moby that brought it +0, its float height +4 and rise speed +8, its pulse +0xc, its spin rates
// +0x10..+0x18. The carrier's: its path +0xd8 and the next one +0xdc, its leg / distance +0xd0 / +0xd4, +0xe4..+0xf0 zeroed
// at the path's start, the one it holds +0xf4, +0xfc 1 when it has no next path.
#include <game/ovl/shared_class933.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {

// the glow's pulse rate (radians a frame)
float pulse_rate() { return lwc1(on_level(OLTANIS) ? L14_ADDR(0x00162000) : L16_ADDR(0x00161c48)); }

// the carrier's tuning: where it holds one (+0 / +4: along its x / y rows), where its sparks start (+8 / +0xc, +0x10 /
// +0x14), the sparks' colour +0x20
uint32_t carrier_words() { return on_level(OLTANIS) ? L14_ADDR(0x00161fc4) : L16_ADDR(0x00161d9c); }
float carrier_word(uint32_t k) { return lwc1(carrier_words() + k); }

// the point `along_x` of the carrier's x row and `along_y` of its y row from it, into `out` (`tmp` a scratch vector)
void carrier_point(uint32_t c, uint32_t out, uint32_t tmp, float along_x, float along_y) {
    vec3_scale(V(tmp), V(c + 0xc0), along_x);
    vec3_add(V(out), V(c + 0x10), V(tmp));
    vec3_scale(V(tmp), V(c + 0xe0), along_y);
    vec3_add(V(out), V(out), V(tmp));
}

// the w lanes of a path's points: each the distance to the next
void path_lengths(uint32_t path) {
    for (int32_t i = 0; i < lw(path) - 1; ++i) {
        const uint32_t at = path + 0x10 + static_cast<uint32_t>(i) * 0x10;
        swc1(at + 0xc, vec3_distance(V(at), V(at + 0x10)));
    }
}

// its height above the floor below it: 0.7
float floor_height(uint32_t m) { return fpu_add(floor_probe_v(0.5f, V(m + 0x10), 0), u2f(0x3f333333u)); }

// The glow (the colour +0x90: red 0xcf, green and blue 0x30, with the pulse's sine) and the spin.
void glow_and_spin(uint32_t m, uint32_t pv) {
    const float phase = angle_add(lwc1(pv + 0xc), pulse_rate());
    swc1(pv + 0xc, phase);
    const float s = sin_vu(phase);
    const uint32_t red = static_cast<uint32_t>(float_to_int(fpu_mul(s, 48.0f)) + 0xcf);
    const uint32_t green = static_cast<uint32_t>(float_to_int(fpu_mul(s, 32.0f)) + 0x30);
    sw(m + 0x90, 0xff000000u | (green << 16) | (green << 8) | red);
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x18)));
}

}  // namespace

// State 0 (start): without a moby to wait on (pvars +0) it sits 0.7 above the floor and takes its spin rates at random
// (0.6..1.2, 0.4..0.8, 0.2..0.4 degrees a frame), then state 1; with one, state 1 once that moby is gone (its state's top
// bit). State 1: on Kalebo III it rises to its float height (found once) on a spring, and a spawned one leaves unless the
// hero's mode (+0x208c) is 15; then a hit of the kinds 0x230000 bursts it (when the hit's state is 2 or more and its moby
// is not of class 0x283), as does the hero's +0x23c naming it: a burst (bigger when it is the hero's), its sound, and a
// placed one hides (state 2) while a spawned one leaves. State 2 shows it again (state 1) once its place (3 up) is off
// screen. In every state but a burst it glows and spins.
// orig: L14:0x002ff640 L16:0x002ddde0
void class933_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hero = 0x0013f350;
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t spec = sp + 0x20, none = sp + 0x60, hit_state = sp + 0x70;
    switch (lbu(m + 0x20)) {
    case 0: {
        const uint32_t wait_on = lwu(pv);
        swc1(pv + 4, -1.0f);
        if (wait_on != 0) {
            if (static_cast<int8_t>(lbu(wait_on + 0x20)) < 0) sb(m + 0x20, 1);
            break;
        }
        swc1(m + 0x18, floor_height(m));
        swc1(pv + 0x10, rand_range_f(u2f(0x3c2b92a6u), u2f(0x3cab92a6u)));
        swc1(pv + 0x14, rand_range_f(u2f(0x3be4c388u), u2f(0x3c64c388u)));
        swc1(pv + 0x18, rand_range_f(u2f(0x3b64c388u), u2f(0x3be4c388u)));
        sb(m + 0x20, 1);
        break;
    }
    case 1: {
        if (lw(0x0015ed84) == KALEBO) {
            if (lwc1(pv + 4) < 0.0f) swc1(pv + 4, floor_height(m));
            const float dt = lwc1(0x0015ed64);
            spring_to(m + 0x18, pv + 8, lwc1(pv + 4), fpu_mul(dt, u2f(0x3cf5c28fu)), fpu_mul(dt, u2f(0x3e99999au)), 0.0f);
            if (lwu(OV(0x0015ffdc)) < m && lw(hero + 0x208c) != 0xf) {
                moby_retire(m);
                ee_stack_free(0xe0);
                return;
            }
        }
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x230000u), 0);
        mem_clear128(spec, 0x40);
        sb(spec + 0x3b, 0xff);
        sb(spec + 0x3a, 7);
        sh(spec + 4, 1);
        swc1(spec, 1.0f);
        swc1(spec + 0x10, 0.5f);
        {
            int32_t st = lw(hit_state);  // (never set before: the frame's word)
            moby_hit_react_r(m, hit, spec, 0, st, nullptr, nullptr, 4);
            sw(hit_state, static_cast<uint32_t>(st));
        }
        const bool struck = !(lw(hit_state) < 2) && hit != 0 && lh(lwu(hit + 0x20) + 0xa6) != 0x283;
        if (!struck && lwu(hero + 0x23c) != m) break;
        vec_clear(V(none));
        moby_sound_start(0, 0, m);
        if (lwu(hero + 0x23c) == m)
            explosion_effect(m, none, m + 0x10, 0xa, 0x14, 0x3c, -1, 1, 1, -1, 0, 3.0f, 1.0f, 3.0f, 5.0f, 9.0f, 1.0f, 15.0f);
        else
            explosion_effect(m, none, m + 0x10, 0xa, 0x14, 0x3c, -1, 1, 1, -1, 0, 0.0f, 0.0f, 3.0f, 5.0f, 9.0f, 1.0f, 15.0f);
        if (m < lwu(OV(0x0015ffdc))) {
            sb(m + 0x30, 0xff);
            sb(m + 0x20, 2);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            sb(m + 0xa4, 0xff);
            sw(m + 0x94, 0);
        } else {
            moby_retire(m);
        }
        ee_stack_free(0xe0);
        return;
    }
    case 2: {
        const uint32_t at = sp + 0x20;
        lq_sq(at, m + 0x10);
        swc1(at + 0xc, 3.0f);
        if (point_on_screen(at, 64.0f) == -1) {
            sb(m + 0x20, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        }
        break;
    }
    default:
        break;
    }
    glow_and_spin(m, pv);
    ee_stack_free(0xe0);
}

// A class 933 of `owner` (pvars +0) at `pos` (an EE address), turned as the owner, with its spin rates at random; the
// moby, or 0.
// orig: L14:0x002ffaf8 L16:0x002de298
uint32_t class933_spawn(uint32_t owner, uint32_t pos) {
    const uint32_t r = moby_spawn(0x3a5);
    if (r == 0) return 0;
    const uint32_t pv = lwu(r + 0x78);
    sw(pv, owner);
    sw(pv + 8, 0);
    sw(pv + 0xc, 0);
    swc1(pv + 4, -1.0f);
    swc1(pv + 0x10, rand_range_f(u2f(0x3c2b92a6u), u2f(0x3cab92a6u)));
    swc1(pv + 0x14, rand_range_f(u2f(0x3be4c388u), u2f(0x3c64c388u)));
    swc1(pv + 0x18, rand_range_f(u2f(0x3b64c388u), u2f(0x3be4c388u)));
    lq_sq(r + 0x10, pos);
    swc1(r + 0x48, lwc1(owner + 0x48));
    sb(r + 0x20, 0);
    sb(r + 0x30, 0xff);
    sh(r + 0x32, 0xff);
    sb(r + 0x31, 1);
    moby_update_transform(r);
    return r;
}

// The carrier at the start of its path (pvars +0xd8): the path's legs measured, its leg and distance 0, at the path's
// first point facing the second, +0xe4..+0xf0 cleared; unless +0xfc is 1 its next path's (+0xdc) legs measured too.
// orig: L14:0x002fe900 L16:0x002e3e20
void class933_carrier_start(uint32_t c) {
    const uint32_t pv = lwu(c + 0x78);
    const uint32_t paths = OV(0x001b04b0);
    const uint32_t path = lwu(paths + (lwu(pv + 0xd8) << 2));
    path_lengths(path);
    sw(pv + 0xd0, 0);
    sw(pv + 0xd4, 0);
    lq_sq(c + 0x10, path + 0x10);
    swc1(c + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
    swc1(pv + 0xf0, 0.0f);
    swc1(pv + 0xec, 0.0f);
    swc1(pv + 0xe8, 0.0f);
    swc1(pv + 0xe4, 0.0f);
    if (lw(pv + 0xfc) == 1) return;
    path_lengths(lwu(paths + (lwu(pv + 0xdc) << 2)));
}

// The one the carrier holds (pvars +0xf4) kept at its place on it (along its x and y rows by the tuning's +0 / +4).
// orig: L14:0x002fec28 L16:0x002e4110
void class933_carrier_hold(uint32_t c) {
    const uint32_t pv = lwu(c + 0x78);
    if (lwu(pv + 0xf4) == 0) return;
    const uint32_t sp = ee_stack_alloc(0x40);
    vec3_set_length(V(sp), V(c + 0xc0), carrier_word(0));
    vec3_add(V(lwu(pv + 0xf4) + 0x10), V(c + 0x10), V(sp));
    vec3_set_length(V(sp), V(c + 0xe0), carrier_word(4));
    const uint32_t at = lwu(pv + 0xf4) + 0x10;
    vec3_add(V(at), V(at), V(sp));
    ee_stack_free(0x40);
}

// The carrier's sparks (particle 0x17): two that spin either way at random (at the tuning's +8 / +0xc point, its colour
// +0x20) and three larger, fading ones (at the +0x10 / +0x14 point, white, each 20000 smaller, spinning 16 each way in
// turn, living 2, 4, 8 frames).
// orig: L14:0x002feca8 L16:0x002e4190
void class933_carrier_sparks(uint32_t c) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t tmp = sp, at = sp + 0x10;
    const uint32_t still = OV(0x0015f580);  // (a zero vector)
    carrier_point(c, at, tmp, carrier_word(8), carrier_word(0xc));
    for (int k = 0; k < 2; ++k) {
        const int32_t spin = rand_mod(0x10);
        const int32_t way = rand_mod(2) == 0 ? spin : -spin;
        const uint32_t p = spawn_particle_17(at, way, still, lwu(carrier_words() + 0x20), u2f(0x3e4ccccdu), 1.0f,
                                             u2f(0x3f666666u), 100000.0f);
        if (p == 0) continue;
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0xc)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, 0x7f);
        sb(p + 0x2b, lbu(p + 0xa));
    }
    carrier_point(c, at, tmp, carrier_word(0x10), carrier_word(0x14));
    float size = 80000.0f;
    int32_t spin = 0x10;
    int32_t life = frame_scale_i(2);
    for (int k = 0; k < 3; ++k) {
        const int32_t way = spin;
        spin = -spin;
        const uint32_t p = spawn_particle_17(at, way, still, 0x7fffffffu, u2f(0x3d4ccccdu), 1.0f, 1.0f, size);
        size = fpu_sub(size, 20000.0f);
        if (p != 0) {
            sh(p + 0xa, static_cast<uint32_t>(life));
            sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
            sw(p + 0x24, 2);
            sb(p + 0x2a, 0x7f);
            sb(p + 0x2b, lbu(p + 0xa));
        }
        life <<= 1;
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
