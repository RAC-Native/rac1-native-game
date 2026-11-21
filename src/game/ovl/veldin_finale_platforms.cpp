// Veldin's finale (level 18): two platforms. Class 586 (two on the level): a lift (+0x60 set: it rises to its top, and
// from its second stop, the hero on it, rises on with a sound; there it draws its beam, L18:0x2d8098, and after its wait
// sets the level's flag L18:0x162390) or a pad (+0x60 0: the hero stepping on it sinks it, sets Veldin's flag 0x13d407 on
// the finale, and near it the hero hears Clank's hint 0x2b02 once the time for it has passed since he last heard it).
// Class 587 (28 on the level): the floating platforms that bob and rock on the water; stepped on they sink, faster as the
// level's flood rises (L18:0x1623a8), bubbling, taking the hero down with them (his state 0x77) under the water's level,
// and are gone at z 15. Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's
// bare (the hero 0x13f350: +0x2fc the moby he stands on, +0x80 his position; his state 0x1413d4; the sound slots
// 0x13e550). Registered in veldin_finale_mobys.cpp.
//
// The pvars: class 586 +0x3e flags, +0x60 a lift, +0x64 its height at the start, +0x68 its speed, +0x6c the beam's wait,
// +0x70 its sound slot; class 587 +0x20 the attach frame (what rides it follows), +0x5c flags, +0x60 its height at the
// start, +0x64 its height now, +0x68 / +0x6c its pitch's spring (+0x6c the pitch's step a frame), +0x70 its sinking speed,
// +0x74 its bob's phase, +0x7c the wait before it rocks, +0x80 sinks faster, +0x84 / +0x88 its kind, +0x8c the bubbles'
// wait, +0x92 its sound.
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
constexpr uint32_t HERO = 0x0013f350;       // the hero
constexpr uint32_t DIALOGUE = 0x00141968;   // the dialogues' records (8 bytes each: +0 heard, +2 when, in 10 s)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the hero standing on the moby `m` (and not +0x30e) in his state 0x22
bool hero_steps_on(uint32_t m) { return lh(HERO + 0x30e) == 0 && lwu(HERO + 0x2fc) == m && lw(g::HERO_STATE) == 0x22; }

// the flood's level between L18:0x161a60 and 0x161a64, 0 .. 1
float flood() {
    const float lo = lwc1(L18_ADDR(0x00161a60));
    float x = fpu_div(fpu_sub(int_to_float(lw(L18_ADDR(0x001623a8))), lo), fpu_sub(lwc1(L18_ADDR(0x00161a64)), lo));
    if (1.0f < x) x = 1.0f;
    else if (x < 0.0f) x = 0.0f;
    return x;
}

// the game's time in tens of seconds
int32_t tens_of_seconds() { return ee_div(frame_scale_i(lw(0x0015eea4)), 600); }
}  // namespace

// ---- class 586: the lift and the pad

// States (the table L18:0x1f2670): 0 set up (a lift state 1, a pad 4); 1 and 3 rise to the top (+0x64 + L18:0x161a18); 2 at
// the second stop (+0x64 + L18:0x161a14), the beam and its wait, the hero on it: 3; 4 the pad, the hero on it: 5;
// 5 sinks by 1.5, then 6 (nothing).
// orig: L18:0x002d7b20
void vf_class586_update_l18(uint32_t m) {
    const Frame f(64);
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002d7b64)
        swc1(pv + 0x64, lwc1(m + 0x18));
        sh(pv + 0x3e, lhu(pv + 0x3e) | 8);
        if (lw(pv + 0x60) == 0) {
            sb(m + 0x20, 4);
            return;
        }
        sb(m + 0x20, 1);
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L18_ADDR(0x00161a10))));
        return;
    case 1:
    case 3:  // (0x002d7ba8)
        seek_1d(m + 0x18, pv + 0x68, fpu_add(lwc1(pv + 0x64), lwc1(L18_ADDR(0x00161a18))), fpu_mul(dt_sq(), 20.0f), fpu_mul(dt_sq(), 40.0f),
                fpu_mul(dt(), 20.0f));
        return;
    case 2: {  // (0x002d7bf4)
        seek_1d(m + 0x18, pv + 0x68, fpu_add(lwc1(pv + 0x64), lwc1(L18_ADDR(0x00161a14))), fpu_mul(dt_sq(), 5.0f), fpu_mul(dt_sq(), 10.0f),
                fpu_mul(dt(), 20.0f));
        if (hero_steps_on(m)) {
            sb(m + 0x20, 3);
            moby_sound_start(0, 0, m);
            const int32_t slot = lw(pv + 0x70);
            if (slot != -1) {
                const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot) * 0x70u;
                if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
            }
            sw(pv + 0x70, 0xffffffffu);
            return;
        }
        if (lw(g::HERO_STATE) == 0x72) return;
        sub_001f93e0(L18_ADDR(0x002d8098), m);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x6c))) != 0) sw(L18_ADDR(0x00162390), 1);
        return;
    }
    case 4: {  // (0x002d7d0c)
        if (hero_steps_on(m)) {
            sb(m + 0x20, 5);
            sb(m + 0xbc, 1);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 1.5f));
            if (lw(0x0015ed84) == 0x12) sb(0x0013d407, 1);
        }
        if (!(vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 2.5f)) return;
        const uint32_t level_bit = 1u << (lwu(0x0015ed84) & 31);
        if ((lwu(DIALOGUE + 0x10000 + 0x5814) & level_bit) != 0) return;  // (the level's bit: heard here already)
        const uint32_t rec = DIALOGUE + (0x71 << 3);                          // (the hint's record)
        if (lhu(rec) != 0) {
            const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(rec + 2)) * 600;
            const int32_t wait = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(static_cast<float>(frame_scale_i(0x12)), 60.0f))));
            if (wait < since || static_cast<int32_t>(lhu(rec + 2)) * 600 == 0) {
                sub_00203908(0x2b02, 0x71);
                return;
            }
            if (static_cast<int32_t>(lhu(rec + 2)) < tens_of_seconds()) sh(rec + 2, static_cast<uint32_t>(tens_of_seconds()));
            return;
        }
        sh(rec, lhu(rec) + 1);
        if (static_cast<int32_t>(lhu(rec + 2)) < tens_of_seconds()) sh(rec + 2, static_cast<uint32_t>(tens_of_seconds()));
        sw(rec + 4, lwu(rec + 4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
        return;
    }
    case 5:  // (0x002d7f0c)
        if (approach_f(m + 0x18, fpu_sub(lwc1(pv + 0x64), 1.5f), u2f(0x3e19999au)) == 0.0f) sb(m + 0x20, 6);
        return;
    default:
        return;
    }
}

// ---- class 587: the floating platforms

// Its rocking (+0x44 by +0x6c a frame, held within 2 degrees, 5 while it rocks, 30 while it sinks) and its bob (its height
// +0x64 and a sine of the phase +0x74 by L18:0x161a48).
// orig: L18:0x002d8710
void vf_float_bob_l18(uint32_t m) {
    const Frame f(48);
    const uint32_t pv = lwu(m + 0x78);
    const float a = angle_add(lwc1(m + 0x44), lwc1(pv + 0x6c));
    swc1(m + 0x44, a);
    const uint32_t st = lbu(m + 0x20);
    const float lim = st == 5 ? u2f(0x3f060a92u) : st == 4 ? u2f(0x3db2b8c2u) : u2f(0x3d0efa35u);
    if (lim < a) swc1(m + 0x44, lim);
    else if (a < -lim) swc1(m + 0x44, -lim);
    const float step = fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00161a4c)), u2f(0x3c8efa35u)), dt());  // (degrees a second)
    const float phase = angle_add(lwc1(pv + 0x74), step);
    swc1(pv + 0x74, phase);
    swc1(m + 0x18, fpu_add(lwc1(pv + 0x64), fpu_mul(lwc1(L18_ADDR(0x00161a48)), sin_vu(phase))));
}

// Its bubbles: 5 to 9 particles of type 2 from a random point 1 to 2.5 from its middle, rising along its z.
// orig: L18:0x002d88b8
void vf_float_bubbles_l18(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp, at = sp + 0x10, up = sp + 0x20, vel = sp + 0x30, drift = sp + 0x40, spread = sp + 0x50;
    const uint32_t mat = m + 0xc0;
    const int32_t n = rand_mod(5) + 5;
    const float r = rand_range_f(1.0f, 2.5f);
    const float a = rand_angle();
    swc1(at + 0, fpu_mul(cos_vu(a), r));
    swc1(at + 4, fpu_mul(sin_vu(a), r));
    sw(at + 8, 0);
    mat3_transform(V(at), V(at), M(mat));
    vec3_add(V(at), V(at), V(m + 0x10));
    vec_set_unit_z(V(up));
    mat3_transform(V(up), V(up), M(mat));
    auto spread_of = [](uint32_t k) {  // 1 +- the value at `k`
        const float s = lwc1(k);
        return fpu_add(rand_range_f(-s, s), 1.0f);
    };
    for (int32_t k = n; k > 0; --k) {
        rand_sphere_point(spread, 0.0f, fpu_mul(lwc1(L18_ADDR(0x00161a78)), dt()));
        vec3_set_length(V(vel), V(up), fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00161a68)), spread_of(L18_ADDR(0x00161a74))), dt()));
        vec3_add(V(vel), V(vel), V(spread));
        vec3_set_length(V(drift), V(vel), fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00161a6c)), spread_of(L18_ADDR(0x00161a74))), dt()));
        swc1(drift + 8, fpu_sub(lwc1(drift + 8), fpu_mul(lwc1(L18_ADDR(0x00161a70)), dt())));
        swc1(vel + 0xc, rand_range_f(lwc1(L18_ADDR(0x00161a8c)), lwc1(L18_ADDR(0x00161a90))));
        swc1(drift + 0xc, rand_range_f(lwc1(L18_ADDR(0x00161a94)), lwc1(L18_ADDR(0x00161a98))));
        const uint32_t c0 = rgba_lerp(lwu(L18_ADDR(0x00161a9c)), lwu(L18_ADDR(0x00161aa0)), rand_range_f(0.0f, 1.0f));
        const uint32_t c1 = rgba_lerp(lwu(L18_ADDR(0x00161aa4)), lwu(L18_ADDR(0x00161aa8)), rand_range_f(0.0f, 1.0f));
        const float r0 = rand_range_f(0.0f, 1.0f);
        const int32_t life = float_to_int(frame_scale_f(fpu_add(fpu_mul(static_cast<float>(lw(L18_ADDR(0x00161a7c))), r0), 1.0f)));
        const int32_t fade =
            float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L18_ADDR(0x00161a80))), spread_of(L18_ADDR(0x00161a88)))));
        const int32_t grow =
            float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L18_ADDR(0x00161a84))), spread_of(L18_ADDR(0x00161a88)))));
        spawn_particle_02(at, vel, drift, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(fade), static_cast<uint32_t>(grow),
                          lw(L18_ADDR(0x00161ab4)));
    }
}

// States (the table L18:0x1f2690): 0 set up (with +0x88 state 2, else 1); 1 and 2 bob; 3 after its wait starts rocking (the
// pitch's step by its kind and the flood) and goes to 4; 4 bubbles now and then, the hero on it (or near it with +0x80)
// with a sound: 5; 5 sinks (bubbling more), under its start by 10 taking the hero under the water's level L18:0x15f638
// down (his state 0x77), gone below z 15. Every state but the last's end then bobs and moves what rides it.
// orig: L18:0x002d82c0
void vf_class587_update_l18(uint32_t m) {
    const Frame f(96);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    const uint32_t move = sp, was_ang = sp + 0x10;
    vec3_scale(V(move), V(pos), -1.0f);
    lq_sq(was_ang, m + 0x40);
    // the bubbles when shown and their wait is over, the next wait `lo` .. `hi` frames
    auto bubbles = [m, pv](int32_t lo, int32_t hi) {
        if (lbu(m + 0x31) == 0) return;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x8c))) == 0) return;
        vf_float_bubbles_l18(m);
        sw(pv + 0x8c, static_cast<uint32_t>(frame_scale_i(rand_range(lo, hi))));
    };
    auto start_sinking = [m, pv]() {
        sb(m + 0x20, 5);
        sh(pv + 0x92, static_cast<uint32_t>(moby_sound_start(1, 0, m)));
    };
    switch (lbu(m + 0x20)) {  // (the table L18:0x1f2690)
    case 0:  // (0x002d832c)
        swc1(pv + 0x60, lwc1(m + 0x18));
        sw(pv + 0x70, 0);
        sw(pv + 0x68, 0);
        sw(pv + 0x6c, 0);
        swc1(pv + 0x64, lwc1(m + 0x18));
        swc1(pv + 0x74, rand_angle());
        sw(pv + 0x5c, lwu(pv + 0x5c) | 1);
        sb(m + 0x20, lw(pv + 0x88) != 0 ? 2 : 1);
        break;
    case 3: {  // (0x002d837c)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x7c))) == 0) break;
        const float k = fpu_sub(1.0f, fpu_mul(flood(), u2f(0x3eaa7efau)));  // (1 - a third of the flood)
        const float rate = lw(pv + 0x84) != 0 ? lwc1(L18_ADDR(0x00161a50)) : lwc1(L18_ADDR(0x00161a58));
        swc1(pv + 0x6c, fpu_mul(fpu_mul(fpu_mul(rate, k), u2f(0x3c8efa35u)), dt()));
        sb(m + 0x20, 4);
        break;
    }
    case 4:  // (0x002d8440)
        bubbles(0x5a, 0x78);
        if (lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0) {
            sw(pv + 0x8c, 0);
            start_sinking();
        }
        if (lw(pv + 0x80) != 0 && vec2_distance(V(pos), V(HERO + 0x80)) < lwc1(L18_ADDR(0x00161a5c))) start_sinking();
        break;
    case 5: {  // (0x002d8504)
        bubbles(0xf, 0x1e);
        const float k = fpu_sub(1.0f, fpu_mul(flood(), u2f(0x3eaa7efau)));
        float acc;
        if (lw(pv + 0x80) != 0 || (lw(pv + 0x88) != 0 && lw(pv + 0x84) == 0)) {
            acc = fpu_mul(dt_sq(), 15.0f);
        } else {
            const float rate = lw(pv + 0x84) != 0 ? lwc1(L18_ADDR(0x00161a40)) : lwc1(L18_ADDR(0x00161a44));
            acc = fpu_mul(fpu_mul(rate, k), dt_sq());
        }
        swc1(pv + 0x70, fpu_sub(lwc1(pv + 0x70), acc));
        swc1(pv + 0x64, fpu_add(lwc1(pv + 0x64), lwc1(pv + 0x70)));
        if (!(lwc1(m + 0x18) < fpu_sub(lwc1(pv + 0x60), 10.0f))) break;
        if (lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0) {
            if (lwc1(HERO + 0x88) < lwc1(L18_ADDR(0x0015f638)) && lw(g::HERO_STATE) != 0x77) hero_set_state(0x77, 1);
        }
        swc1(pv + 0x70, fpu_sub(lwc1(pv + 0x70), fpu_mul(dt_sq(), u2f(0x415b3333u))));  // (13.7)
        if (lwc1(m + 0x18) < 15.0f) {
            moby_retire(m);
            return;
        }
        break;
    }
    default:  // 1, 2 (0x002d86cc) and past the table
        break;
    }
    vf_float_bob_l18(m);
    vec3_add(V(move), V(move), V(pos));
    attach_frame_set(pv + 0x20, move, was_ang, m + 0x40);
}

}  // namespace rac1
