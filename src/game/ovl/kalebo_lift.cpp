// Kalebo III (level 16), class 1826: a lift going up and down a cylinder of the level (pvars +0xa0: its axis the
// cylinder's local z, its two stops 1 below and 1 above the centre, plus the level's 0x161eac). State 0 puts it at the
// upper stop (+0xbc 1: the next is the lower) and waits (2); waiting, its sound stops, and it sets off (3) to the other
// stop when Ratchet stands on it (+0x2fc, his halfword +0x30e clear) or, within 20, is level with the other stop (he
// calls it). Moving (3), seek_1d brings its height (+0x18) to the stop (pvars +0xa8; its speed +0xb0), slower and held
// where it is while Ratchet rides it down; its sound 4 (the slot pvars +0xb4) plays while it moves; at the stop, with
// Ratchet off it, it waits again (2); while its speed is not 0 its rings (0x2e84e8) are queued. Every frame its record
// pvars +0x60 carries what stands on it by its move. Its draw: rings of quads round the cylinder's axis from its bottom
// to its top (pvars +0xb8 the rise, +0xbc the spacing), turning with the frame counter, each one the other way, their
// colour from the level's two by its speed. Level 16 only: `orig: L16:`, level data L16_ADDR, base ELF bare. Registered
// in kalebo_mobys.cpp; checked by batch 250 (class1826, cb2e84e8, the P16.6 checks).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

#include <game/ovl/variants_l16.h>

#include <cstdint>

namespace rac1 {

namespace {
using kalebo::Frame;

constexpr uint32_t HERO = 0x0013f350u;
constexpr uint32_t HERO_POS = 0x0013f3d0u;

float dt() { return lwc1(0x0015ed6cu); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70u); }  // its square

uint32_t lift_cylinder(uint32_t pv) { return lvd::box(lvd::CYLINDERS, lw(pv + 0xa0)); }

// Ratchet stands on the lift `m` (his moby under him, his halfword +0x30e clear)
bool hero_aboard(uint32_t m) { return lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0; }

// its sound (the slot pvars +0xb4) stopped if it plays
void lift_sound_stop(uint32_t m, uint32_t pv) {
    if (sub_0028d8c0(m, lw(pv + 0xb4)) != 0) {
        sub_0028d918(lw(pv + 0xb4));
        sw(pv + 0xb4, 0xffffffffu);
    }
}
}  // namespace

// orig: L16:0x002e7f80
void kal_class1826_update_l16(uint32_t m) {
    const Frame f(0xd0);
    const uint32_t move = f.sp, turn_was = f.sp + 0x10, p = f.sp + 0x20;
    vec3_scale(V(move), V(m + 0x10), -1.0f);
    lq_sq(turn_was, m + 0x40);
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t rec = pv + 0x60, turn = m + 0x40;
    // the stop on the cylinder's axis at local z +1 / -1 (+0xbc set: -1), as a height
    auto stop_height = [&](bool down) {
        vec_clear(V(p));
        swc1(p + 8, down ? -1.0f : 1.0f);
        mat4_transform(V(p), V(p), M(lift_cylinder(pv)));
        return fpu_add(fpu_add(lwc1(p + 8), lwc1(lift_cylinder(pv) + 0x38)), lwc1(L16_ADDR(0x00161eac)));
    };
    switch (lbu(m + 0x20)) {  // (the original's compare chain)
    case 0: {  // (0x002e8020)
        for (uint32_t k = 0; k < 0x10; k += 4) sw(p + k, 0);
        swc1(p + 8, 1.0f);
        sw(pv + 0xb4, 0xffffffffu);
        mat3_transform(V(p), V(p), M(lift_cylinder(pv)));
        swc1(pv + 0xa8, fpu_add(lwc1(p + 8), lwc1(lift_cylinder(pv) + 0x38)));
        sb(m + 0xbc, 1);
        sb(m + 0x20, 2);
        swc1(m + 0x18, fpu_add(fpu_add(lwc1(p + 8), lwc1(lift_cylinder(pv) + 0x38)), lwc1(L16_ADDR(0x00161eac))));
        break;
    }
    case 2: {  // (0x002e80ac)
        lift_sound_stop(m, pv);
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 20.0f) {
            const float other = stop_height(lbu(m + 0xbc) != 0);
            swc1(p + 8, other);
            if (abs_f(fpu_sub(lwc1(HERO_POS + 8), other)) < 1.0f) {  // (called from the other stop)
                sb(m + 0xbc, (lbu(m + 0xbc) + 1u) & 1u);
                swc1(pv + 0xa8, lwc1(p + 8));
                sw(pv + 0xb0, 0);
                sb(m + 0x20, 3);
                break;
            }
        }
        if (!hero_aboard(m)) break;
        const bool down = lbu(m + 0xbc) != 0;
        sb(m + 0xbc, (lbu(m + 0xbc) + 1u) & 1u);
        const float other = stop_height(down);
        sw(pv + 0xb0, 0);
        swc1(pv + 0xa8, other);
        sb(m + 0x20, 3);
        break;
    }
    case 3: {  // (0x002e8290)
        const float rate = lwc1(L16_ADDR(0x00161ebc));
        float target = lwc1(pv + 0xa8), accel = fpu_mul(rate, dt2());
        if (!(2.25f < vec2_distance(V(HERO_POS), V(lift_cylinder(pv) + 0x30))) &&
            !(1.0f < abs_f(fpu_sub(fpu_sub(lwc1(m + 0x18), lwc1(HERO_POS + 8)), 2.0f))) && !(lwc1(m + 0x18) < lwc1(pv + 0xa8))) {
            target = lwc1(m + 0x18);  // (Ratchet riding it down: held where it is, 4 times the rate)
            accel = fpu_mul(fpu_mul(rate, 4.0f), dt2());
        }
        const float step = seek_1d(m + 0x18, pv + 0xb0, target, accel, accel, fpu_mul(lwc1(L16_ADDR(0x00161eb8)), dt()));
        vec_clear(V(p));
        swc1(p + 8, step);
        attach_frame_set(rec, p, turn, turn);
        if (lwc1(p + 8) == 0.0f) lift_sound_stop(m, pv);
        else if (sub_0028d8c0(m, lw(pv + 0xb4)) == 0) sw(pv + 0xb4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        if (lwc1(m + 0x18) == lwc1(pv + 0xa8) && !hero_aboard(m)) {  // (at the stop, Ratchet off it)
            vec_clear(V(pv + 0x70));
            sb(m + 0x20, 2);
            break;
        }
        if (lwc1(pv + 0xb0) != 0.0f) sub_001f93e0(L16_ADDR(0x002e84e8), m);  // (its rings' draw)
        break;
    }
    default:  // (0x002e8490)
        break;
    }
    vec3_add(V(move), V(move), V(m + 0x10));
    attach_frame_set(rec, move, turn_was, turn);
}

// The lift's rings: quads of the effect texture 0xe (corners on two circles of radius 0x161ec4 less 0.05 and 0x161ec4 at
// heights -/+ 0x161ec8, 0x161ee8 degrees apart; (s, t) from 0x1d9880; the blend from 0x161ecc), drawn through the lift's
// matrix with its place's height replaced: rings from the cylinder's bottom to its top (+ pvars +0xb8), pvars +0xbc
// apart, each one a full turn of quads, turned by the frame counter (120 a turn, 1.5 degrees a frame), every other ring
// the other way.
// orig: L16:0x002e84e8
void kal_cb_2e84e8_l16(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cyl = lift_cylinder(pv);
    const float half = lwc1(cyl + 0x28), mid = lwc1(cyl + 0x38), rise = lwc1(pv + 0xb8);
    const float top = fpu_add(fpu_add(mid, half), rise);
    const float bottom = fpu_add(fpu_sub(mid, half), rise);
    const float fast = fpu_div(abs_f(lwc1(pv + 0xb0)), fpu_mul(lwc1(L16_ADDR(0x00161eb8)), dt()));
    const uint32_t colour = rgba_lerp(lwu(L16_ADDR(0x00161edc)), lwu(L16_ADDR(0x00161ee0)), fast);
    const Frame f(0x1a0);
    const uint32_t q = f.sp, mat = f.sp + 0x90;  // (quad_draw's descriptor, 0x90 bytes, and its matrix, 0x40)
    mat3_copy_affine(M(mat), M(m + 0xc0));
    lq_sq(mat + 0x30, m + 0x10);
    swc1(mat + 0x38, bottom);
    swc1(mat + 0x3c, 1.0f);
    sd(q + 0x78, effect_tex0(0xe));
    sd(q + 0x80, 0xff9000000260ull);
    sd(q + 0x88, kalebo::quad_blend(L16_ADDR(0x00161ecc)));
    sd(q + 0x70, 0);
    const float deg = u2f(0x3c8efa35u);
    const uint32_t st = L16_ADDR(0x001d9880);
    for (uint32_t k = 0; k < 4; ++k) {  // (the corners: even ones on the inner circle, below)
        const uint32_t c = q + k * 0x10;
        swc1(q + 0x50 + k * 8, lwc1(st + k * 8));
        swc1(q + 0x54 + k * 8, lwc1(st + k * 8 + 4));
        float r;
        if ((k & 1u) == 0) {
            swc1(c + 8, -lwc1(L16_ADDR(0x00161ec8)));
            r = fpu_sub(lwc1(L16_ADDR(0x00161ec4)), 0.05f);
        } else {
            swc1(c + 8, lwc1(L16_ADDR(0x00161ec8)));
            r = lwc1(L16_ADDR(0x00161ec4));
        }
        const float side = static_cast<float>(static_cast<int32_t>(k >> 1));
        swc1(c + 0, fpu_mul(cos_vu(fpu_mul(side, fpu_mul(lwc1(L16_ADDR(0x00161ee8)), deg))), r));
        swc1(c + 0xc, 1.0f);
        swc1(c + 4, fpu_mul(sin_vu(fpu_mul(side, fpu_mul(lwc1(L16_ADDR(0x00161ee8)), deg))), r));
    }
    if (!(lwc1(mat + 0x38) < top)) return;
    int32_t way = 1;
    for (;;) {
        const int32_t phase = ee_rem(lw(L16_ADDR(0x0015f5cc)), 120);
        way = -way;
        const float spin_deg = fpu_mul(static_cast<float>(phase), u2f(0x3cd67744u));  // (1.5 degrees a frame)
        float i = 0.0f;
        if (i < fpu_div(360.0f, lwc1(L16_ADDR(0x00161ee8)))) {
            const float spin = fpu_mul(spin_deg, static_cast<float>(way));
            const float quarter = u2f(0x3fc90fdbu);
            int32_t n = 0;
            for (;;) {
                ++n;
                const float a = fpu_mul(fpu_mul(lwc1(L16_ADDR(0x00161ee8)), deg), i);
                swc1(mat + 0, cos_vu(angle_wrap(fpu_add(a, spin))));
                swc1(mat + 4, sin_vu(angle_wrap(fpu_add(fpu_mul(fpu_mul(lwc1(L16_ADDR(0x00161ee8)), deg), i), spin))));
                sw(mat + 8, 0);
                swc1(mat + 0x10, cos_vu(angle_wrap(fpu_add(fpu_add(fpu_mul(fpu_mul(lwc1(L16_ADDR(0x00161ee8)), deg), i), quarter), spin))));
                const float b = fpu_mul(fpu_mul(lwc1(L16_ADDR(0x00161ee8)), deg), i);
                i = static_cast<float>(n);
                swc1(mat + 0x14, sin_vu(angle_wrap(fpu_add(fpu_add(b, quarter), spin))));
                swc1(mat + 0x28, 1.0f);
                sw(mat + 0x18, 0);
                for (uint32_t k = 0; k < 0x10; k += 4) sw(q + 0x40 + k, colour);
                quad_draw(q, mat);
                if (!(i < fpu_div(360.0f, lwc1(L16_ADDR(0x00161ee8))))) break;
            }
        }
        const float next = fpu_add(lwc1(mat + 0x38), lwc1(pv + 0xbc));
        swc1(mat + 0x38, next);
        if (!(next < top)) break;
    }
}

}  // namespace rac1
