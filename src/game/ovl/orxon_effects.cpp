// Orxon's own moby classes (level 10): the effect classes 1240 (a spray) and 1544 (smoke and steam vents along a path)
// with their emitters, and the draw callback 0x2da690 that class 1067's light panels queue. Header orxon_classes.h,
// registered in orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/orxon_classes.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/vu0_regs.h>
#include <game/port_stubs.h>
#include <game/level/level_write.h>

namespace rac1 {

using namespace orxon;

// Class 1240: a spray of pvars +0xc particles (particle 0x40) a frame from points scattered about it (pvars +0x10 sideways,
// +0x14 up and down), thrown along its heading at the fastest of three speeds in the level's range (L10:0x161efc ..
// 0x161ef8); L10:0x161f04 in 100 of them dark (slower, smaller); sizes pvars +4, lives pvars +8 seconds, colours two blends
// of the level's L10:0x161ef0 / 0x161ef4, falling by the level's L10:0x161f00. Its +0x30 0x80.
// orig: L10:0x002e5be0
void orx_class1240_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x30, 0x80);
    if (lw(pv + 0xc) <= 0) return;
    const uint32_t frame = ee_stack_alloc(0x30);
    const uint32_t pos = frame, vel = frame + 0x10, off = frame + 0x20;
    const float quarter = u2f(0x3fc90fdbu);
    for (int32_t i = 0; i < lw(pv + 0xc); ++i) {
        const bool dark = rand_mod(100) < lw(L10_ADDR(0x00161f04));
        lq_sq(pos, m + 0x10);
        const float c = cos_vu(angle_add(lwc1(m + 0x48), quarter));
        const float side = lwc1(pv + 0x10);
        swc1(off, fpu_mul(c, rand_range_f(-side, side)));
        const float s = sin_vu(angle_add(lwc1(m + 0x48), quarter));
        const float side2 = lwc1(pv + 0x10);
        const float y = fpu_mul(s, rand_range_f(-side2, side2));
        sw(off + 8, 0);
        swc1(off + 4, y);
        const float up = lwc1(pv + 0x14);
        swc1(off + 8, rand_range_f(-up, up));
        vec3_add(V(pos), V(pos), V(off));
        const float lo = lwc1(L10_ADDR(0x00161efc)), hi = lwc1(L10_ADDR(0x00161ef8));
        float speed = rand_range_f(lo, hi);
        const float s2 = rand_range_f(lwc1(L10_ADDR(0x00161efc)), lwc1(L10_ADDR(0x00161ef8)));
        const float s3 = rand_range_f(lwc1(L10_ADDR(0x00161efc)), lwc1(L10_ADDR(0x00161ef8)));
        if (speed < s2) speed = s2;
        if (speed < s3) speed = s3;
        vec_from_spherical_v(V(vel), fpu_mul(speed, dt()), lwc1(m + 0x48), -lwc1(m + 0x44));
        float slo = u2f(0x3f4ccccdu), shi = u2f(0x3f99999au);
        if (dark) {
            vec3_scale(V(vel), V(vel), rand_range_f(0.5f, 1.5f));
            slo = u2f(0x3e75c28fu);
            shi = u2f(0x3eb851ecu);
        }
        const float size = fpu_mul(lwc1(pv + 4), rand_range_f(slo, shi));
        const uint32_t c0 = rgba_lerp(lwu(L10_ADDR(0x00161ef0)), lwu(L10_ADDR(0x00161ef4)), rand_range_f(0.5f, 1.0f));
        const uint32_t clear = lwu(L10_ADDR(0x00161ef4)) & 0xff000000u;
        const uint32_t c1 = rgba_lerp(lwu(L10_ADDR(0x00161ef4)), clear, rand_range_f(0.25f, 0.5f));
        const int32_t life = float_to_int(frame_scale_f(fpu_mul(lwc1(pv + 8), 60.0f)));
        spawn_particle_40(pos, vel, static_cast<uint32_t>(life), c0, c1, dark ? 1u : 0u, size, lwc1(pv), fpu_mul(lwc1(L10_ADDR(0x00161f00)), dt2()));
    }
    ee_stack_free(0x30);
}

namespace {
// The vents' common puff (particle 0x02): from `at`, rising by `rise` (and `lift` a frame on its random spread of `spread`),
// its size, its spin, three counts scaled by one random factor about 1 and two colour blends; `k` the addresses of these
// in the level's table
struct Puff {
    uint32_t rise, spread, lift, size_lo, size_hi, spin_lo, spin_hi, n0, n1, n2, jitter, c0a, c0b, c1a, c1b;
};
void puff(uint32_t at, const Puff& k, uint32_t frame) {
    const uint32_t vel = frame + 0x20, end = frame + 0x30;
    vec_clear(V(vel));
    const float rise = fpu_mul(lwc1(k.rise), dt());
    const float spread = fpu_mul(lwc1(k.spread), dt());
    swc1(vel + 8, rise);
    rand_sphere_point(end, 0.0f, spread);
    swc1(end + 8, fpu_add(lwc1(end + 8), fpu_mul(lwc1(k.lift), dt())));
    swc1(vel + 0xc, rand_range_f(lwc1(k.size_lo), lwc1(k.size_hi)));
    swc1(end + 0xc, rand_range_f(lwc1(k.spin_lo), lwc1(k.spin_hi)));
    const float j = lwc1(k.jitter);
    const float f = rand_range_f(fpu_sub(1.0f, j), fpu_add(j, 1.0f));
    const int32_t n0 = float_to_int(fpu_mul(int_to_float(lw(k.n0)), f));
    const int32_t n1 = float_to_int(fpu_mul(int_to_float(lw(k.n1)), f));
    const int32_t n2 = float_to_int(fpu_mul(int_to_float(lw(k.n2)), f));
    const uint32_t c0 = rgba_lerp(lwu(k.c0a), lwu(k.c0b), rand_range_f(0.0f, 1.0f));
    const uint32_t c1 = rgba_lerp(lwu(k.c1a), lwu(k.c1b), rand_range_f(0.0f, 1.0f));
    spawn_particle_02(at, vel, end, c0, c1, static_cast<uint32_t>(n0), static_cast<uint32_t>(n1), static_cast<uint32_t>(n2), -1);
}

// a vent's on / off timer (s16 at `t`): counting up to 0 while off (then on for the level's [on_lo, on_hi] frames),
// down while on (then off for [off_lo, off_hi] frames, kept negative); true while it is on
bool vent_cycle(uint32_t t, uint32_t on_lo, uint32_t on_hi, uint32_t off_lo, uint32_t off_hi) {
    if (lh(t) < 0) {
        const uint32_t up = lhu(t) + 1u;
        sh(t, up);
        if ((up & 0xffffu) != 0) return false;
        sh(t, static_cast<uint32_t>(float_to_int(rand_range_f(int_to_float(lw(on_lo)), int_to_float(lw(on_hi))))));
        return true;
    }
    if (tick16(t) == 0) return true;
    sh(t, 0u - static_cast<uint32_t>(float_to_int(rand_range_f(int_to_float(lw(off_lo)), int_to_float(lw(off_hi))))));
    return false;
}

// the next puff's countdown (s16 at `t`) set to the level's [lo, hi] frames
void puff_wait(uint32_t t, uint32_t lo, uint32_t hi) {
    sh(t, static_cast<uint32_t>(float_to_int(rand_range_f(int_to_float(lw(lo)), int_to_float(lw(hi))))));
}

// a point within 32 of the camera and on screen (2 or 5 about it; off screen in 1 of 3 frames anyway)
bool vent_seen(uint32_t at, float dz, float w, uint32_t frame) {
    if (32.0f < vec3_distance(V(OV(0x00166dc0)), V(at))) return false;
    const uint32_t p = frame + 0x10;
    lq_sq(p, at);
    if (dz != 0.0f) swc1(p + 8, fpu_add(lwc1(p + 8), dz));
    swc1(p + 0xc, w);
    if (point_on_screen(p, 32.0f) == -1 && rand_mod(3) != 0) return false;
    return true;
}
}  // namespace

// A smoke vent at `at`: on / off by the timer `cycle` (the level's L10:0x161fac .. 0x161fb8), a puff (the level's table
// L10:0x161f68 ..) whenever the puff countdown `wait` runs out while it is on and seen.
// orig: L10:0x002ea4e8
void orx_2ea4e8_l10(uint32_t at, uint32_t wait, uint32_t cycle) {
    if (!vent_cycle(cycle, L10_ADDR(0x00161fac), L10_ADDR(0x00161fb0), L10_ADDR(0x00161fb4), L10_ADDR(0x00161fb8))) return;
    const uint32_t frame = ee_stack_alloc(0x40);
    if (vent_seen(at, 0.0f, 2.0f, frame) && tick16(wait) != 0) {
        puff(at, Puff{L10_ADDR(0x00161f68), L10_ADDR(0x00161f70), L10_ADDR(0x00161f6c), L10_ADDR(0x00161f74), L10_ADDR(0x00161f78), L10_ADDR(0x00161f7c), L10_ADDR(0x00161f80), L10_ADDR(0x00161f84), L10_ADDR(0x00161f88), L10_ADDR(0x00161f8c),
                      L10_ADDR(0x00161f90), L10_ADDR(0x00161f94), L10_ADDR(0x00161f98), L10_ADDR(0x00161f9c), L10_ADDR(0x00161fa0)},
             frame);
        puff_wait(wait, L10_ADDR(0x00161fa4), L10_ADDR(0x00161fa8));
    }
    ee_stack_free(0x40);
}

// A steam vent at `at` (always on): a puff (the level's table L10:0x161fbc ..) from 6 above it whenever the countdown `wait`
// runs out while it is seen.
// orig: L10:0x002ea7b0
void orx_2ea7b0_l10(uint32_t at, uint32_t wait) {
    const uint32_t frame = ee_stack_alloc(0x40);
    if (vent_seen(at, 6.0f, 5.0f, frame) && tick16(wait) != 0) {
        puff(at, Puff{L10_ADDR(0x00161fbc), L10_ADDR(0x00161fc4), L10_ADDR(0x00161fc0), L10_ADDR(0x00161fc8), L10_ADDR(0x00161fcc), L10_ADDR(0x00161fd0), L10_ADDR(0x00161fd4), L10_ADDR(0x00161fd8), L10_ADDR(0x00161fdc), L10_ADDR(0x00161fe0),
                      L10_ADDR(0x00161fe4), L10_ADDR(0x00161fe8), L10_ADDR(0x00161fec), L10_ADDR(0x00161ff0), L10_ADDR(0x00161ff4)},
             frame);
        puff_wait(wait, L10_ADDR(0x00161ff8), L10_ADDR(0x00161ffc));
    }
    ee_stack_free(0x40);
}

// A drip vent at `at`, `height` above its floor: on / off by `cycle` (L10:0x16203c .. 0x162048); while on and its point 4
// below on screen, whenever `wait` runs out a drop (particle 0x02, the level's table L10:0x162000 ..) falling by the level's
// gravity, its second count the frames it takes to fall `height`, its texture L10:0x16204c.
// orig: L10:0x002eaa08
void orx_2eaa08_l10(uint32_t at, uint32_t wait, uint32_t cycle, float height) {
    if (!vent_cycle(cycle, L10_ADDR(0x0016203c), L10_ADDR(0x00162040), L10_ADDR(0x00162044), L10_ADDR(0x00162048))) return;
    const uint32_t frame = ee_stack_alloc(0x40);
    const uint32_t p = frame + 0x10;
    lq_sq(p, at);
    swc1(p + 8, fpu_sub(lwc1(p + 8), 4.0f));
    swc1(p + 0xc, 3.0f);
    if (point_on_screen(p, 32.0f) != -1 && tick16(wait) != 0) {
        const float j = lwc1(L10_ADDR(0x00162020));
        const float f = rand_range_f(fpu_sub(1.0f, j), fpu_add(j, 1.0f));
        const int32_t n0 = float_to_int(fpu_mul(int_to_float(lw(L10_ADDR(0x00162018))), f));
        const int32_t n2 = float_to_int(fpu_mul(int_to_float(lw(L10_ADDR(0x0016201c))), f));
        const float g = fpu_mul(lwc1(L10_ADDR(0x00162000)), dt2());
        const float fall = fpu_mul(fpu_mul(sqrt_f(fpu_div(fpu_add(height, height), g)), lwc1(L10_ADDR(0x00162050))), f);
        const int32_t n1 = float_to_int(fall);
        const uint32_t vel = frame + 0x20, end = frame + 0x30;
        vec_clear(V(vel));
        rand_sphere_point(end, 0.0f, fpu_mul(lwc1(L10_ADDR(0x00162004)), dt()));
        swc1(end + 8, fpu_sub(lwc1(end + 8), fpu_mul(fpu_mul(lwc1(L10_ADDR(0x00162000)), dt2()), int_to_float(n1))));
        swc1(vel + 0xc, rand_range_f(lwc1(L10_ADDR(0x00162008)), lwc1(L10_ADDR(0x0016200c))));
        swc1(end + 0xc, rand_range_f(lwc1(L10_ADDR(0x00162010)), lwc1(L10_ADDR(0x00162014))));
        const uint32_t c0 = rgba_lerp(lwu(L10_ADDR(0x00162024)), lwu(L10_ADDR(0x00162028)), rand_range_f(0.0f, 1.0f));
        const uint32_t c1 = rgba_lerp(lwu(L10_ADDR(0x0016202c)), lwu(L10_ADDR(0x00162030)), rand_range_f(0.0f, 1.0f));
        spawn_particle_02(at, vel, end, c0, c1, static_cast<uint32_t>(n0), static_cast<uint32_t>(n1), static_cast<uint32_t>(n2),
                          lw(L10_ADDR(0x0016204c)));
        puff_wait(wait, L10_ADDR(0x00162034), L10_ADDR(0x00162038));
    }
    ee_stack_free(0x40);
}

// Class 1544: vents at its place (pvars +4 = -1) or at each of the first 32 points of path +4. 0 starts it by its mode
// (pvars +0): 0 smoke (1), 1 drips (2; the floor under each point noted in its w, or under it in +0x88), 2 steam (3).
// Its timers: pvars +8 (the puffs) and +0x48 (the on / off cycles), two bytes a point.
// orig: L10:0x002ea1f0
void orx_class1544_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t p = lw(pv + 4);
    auto points = [&](uint32_t& first) {  // the path's first point and how many (at most 32)
        const uint32_t path = lvd::path(p);
        first = path + 0x10;
        int32_t n = lw(path);
        if (!(n < 33)) n = 0x20;
        return n;
    };
    switch (lbu(m + 0x20)) {
    case 0:
        switch (lw(pv)) {
        case 0:
            sb(m + 0x20, 1);
            break;
        case 1:
            if (p == -1) {
                swc1(pv + 0x88, floor_probe_v(0.5f, V(m + 0x10), 0));
            } else {
                uint32_t pt;
                const int32_t n = points(pt);
                for (int32_t i = 0; i < n; ++i) lvd::set_path_f32(pt + static_cast<uint32_t>(i) * 0x10 + 0xc, floor_probe_v(0.5f, V(pt + static_cast<uint32_t>(i) * 0x10), 0));
            }
            sb(m + 0x20, 2);
            break;
        case 2:
            sb(m + 0x20, 3);
            break;
        default:
            break;
        }
        break;
    case 1:
        if (p == -1) {
            orx_2ea4e8_l10(m + 0x10, pv + 8, pv + 0x48);
        } else {
            uint32_t pt;
            const int32_t n = points(pt);
            for (int32_t i = 0; i < n; ++i)
                orx_2ea4e8_l10(pt + static_cast<uint32_t>(i) * 0x10, pv + 8 + static_cast<uint32_t>(i) * 2, pv + 0x48 + static_cast<uint32_t>(i) * 2);
        }
        break;
    case 2:
        if (p == -1) {
            orx_2eaa08_l10(m + 0x10, pv + 8, pv + 0x48, lwc1(pv + 0x88));
        } else {
            uint32_t pt;
            const int32_t n = points(pt);
            for (int32_t i = 0; i < n; ++i) {
                const uint32_t at = pt + static_cast<uint32_t>(i) * 0x10;
                orx_2eaa08_l10(at, pv + 8 + static_cast<uint32_t>(i) * 2, pv + 0x48 + static_cast<uint32_t>(i) * 2, lwc1(at + 0xc));
            }
        }
        break;
    case 3:
        if (p == -1) {
            orx_2ea7b0_l10(m + 0x10, pv + 8);
        } else {
            uint32_t pt;
            const int32_t n = points(pt);
            for (int32_t i = 0; i < n; ++i) orx_2ea7b0_l10(pt + static_cast<uint32_t>(i) * 0x10, pv + 8 + static_cast<uint32_t>(i) * 2);
        }
        break;
    default:
        break;
    }
}

namespace {
// The scale matrix diag(s, s, s, 1) at `out` through VU0's registers, as the level's copy of the engine routine leaves
// them: vf1 .. vf3 vf0 times its x (0) then s in their own lane, vf4 a copy of vf0, vf5 the scale's GPR word as qmtc2
// moves it (its bits sign-extended to 64; the upper half 0, as the transliteration had it).
// orig: L10:0x001f3080
void scale_rows_vu(uint32_t out, float s) {
    using namespace vu0m;
    const uint32_t b = f2u(s);
    eer::qmtc2(5, eer::mkw(b, (b & 0x80000000u) != 0 ? 0xffffffffu : 0u, 0u, 0u));
    eer::vmacro();
    mulbc(vf()[1], vf()[0], vf()[0].v[0], XYZW);
    eer::vmacro();
    mulbc(vf()[2], vf()[0], vf()[0].v[0], XYZW);
    eer::vmacro();
    mulbc(vf()[3], vf()[0], vf()[0].v[0], XYZW);
    eer::vmacro();
    vf()[4] = vf()[0];
    eer::vmacro();
    addbc(vf()[1], vf()[1], vf()[5].v[0], X);
    eer::vmacro();
    addbc(vf()[2], vf()[2], vf()[5].v[0], Y);
    eer::vmacro();
    addbc(vf()[3], vf()[3], vf()[5].v[0], Z);
    for (int r = 1; r <= 4; ++r) eer::sqc2_(out + static_cast<uint32_t>(r - 1) * 0x10, r);
}
}  // namespace

// Class 1067's draw callback (a0 = the panel): pvars +4 light strips, each a copy of the panel's place and angles scaled by
// half its pvars +0 and raised by pvars +8 per strip, drawn as 4 rows x 19 columns of quads (quad_draw, effect texture 0xe)
// over the level's grid (L10:0x1da010) at the moby of pvars +0xc's pvars (+0x10000: the cells' values +0x2c00 by row and
// strip, +0x2de0 the texture's scroll), each quad's colour a blend of the level's pair L10:0x161c90 / 0x161c94 by its
// cell's value about the level's thresholds L10:0x161d64 / 0x161d68 (the panel switched off, state 2: white fading out
// over L10:0x161c98 seconds), faded beyond 48 of the camera; the texture scrolled along the columns by L10:0x161d60.
// orig: L10:0x002da690
void orx_cb_2da690_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t strips = lw(pv + 4);
    const uint32_t f = ee_stack_alloc(0x1b0);
    const uint32_t mats_size = (static_cast<uint32_t>(strips) << 9) >> 3;
    const uint32_t mats = ee_stack_alloc(mats_size);
    const uint32_t other = lwu(moby_at(lw(pv + 0xc)) + 0x78);
    const float d = vec3_distance(V(m + 0x10), V(OV(0x00166dc0)));
    const float fade = 48.0f < d ? fpu_mul(fpu_sub(80.0f, d), 0.03125f) : 1.0f;
    const uint32_t scale = f + 0x120;
    scale_rows_vu(scale, fpu_mul(lwc1(pv), 0.5f));
    for (int32_t i = 0; i < lw(pv + 4); ++i) {
        const uint32_t mt = mats + static_cast<uint32_t>(i) * 0x40;
        euler_to_mat4_vu(M(mt), V(m + 0x40));
        mat4_mul(M(mt), M(mt), M(scale));
        lq_sq(mt + 0x30, m + 0x10);
        swc1(mt + 0x38, fpu_add(lwc1(mt + 0x38), fpu_mul(int_to_float(i + 1), lwc1(pv + 8))));
    }
    const uint64_t tex0 = effect_tex0(0xe);
    auto sx = [](uint32_t a) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lwu(a)))); };
    const uint64_t alpha = sx(L10_ADDR(0x00161c70)) | (sx(L10_ADDR(0x00161c74)) << 2) | (sx(L10_ADDR(0x00161c78)) << 4) |
                           (sx(L10_ADDR(0x00161c7c)) << 6) | (uint64_t{0x8000} << 24);
    const uint64_t tex1 = (uint64_t{0xff90} << 32) | 0x260u;
    sd(f + 0x78, tex0);
    sd(f + 0x108, tex0);
    sd(f + 0x80, tex1);
    sd(f + 0x88, alpha);
    sd(f + 0x110, tex1);
    sd(f + 0x100, 0);
    sd(f + 0x70, 0);
    sd(f + 0x118, alpha);
    for (uint32_t k = 0; k < 4; ++k) {  // the corners' t (the grid's second column)
        const uint32_t t = lwu(L10_ADDR(0x001d9ff4) + k * 8);
        sw(f + 0xe4 + k * 8, t);
        sw(f + 0x54 + k * 8, t);
    }
    const uint32_t cells = other + 0x10000;
    for (int32_t row = 0; row < 4; ++row) {
        float scroll = 0.0f;
        for (int32_t col = 0; col < 19; ++col) {
            scroll = fpu_add(scroll, lwc1(L10_ADDR(0x00161d60)));
            if (1.0f < scroll) scroll = fpu_sub(scroll, 1.0f);
            for (uint32_t k = 0; k < 4; ++k) {  // the corners' s, scrolled
                const float s = fpu_add(fpu_add(lwc1(L10_ADDR(0x001d9ff0) + k * 8), scroll), lwc1(cells + 0x2de0));
                swc1(f + 0xe0 + k * 8, s);
                swc1(f + 0x50 + k * 8, s);
            }
            for (int32_t i = 0; i < lw(pv + 4); ++i) {
                const uint32_t lamp = lbu(pv + 0x10 + static_cast<uint32_t>(i));
                const int32_t v = lw(cells + 0x2c00 + (lamp << 2) + static_cast<uint32_t>(row * 0x78));
                const int32_t lo = lw(L10_ADDR(0x00161d68));
                float t;
                if (lo < v) {
                    t = fpu_div(int_to_float(v - lo), int_to_float(lw(L10_ADDR(0x00161d64)) - lw(L10_ADDR(0x00161d68))));
                } else {
                    t = fpu_sub(1.0f, fpu_div(int_to_float(v), int_to_float(lw(L10_ADDR(0x00161d68)))));
                }
                const uint32_t c = rgba_lerp(lwu(L10_ADDR(0x00161c90)), lwu(L10_ADDR(0x00161c94)), t);
                const int32_t a = float_to_int(fpu_mul(fpu_mul(fade, fade), int_to_float(static_cast<int32_t>(c) >> 24)));
                uint32_t colour = (c & 0xffffffu) + (static_cast<uint32_t>(a) << 24);
                if (lbu(m + 0x20) == 2) {  // switched off: white, fading out
                    const float k = fpu_div(fpu_mul(int_to_float(lw(pv + 0x34)), 255.0f), frame_scale_f(fpu_mul(lwc1(L10_ADDR(0x00161c98)), 60.0f)));
                    colour = (static_cast<uint32_t>(float_to_int(k)) << 24) | 0xffffffu;
                }
                sw(f + 0xd0, colour);
                sw(f + 0xdc, colour);
                sw(f + 0xd8, colour);
                sw(f + 0xd4, colour);
                const uint32_t base = other + static_cast<uint32_t>(row * 0x2580);
                for (uint32_t j = 0; j < 4; ++j) {  // the quad's corners: the grid's points at this cell
                    const uint32_t cell = base + static_cast<uint32_t>((col + static_cast<int32_t>(j >> 1)) * 0x140) + (lamp << 4);
                    vec3_add(V(f + 0x90 + j * 0x10), V(L10_ADDR(0x001da010) + j * 0x10), V(cell));
                }
                quad_draw(f + 0x90, mats + (static_cast<uint32_t>(i) << 6));
            }
        }
    }
    ee_stack_free(mats_size);
    ee_stack_free(0x1b0);
}

}  // namespace rac1
