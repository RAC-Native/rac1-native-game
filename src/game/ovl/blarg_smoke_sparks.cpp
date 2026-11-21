// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1512, a damaged spot of the station that puffs smoke or
// throws sparks (either from the start, or after it blows up when another moby sets it off), only while on screen. Header
// blarg_own.h; registered in blarg_mobys.cpp. Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR (the smoke's
// tuning 0x1622f0 .. 0x16232c, the sparks' 0x162330 .. 0x16237c), the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// a word of the level's data read as an integer and turned into a float (cvt.s.w)
float int_word(uint32_t a) { return static_cast<float>(lw(a)); }

// a random factor 1 - spread .. 1 + spread, the spread a float of the level's data at `spread`
float jitter(uint32_t spread) {
    const float s = lwc1(spread);
    return fpu_add(rand_range_f(-s, s), 1.0f);
}

// a whole number of frames, scaled to the frame rate, of the float `frames60`
int32_t scaled_frames(float frames60) { return float_to_int(frame_scale_f(frames60)); }

// The particles' life and fade frame counts (spawn_particle_02's n / h32 / h34): the level's word `life` (times 0 .. 1,
// plus 1) and the words `fade_a` / `fade_b` (each times 1 +- the spread at `spread`).
struct Frames {
    int32_t life, fade_a, fade_b;
};
Frames particle_frames(uint32_t life, uint32_t fade_a, uint32_t fade_b, uint32_t spread) {
    Frames f{};
    f.life = scaled_frames(fpu_add(fpu_mul(int_word(life), rand_range_f(0.0f, 1.0f)), 1.0f));
    f.fade_a = scaled_frames(fpu_mul(int_word(fade_a), jitter(spread)));
    f.fade_b = scaled_frames(fpu_mul(int_word(fade_b), jitter(spread)));
    return f;
}
}  // namespace

// Class 1512, a smoking or sparking spot. pvars: +0 sparks (else smoke), +4 set off by another moby first, +0x20 the
// sparks' timer, +0x24 the smoke's timer, +0x28 the blast's timer. Nothing happens while its position (radius 3) is off
// screen (64). States: 0 set up: its matrix +0xc0 from its rotation (1 with +4, else 3 / 4); 1 waits (another moby sets 2
// and the timer +0x28); 2 at the timer's end a blast (explosion_burst 0.333, 13) (3 / 4); 3 sparks: at each end of the
// timer +0x20 (0x162374 .. 0x162378 frames) a burst of 1, or 1 in 7 times 5 .. 9, sparks (type 0x16237c) thrown along
// its forward axis (+0xc0) with a random push (0x162340), falling (0x162338); 4 smoke: a puff a frame 1 in 4 times, and
// at each end of the timer +0x24 (60 .. 120 frames) five, drifting along its forward axis (0x1622f0), rising (0x1622f4)
// and spread sideways along +0xd0 (0x1622f8).
// orig: L6:0x00308c68
void blg_class1512_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t spot = sp + 0x10, vel = sp + 0x20, push = sp + 0x30, side = sp + 0x40;
    lq_sq(spot, m + 0x10);
    swc1(spot + 0xc, 3.0f);
    if (point_on_screen(spot, 64.0f) == -1) {
        ee_stack_free(0x110);
        return;
    }
    auto settle = [&] { sb(m + 0x20, lw(pv + 0x0) != 0 ? 4 : 3); };  // sparks or smoke
    switch (lbu(m + 0x20)) {
    case 0:
        euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
        if (lw(pv + 0x4) != 0)
            sb(m + 0x20, 1);
        else
            settle();
        break;
    case 2:
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x28))) == 0) break;
        explosion_burst(m, m + 0x10, -1, 0.333f, 13.0f);
        settle();
        break;
    case 3: {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) == 0) break;
        sw(pv + 0x20, static_cast<uint32_t>(
                          scaled_frames(rand_range_f(int_word(L6_ADDR(0x00162374)), int_word(L6_ADDR(0x00162378))))));
        const int32_t n = rand_mod(7) != 0 ? 1 : rand_mod(5) + 5;
        for (int32_t k = 0; k < n; ++k) {
            rand_sphere_point(side, 0.0f, fpu_mul(lwc1(L6_ADDR(0x00162340)), dt()));
            vec3_set_length(V(vel), V(m + 0xc0), fpu_mul(fpu_mul(lwc1(L6_ADDR(0x00162330)), jitter(L6_ADDR(0x0016233c))), dt()));
            vec3_add(V(vel), V(vel), V(side));
            vec3_set_length(V(push), V(vel), fpu_mul(fpu_mul(lwc1(L6_ADDR(0x00162334)), jitter(L6_ADDR(0x0016233c))), dt()));
            swc1(push + 0x8, fpu_sub(lwc1(push + 0x8), fpu_mul(lwc1(L6_ADDR(0x00162338)), dt())));
            swc1(vel + 0xc, rand_range_f(lwc1(L6_ADDR(0x00162354)), lwc1(L6_ADDR(0x00162358))));
            swc1(push + 0xc, rand_range_f(lwc1(L6_ADDR(0x0016235c)), lwc1(L6_ADDR(0x00162360))));
            const uint32_t c0 = rgba_lerp(lwu(L6_ADDR(0x00162364)), lwu(L6_ADDR(0x00162368)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(L6_ADDR(0x0016236c)), lwu(L6_ADDR(0x00162370)), rand_range_f(0.0f, 1.0f));
            const Frames f = particle_frames(L6_ADDR(0x00162344), L6_ADDR(0x00162348), L6_ADDR(0x0016234c), L6_ADDR(0x00162350));
            spawn_particle_02(m + 0x10, vel, push, c0, c1, static_cast<uint32_t>(f.life), static_cast<uint32_t>(f.fade_a),
                              static_cast<uint32_t>(f.fade_b), lw(L6_ADDR(0x0016237c)));
        }
        break;
    }
    case 4: {
        int32_t n = 1;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x24))) != 0) {
            n = 5;
            sw(pv + 0x24, static_cast<uint32_t>(scaled_frames(rand_range_f(60.0f, 120.0f))));
        }
        if (rand_mod(4) != 0 && n < 2) break;
        for (int32_t k = 0; k < n; ++k) {
            vec3_set_length(V(vel), V(m + 0xc0), fpu_mul(fpu_mul(lwc1(L6_ADDR(0x001622f0)), jitter(L6_ADDR(0x001622fc))), dt()));
            const float rise = fpu_mul(fpu_mul(lwc1(L6_ADDR(0x001622f4)), jitter(L6_ADDR(0x001622fc))), dt());
            vec_clear(V(push));
            swc1(push + 0x8, rise);
            const float s = lwc1(L6_ADDR(0x001622f8));
            vec3_set_length(V(side), V(m + 0xd0), fpu_mul(rand_range_f(-s, s), dt()));
            vec3_add(V(push), V(push), V(side));
            swc1(vel + 0xc, rand_range_f(lwc1(L6_ADDR(0x00162310)), lwc1(L6_ADDR(0x00162314))));
            swc1(push + 0xc, rand_range_f(lwc1(L6_ADDR(0x00162318)), lwc1(L6_ADDR(0x0016231c))));
            const uint32_t c0 = rgba_lerp(lwu(L6_ADDR(0x00162320)), lwu(L6_ADDR(0x00162324)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(L6_ADDR(0x00162328)), lwu(L6_ADDR(0x0016232c)), rand_range_f(0.0f, 1.0f));
            const Frames f = particle_frames(L6_ADDR(0x00162300), L6_ADDR(0x00162304), L6_ADDR(0x00162308), L6_ADDR(0x0016230c));
            spawn_particle_02(m + 0x10, vel, push, c0, c1, static_cast<uint32_t>(f.life), static_cast<uint32_t>(f.fade_a),
                              static_cast<uint32_t>(f.fade_b), -1);
        }
        break;
    }
    default:  // (1 and on)
        break;
    }
    ee_stack_free(0x110);
}

}  // namespace rac1
