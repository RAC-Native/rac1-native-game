// The puffer of Batalia (level 8) and Orxon (10), class 648: a vent that puffs a cloud (particle 2) every few frames, forward
// along the way it faces and up. Its tuning is its own pvars, or (pvar +0x38 set) the class's defaults, level data in another
// order. The two levels' copies are the same code, relinked: the defaults' address is the one level word. Header
// shared_puffer.h.
#include <game/ovl/shared_puffer.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
// the class's default tuning (level data)
uint32_t puffer_defaults() { return on_level(ORXON) ? L10_ADDR(0x00161954) : L8_ADDR(0x00161f74); }

struct PuffTuning {
    uint32_t colour0, colour1;
    uint32_t life, grow, fade;    // frame counts (+20 % at random)
    float speed0, rise0, scatter0;  // the cloud's first speed (+20 %), its rise (+20 %), its sideways scatter
    float speed1, rise1, scatter1;  // its second (+50 %)
    float w0, w1;                 // (the two speed vectors' fourth words)
};

float dt() { return lwc1(0x0015ed6c); }

// a frame count +20 % at random, in the frame rate's frames
int32_t frames(uint32_t n) {
    const float f = int_to_float(static_cast<int32_t>(n));
    return float_to_int(frame_scale_f(rand_range_f(f, fpu_mul(f, 1.2f))));
}
}  // namespace

// pvars: the colours +0 / +4, the frame counts +8 / +0xc / +0x10, the interval +0x14, the speeds and rises +0x18 .. +0x24,
// the w words +0x28 / +0x2c, the scatters +0x30 / +0x34, "use the defaults" +0x38, the timer +0x3c.
// orig: L8:0x002f5830 L10:0x002c74b8
void puffer_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    {
        int32_t timer = lw(pv + 0x3c);
        const int32_t r = timer_tick_i32(timer);
        sw(pv + 0x3c, static_cast<uint32_t>(timer));
        if (r == 0) return;
    }
    PuffTuning t{};
    const bool defaults = lw(pv + 0x38) != 0;
    if (defaults) {  // (the defaults' order: the colours, the rises and speeds, the frame counts, the interval, the w words, the scatters)
        const uint32_t d = puffer_defaults();
        t = {lwu(d), lwu(d + 4), lwu(d + 0x18), lwu(d + 0x1c), lwu(d + 0x20),
             lwc1(d + 0xc), lwc1(d + 8), lwc1(d + 0x30), lwc1(d + 0x14), lwc1(d + 0x10), lwc1(d + 0x34), lwc1(d + 0x28), lwc1(d + 0x2c)};
        sw(pv + 0x3c, lwu(d + 0x24));
    } else {
        t = {lwu(pv), lwu(pv + 4), lwu(pv + 8), lwu(pv + 0xc), lwu(pv + 0x10),
             lwc1(pv + 0x18), lwc1(pv + 0x1c), lwc1(pv + 0x30), lwc1(pv + 0x20), lwc1(pv + 0x24), lwc1(pv + 0x34), lwc1(pv + 0x28), lwc1(pv + 0x2c)};
        sw(pv + 0x3c, lwu(pv + 0x14));
    }
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t vel0 = sp + 0x10, vel1 = sp + 0x20, at = sp + 0x30, side = sp + 0x40, frame = sp + 0x50;
    const uint32_t forward = frame, sideways = frame + 0x10;  // (the facing's rows 0 and 1)
    euler_to_mat4_vu(M(frame), V(m + 0x40));
    V(at) = V(m + 0x10);
    const float jitter = 0.05f;
    swc1(at, fpu_add(lwc1(at), rand_range_f(-jitter, jitter)));
    swc1(at + 4, fpu_add(lwc1(at + 4), rand_range_f(-jitter, jitter)));
    const float dz = rand_range_f(-jitter, jitter);
    if (defaults) swc1(at, fpu_add(lwc1(at), dz));  // (the defaults' copy jitters x twice and z not at all)
    else swc1(at + 8, fpu_add(lwc1(at + 8), dz));
    rand_range_f(-u2f(0x3e32b8c2u), u2f(0x3e32b8c2u));  // (a turn of +-10 degrees drawn and not used)
    const float s0 = rand_range_f(t.speed0, fpu_mul(t.speed0, 1.2f));
    vec3_set_length(V(vel0), V(forward), fpu_mul(s0, dt()));
    const float c0 = rand_range_f(-t.scatter0, t.scatter0);
    vec3_set_length(V(side), V(sideways), fpu_mul(c0, dt()));
    vec3_add(V(vel0), V(vel0), V(side));
    const float r0 = rand_range_f(t.rise0, fpu_mul(t.rise0, 1.2f));
    swc1(vel0 + 8, fpu_add(lwc1(vel0 + 8), fpu_mul(r0, dt())));
    V(vel1) = V(vel0);
    const float s1 = rand_range_f(t.speed1, fpu_mul(t.speed1, 1.5f));
    vec3_set_length(V(vel1), V(forward), fpu_mul(s1, dt()));
    const float c1 = rand_range_f(-t.scatter1, t.scatter1);
    vec3_set_length(V(side), V(sideways), fpu_mul(c1, dt()));
    vec3_add(V(vel1), V(vel1), V(side));
    const float r1 = rand_range_f(t.rise1, fpu_mul(t.rise1, 1.5f));
    swc1(vel1 + 8, fpu_add(lwc1(vel1 + 8), fpu_mul(r1, dt())));
    swc1(vel0 + 0xc, t.w0);
    swc1(vel1 + 0xc, t.w1);
    const int32_t life = frames(t.life);
    const int32_t grow = frames(t.grow);
    const int32_t fade = frames(t.fade);
    spawn_particle_02(at, vel0, vel1, t.colour0, t.colour1, static_cast<uint32_t>(life), static_cast<uint32_t>(grow),
                      static_cast<uint32_t>(fade), -1);
    ee_stack_free(0xa0);
}

}  // namespace rac1
