// Class 1523's update and its streak: one port of the copies Quartu and Veldin carry (the same code relinked). Each level keeps
// one word of its own (per level below: the streaks' life); the rest is the engine's (OV: the charge light's switch 0x15f320,
// its glow 0x15f330 / colour 0x15f334 / blend register 0x15f338, the light regions 0x1802c0, the frame counter 0x15f5cc, the
// hit list 0x177f00, the shot's moby 0x1613c8, the zero vector 0x15f580, the spark texture 0x1b20ac) or the base ELF's (bare:
// the hero 0x13f350 and his moby's mode byte 0x1413f4, the frame time 0x15ed6c and its step 0x15ed60). Header
// shared_class1523.h.
//
// The pvars: +0 the velocity, +0x10 the frames charged, +0x14 the frames flown, +0x2c its light region (-1: none), +0x2e the
// button held (cleared every frame).
#include <game/ovl/shared_class1523.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t GIANT_CLANK = 2;  // (the hero moby's mode byte +0x24)

// the streaks' life (frames)
uint32_t streak_life() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x001601fc) : L15_ADDR(0x001601fc); }

float dt() { return lwc1(0x0015ed6c); }

// a light region's record (32 bytes: +0 / +4 its flicker, +0x10 its place, +0x1c its strength)
uint32_t region(int16_t r) { return OV(0x001802c0) + (static_cast<uint32_t>(static_cast<int32_t>(r)) << 5); }

// a colour of three random channels (red `r0`..`r1`, green, blue) and the alpha `a`
uint32_t rand_colour(uint32_t alpha) {
    const uint32_t red = static_cast<uint32_t>(rand_range(0x3f, 0x7f));
    const uint32_t green = static_cast<uint32_t>(rand_range(0x2f, 0x3f));
    const uint32_t blue = static_cast<uint32_t>(rand_range(0xf, 0x2f));
    return (red << 16) | ((green << 8) | alpha) | blue;
}

// the charge (frames charged over 30, at most 1)
float charge_of(uint32_t pv) {
    const float n = int_to_float(lw(pv + 0x10));
    const float c = fpu_div(n, frame_scale_f(30.0f));
    return 1.0f < c ? 1.0f : c;
}
}  // namespace

// orig: L15:0x0025e2c0 L18:0x00270af0
void class1523_streak(uint32_t colour, uint32_t from, uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t start = sp, step = sp + 0x10, end = sp + 0x20;
    lq_sq(start, from);
    const uint32_t p = alloc_particle(0x1f);
    if (p != 0) {
        vec3_sub(V(end), V(start), V(m + 0x10));
        lq_sq(step, end);
        vec3_scale(V(step), V(step), 0.25f);
        vec3_add(V(end), V(m + 0x10), V(step));
        lq_sq(p + 0x10, end);
        sw(p + 4, colour);
        sw(p + 0xc, colour & 0xffffffu);
        lq_sq(p + 0x20, start);
        sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x70));
        sb(p + 3, 0x44);
        sb(p + 1, 2);
        sw(p + 0x30, m);
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(lw(streak_life()))));
    }
    ee_stack_free(0xa0);
}

// State 0 charges while the button is held (+0x2e) as Giant Clank: streaks drawn in from around it (more the longer), the charge
// light's colour faded in (its blend register set) and its light region grown and flickering. Let go (or not Giant Clank) it is
// fired along his heading at 40 a second (state 1); state 1 flies (its light fading), hurting the mobys within its reach (3,
// growing as it flies), with a glow, sparks and puffs; it is gone after 60 frames, out of the level's box or when he is no
// longer Giant Clank.
// orig: L15:0x0029edb0 L18:0x002abaa0
void class1523_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st != 0 && st != 1) {
        sh(pv + 0x2e, 0);
        return;
    }
    const uint32_t f = ee_stack_alloc(0x110);
    const uint32_t dir = f, rnd = f + 0x10, at = f + 0x20;
    const uint32_t pos = m + 0x10;
    if (st == 0) {
        if (lh(pv + 0x2e) == 0 || lbu(0x001413f4) != GIANT_CLANK) {  // (fired)
            sw(OV(0x0015f320), 0);
            sw(OV(0x0015f330), 0);
            sw(pv + 0x10, 0);
            swc1(pv, fpu_mul(cos_vu(lwc1(HERO + 0x98)), fpu_mul(dt(), 40.0f)));
            const float s = sin_vu(lwc1(HERO + 0x98));
            sw(pv + 8, 0);
            swc1(pv + 4, fpu_mul(s, fpu_mul(dt(), 40.0f)));
            sb(m + 0x20, 1);
            sw(pv + 0x14, 0);
            sh(pv + 0x2e, 0);
            ee_stack_free(0x110);
            return;
        }
        int32_t charged = lw(pv + 0x10);
        if (charged == 0) {
            sw(OV(0x0015f330), 1);
            sw(OV(0x0015f334), 0xffff00);
            charged = lw(pv + 0x10);
        }
        sw(pv + 0x10, static_cast<uint32_t>(charged + 1));
        float charge = charge_of(pv);
        charge = fpu_mul(charge, charge);
        for (int32_t i = 0; i < float_to_int(fpu_mul(charge, 10.0f)); ++i) {  // (streaks from a sphere of 20 .. 40 about it)
            V(rnd) = Vec4{};
            swc1(rnd, rand_range_f(-1.0f, 1.0f));
            swc1(rnd + 4, rand_range_f(-1.0f, 1.0f));
            swc1(rnd + 8, rand_range_f(-1.0f, 1.0f));
            lq_sq(dir, rnd);
            int32_t green = rand_range(0x4f, 0xaf);
            int32_t red = rand_range(0x4f, 0xff);
            if (!(red < 256)) red = 0xff;
            if (!(green < 256)) green = 0xff;
            vec3_set_length(V(dir), V(dir), rand_range_f(20.0f, 40.0f));
            vec3_add_inplace(V(dir), V(pos));
            const uint32_t colour = (static_cast<uint32_t>(red) << 16) | ((static_cast<uint32_t>(green) << 8) | 0x7f000000u) | 0x4f;
            class1523_streak(colour, dir, m);
        }
        const uint32_t light = OV(0x0015f334);
        if (lw(pv + 0x10) < 50) {  // (the light's alpha up to 0x25, the blend register set)
            int32_t alpha = lbu(light + 3);
            if (ld(OV(0x0015f338)) == 0 && alpha == 0xff) alpha = 0x25;
            sd(OV(0x0015f338), (uint64_t{0xf800} << 21) | 0x42);
            alpha = alpha < 0x25 - frame_scale_i(1) ? alpha + frame_scale_i(1) : 0x25;
            sw(light, (lwu(light) & 0xffff00u) | (static_cast<uint32_t>(alpha) << 24));
        } else {
            sw(light, lwu(light) | 0xff000000u);
        }
        const int16_t r = static_cast<int16_t>(lh(pv + 0x2c));
        if (r == -1) {  // (a light region, every fourth frame)
            if ((lw(OV(0x0015f5cc)) & 3) == 0) sh(pv + 0x2c, static_cast<uint32_t>(sub_0023e5e0(pos, 0, 0.0f, 0.0f)));
        } else {
            const uint32_t e = region(r);
            swc1(e + 0x10, lwc1(pos));
            swc1(e + 0x14, lwc1(pos + 4));
            swc1(e + 0x18, lwc1(pos + 8));
            int32_t n = lw(pv + 0x10);
            if (!(n < 51)) n = 50;
            if (!(-1 < n)) n = 0;
            swc1(e + 0x1c, fpu_div(fpu_mul(int_to_float(n), 7.0f), 50.0f));
            const float glow = charge_of(pv);
            const uint32_t red = static_cast<uint32_t>(rand_range(0xf, 0x2f)) & 0xffu;
            const uint32_t green = static_cast<uint32_t>(rand_range(0x2f, 0x3f)) & 0xffu;
            const uint32_t blue = static_cast<uint32_t>(rand_range(0x3f, 0x7f)) & 0xffu;
            spawn_particle_05(e + 0x10, red, green, blue, frame_scale_i(0x14), fpu_mul(fpu_mul(glow, 4.0f), 210000.0f), 0.0f);
            const float strength = lwc1(e + 0x1c);
            swc1(e, rand_range_f(fpu_mul(lwc1(e), 0.8f), fpu_div(fpu_add(strength, strength), 7.0f)));
            float lo = fpu_mul(lwc1(e + 4), 0.8f);
            const float hi = lwc1(e);
            if (hi < lo) lo = hi;
            swc1(e + 4, rand_range_f(lo, hi));
        }
        sh(pv + 0x2e, 0);
        ee_stack_free(0x110);
        return;
    }

    // state 1: fired
    const int32_t flown = lw(pv + 0x14);
    const float t = int_to_float(flown);
    sw(pv + 0x14, static_cast<uint32_t>(flown + 1));
    const float reach = fpu_add(fpu_mul(fpu_mul(fpu_mul(dt(), 40.0f), t), 0.5f), 3.0f);
    const int16_t r = static_cast<int16_t>(lh(pv + 0x2c));
    if (r != -1) {  // (its light fading with the frame rate)
        const float fade = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbf19999au)), 1.0f);  // (-0.6)
        const uint32_t e = region(r);
        swc1(e + 0x1c, fpu_mul(lwc1(e + 0x1c), fade));
        if (lwc1(region(static_cast<int16_t>(lh(pv + 0x2c))) + 0x1c) < u2f(0x38d1b717u)) {  // (0.0001)
            region_clear(static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lh(pv + 0x2c)))));
            sh(pv + 0x2c, 0xffff);
        }
    }
    bool gone = frame_scale_i(0x3c) < lw(pv + 0x14) || lbu(0x001413f4) != GIANT_CLANK;
    if (!gone) {
        vec3_add_inplace(V(pos), V(pv));
        for (uint32_t k = 0; k < 3 && !gone; ++k) {
            const float c = lwc1(pos + k * 4);
            if (c < 2.0f || 1021.0f < c) gone = true;
        }
    }
    if (gone) {
        moby_retire(lwu(OV(0x001613c8)));
        sw(OV(0x001613c8), 0);
        ee_stack_free(0x110);
        return;
    }
    {
        const int32_t n = mobys_in_sphere(pos, reach, 0x10, m, 0);
        lq_sq(at, pos);
        moby_list_hit(m, at, OV(0x00177f00), n, 0, 0x8b0000, 2, 1, 20.0f, 1.0f, 1.0f);
    }
    const int32_t left = frame_scale_i(0x3c) - lw(pv + 0x14);
    const int32_t glow_life = left < frame_scale_i(5) ? frame_scale_i(0x3c) - lw(pv + 0x14) : frame_scale_i(5);
    const float twice = fpu_add(reach, reach);
    spawn_particle_1a(m, rand_colour(0x4f000000u), static_cast<uint32_t>(glow_life), 0xffffffffu, fpu_mul(fpu_mul(reach, 0.5f), 210000.0f));
    rand_dir_scaled(dir, fpu_mul(reach, dt()), fpu_mul(twice, dt()));
    {
        const uint32_t colour = rand_colour(0x7f000000u);
        spawn_particle_0f(pos, dir, colour, 0x4f7f4f2fu, static_cast<uint32_t>(frame_scale_i(0x2d)), 1, lbu(lwu(OV(0x001b20ac))), -1,
                          210000.0f);
    }
    for (int i = 0; i < 8; ++i) {  // (sparks thrown out from about it)
        rand_dir_scaled(dir, fpu_mul(reach, dt()), fpu_mul(twice, dt()));
        rand_dir_scaled(rnd, 0.0f, fpu_mul(reach, 0.5f));
        vec3_add_inplace(V(rnd), V(pos));
        const uint32_t p = spawn_particle_45(rnd, dir, 0x7f, m);
        if (p != 0) swc1(p + 0xc, fpu_mul(fpu_mul(rand_range_f(6000.0f, 32000.0f), reach), 0.4f));
    }
    for (int32_t i = 0; i < 12; ++i) {  // (still puffs at it, one now and then larger)
        const uint32_t p = spawn_particle_45(pos, OV(0x0015f580), 0x7f, m);
        if (p == 0) continue;
        if (i == 2 && rand_mod(8) == 0) swc1(p + 0xc, 180000.0f);
        else swc1(p + 0xc, rand_range_f(80000.0f, 120000.0f));
        swc1(p + 0xc, fpu_mul(lwc1(p + 0xc), reach));
        const int32_t frames = frame_scale_i(2);
        sh(p + 0xa, static_cast<uint32_t>(frames));
        swc1(p + 0x30, fpu_div(1.0f, int_to_float(static_cast<int16_t>(frames))));
        sh(p + 0x36, 3);
        sw(p + 0x38, 0x7f7f7f);
    }
    sh(pv + 0x2e, 0);
    ee_stack_free(0x110);
}

}  // namespace rac1
