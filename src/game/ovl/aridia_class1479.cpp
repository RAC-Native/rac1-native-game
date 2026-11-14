// Aridia's own moby classes: class 1479, a steam vent (two builds by its pvar byte +0: steady, or puffing harder now and
// then), hand-ported from Aridia's overlay (`orig: L2:`). Its particles' speeds, colours and lives come from the level's
// records (0x162048 .., 0x162088 .., 0x1620c8 .., 0x162118 ..). Declared in aridia_enemies.h, registered in
// aridia_mobys.cpp.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l2.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }

// 1 + a random jitter within +-the float at `j`
float jitter(uint32_t j) {
    const float k = lwc1(j);
    return fpu_add(rand_range_f(-k, k), 1.0f);
}

// a frame count: the level's integer at `n` scaled by `k` (60 Hz frames, scaled)
int32_t frames_of(float n, float k) { return float_to_int(frame_scale_f(fpu_mul(n, k))); }

// The particles' common tail: the velocity's w and the acceleration's w (vel +0xc, acc +0xc) from the ranges at `w`, the two
// colours blended at random from the pairs at `c`, the life (life_base * a random 0..1, plus 1) and the two counts at `n`
// scaled by 1 +- the jitter at `j2`; then the particle 2 at the moby with the velocity and acceleration at vel / acc.
void vent_particle(uint32_t m, uint32_t vel, uint32_t acc, uint32_t w, uint32_t c, float life_base, uint32_t n, uint32_t j2,
                   int32_t tex) {
    swc1(vel + 0xc, rand_range_f(lwc1(w), lwc1(w + 4)));
    swc1(acc + 0xc, rand_range_f(lwc1(w + 8), lwc1(w + 0xc)));
    const uint32_t c0 = rgba_lerp(lwu(c), lwu(c + 4), rand_range_f(0.0f, 1.0f));
    const uint32_t c1 = rgba_lerp(lwu(c + 8), lwu(c + 0xc), rand_range_f(0.0f, 1.0f));
    const int32_t life = float_to_int(frame_scale_f(fpu_add(fpu_mul(life_base, rand_range_f(0.0f, 1.0f)), 1.0f)));
    const int32_t a = frames_of(static_cast<float>(lw(n)), jitter(j2));
    const int32_t b = frames_of(static_cast<float>(lw(n + 4)), jitter(j2));
    spawn_particle_02(m + 0x10, vel, acc, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(a), static_cast<uint32_t>(b), tex);
}

// A burst of `n` sparks (a 0x50-byte record `r`: the speeds +0 / +4, the fall +8, the jitter +0xc, the spread +0x10, the
// life +0x14, the counts +0x18 / +0x1c and their jitter +0x20, the w ranges +0x24, the colours +0x34, the texture
// +0x4c): out along the moby's forward row (+0xc0) with a random spread, falling.
void vent_burst(uint32_t m, uint32_t sp, int32_t n, uint32_t r) {
    const uint32_t vel = sp + 0x20, acc = sp + 0x30, spread = sp + 0x40;
    for (int32_t k = 0; k < n; ++k) {
        rand_sphere_point(spread, 0.0f, fpu_mul(lwc1(r + 0x10), dt()));
        vec3_set_length(V(vel), V(m + 0xc0), fpu_mul(fpu_mul(lwc1(r), jitter(r + 0xc)), dt()));
        vec3_add(V(vel), V(vel), V(spread));
        vec3_set_length(V(acc), V(vel), fpu_mul(fpu_mul(lwc1(r + 4), jitter(r + 0xc)), dt()));
        swc1(acc + 8, fpu_sub(lwc1(acc + 8), fpu_mul(lwc1(r + 8), dt())));
        vent_particle(m, vel, acc, r + 0x24, r + 0x34, static_cast<float>(lw(r + 0x14)), r + 0x18, r + 0x20, lw(r + 0x4c));
    }
}

// The plume's puff (a record `r`: the speeds +0 / +4, the drift across +8, the jitter +0xc, the counts +0x14 / +0x18 and
// their jitter +0x1c, the w ranges +0x20, the colours +0x30): out along the forward row, rising, drifting along the side
// row (+0xd0); its life `life_base`.
void vent_plume(uint32_t m, uint32_t sp, uint32_t r, float life_base) {
    const uint32_t vel = sp + 0x20, acc = sp + 0x30, side = sp + 0x40;
    vec3_set_length(V(vel), V(m + 0xc0), fpu_mul(fpu_mul(lwc1(r), jitter(r + 0xc)), dt()));
    const float rise = fpu_mul(fpu_mul(lwc1(r + 4), jitter(r + 0xc)), dt());
    vec_clear(V(acc));
    swc1(acc + 8, rise);
    vec3_set_length(V(side), V(m + 0xd0), fpu_mul(rand_range_f(-lwc1(r + 8), lwc1(r + 8)), dt()));
    vec3_add(V(acc), V(acc), V(side));
    vent_particle(m, vel, acc, r + 0x20, r + 0x30, life_base, r + 0x14, r + 0x1c, -1);
}

// the vent on screen (its place within 120, a 6 margin)
bool on_screen(uint32_t m, uint32_t at) {
    lq_sq(at, m + 0x10);
    swc1(at + 0xc, 6.0f);
    return point_on_screen(at, 120.0f) != -1;
}

// the burst's count: 1, or 5 .. 9 one time in seven
int32_t burst_count() { return rand_mod(7) != 0 ? 1 : rand_mod(5) + 5; }
}  // namespace

// Class 1479, a steam vent: state 0 picks the build (1 + pvar byte +0) and turns its matrix to its angles; build 1 (state
// 1), on screen: a burst of sparks when the timer +0xc0 runs out (the record 0x1620c8), and a plume puff one frame in four
// (0x162088); build 2 (state 2): its puffs' life (+0xc8) switches between 30 and 10 frames every 120 .. 180 (+0xc4), the
// bursts from 0x162118 (off screen only one frame in four), a puff (0x162048) every frame; state 3 is gone.
// orig: L2:0x002ef020
void class1479_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x20, lbu(pv) + 1);
        euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
        return;
    }
    if (state == 3) {
        moby_retire(m);
        return;
    }
    if (state != 1 && state != 2) return;
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t at = sp + 0x10;
    auto timer = [pv](uint32_t t) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + t))); };
    auto rearm = [pv](uint32_t lo) {
        sw(pv + 0xc0, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(static_cast<float>(lw(lo)), static_cast<float>(lw(lo + 4)))))));
    };
    if (state == 1) {
        if (on_screen(m, at)) {
            if (timer(0xc0) != 0) {
                rearm(L2_ADDR(0x0016210c));
                vent_burst(m, sp, burst_count(), L2_ADDR(0x001620c8));
            }
            if (rand_mod(4) == 0) vent_plume(m, sp, L2_ADDR(0x00162088), static_cast<float>(lw(L2_ADDR(0x00162098))));
        }
    } else {
        if (timer(0xc4) != 0) {
            sw(pv + 0xc8, lw(pv + 0xc8) < 21 ? 0x1eu : 0xau);
            sw(pv + 0xc4, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(120.0f, 180.0f)))));
        }
        if (on_screen(m, at) || rand_mod(4) == 0) {
            if (timer(0xc0) != 0) {
                rearm(L2_ADDR(0x0016215c));
                vent_burst(m, sp, burst_count(), L2_ADDR(0x00162118));
            }
            vent_plume(m, sp, L2_ADDR(0x00162048), static_cast<float>(lw(pv + 0xc8)));
        }
    }
    ee_stack_free(0x120);
}

}  // namespace rac1
