// Novalis's own moby classes, part 2d: the path follower library the flying classes use (the jets and
// the ship): a "follower" struct {+0 the current index, +4 a byte (negative: an open path with a single point),
// +0x10 the path (count at +0, the points 0x10 apart from +0x10, each with its segment length at +0x1c), +0x14 the
// path's number in the table 0x1b04b0}. Header novalis_mobys2.h. Addresses are
// Novalis's (`orig: L1:`); the globals Novalis shares with Veldin are the level's own (OV), the base ELF's are bare.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// A follower started: its +5 and index +8 cleared, its path (the table 0x1b04b0 / Novalis 0x1b0930 at its +0x14)
// found, and each point's segment length (the distance to the next point, wrapping) stored at its +0x1c.
// (Shared: the same code on Kerwan, Eudora, Hoven and Oltanis.)
// orig: L1:0x0028b410 L3:0x00264458 L4:0x00268b18 L12:0x0028eb00 L14:0x0027df78
void path_follower_init_l1(uint32_t f) {
    sb(f + 5, 0);
    const uint32_t path0 = lwu(OV(0x001b04b0) + (lwu(f + 0x14) << 2));
    sw(f + 8, 0);
    sw(f + 0x10, path0);
    if (!(lw(path0) > 0)) return;
    const float half = u2f(0x3f000000u);
    uint32_t v1 = lwu(f + 0x10);
    for (int32_t a1 = 0;;) {
        const int32_t s0 = a1 + 1;
        const uint32_t s1 = static_cast<uint32_t>(a1) << 4;
        const int32_t next = ee_rem(s0, lw(v1));
        const float d = vec3_distance(V(v1 + s1 + 0x10), V(v1 + (static_cast<uint32_t>(next) << 4) + 0x10));
        swc1(lwu(f + 0x10) + s1 + 0x1c, d);
        swc1(lwu(f + 0x10) + s1 + 0x18, lwc1(lwu(f + 0x10) + s1 + 0x18) + half);
        swc1(lwu(f + 0x10) + s1 + 0x18, lwc1(lwu(f + 0x10) + s1 + 0x18) - half);
        a1 = s0;
        v1 = lwu(f + 0x10);
        if (!(a1 < lw(v1))) break;
    }
}

// The index of the point of path `path` whose distance to `at` is nearest to `bias` (the comparison against the best
// kept as the distance itself).
// (Shared: the same code on Kerwan, Eudora, Rilgar, Umbris, Batalia, Gaspar, Hoven, Gemlik, Oltanis and Kalebo III; the
// hoverboard race's (hero_hoverboard.cpp) on Rilgar and Kalebo III.)
// orig: L1:0x0028b510 L3:0x00264558 L4:0x00268c18 L5:0x002a0260 L7:0x0029e610 L8:0x002806a8 L9:0x00294a10 L12:0x0028ec00 L13:0x00281d70 L14:0x0027e078 L16:0x00271250
int32_t path_nearest_l1(uint32_t at, uint32_t path, float bias) {
    int32_t best = 0;
    float f21 = u2f(0x51ba43b7u);
    int32_t n = lw(path);
    if (n > 0) {
        uint32_t s0 = path + 0x10;
        for (int32_t i = 0;;) {
            const float d = vec3_distance(V(s0), V(at));
            if (abs_f(d - bias) < f21) {
                f21 = d;
                best = i;
            }
            n = lw(path);
            ++i;
            s0 += 0x10;
            if (!(i < n)) break;
        }
    }
    return best;
}

// The address of point `i` of the follower's path: i < 0 counts from the end (past the last point's +0x20 offset);
// beyond the last wraps around the count; a follower with the byte +4 negative has the single point 0.
// orig: L1:0x0028b7b0
uint32_t path_point_l1(uint32_t f, int32_t i) {
    uint32_t a0;
    int32_t v1;
    if (static_cast<int8_t>(lbu(f + 4)) < 0) {
        a0 = lwu(f + 0x10);
        v1 = 0;
    } else {
        a0 = lwu(f + 0x10);
        v1 = lw(a0) - 1;
    }
    if (i < 0) return a0 + (static_cast<uint32_t>(v1 + i) << 4) + 0x20;
    if (!(v1 < i)) return a0 + (static_cast<uint32_t>(i) << 4) + 0x10;
    v1 += 1;
    return a0 + (static_cast<uint32_t>(ee_rem(i, v1)) << 4) + 0x10;
}

// Index `i` wrapped into the follower's path (negative: counted back from the last index; past it, modulo the count).
// orig: L1:0x0028b828
int32_t path_index_wrap_l1(uint32_t f, int32_t i) {
    int32_t v1 = 0;
    if (!(static_cast<int8_t>(lbu(f + 4)) < 0)) v1 = lw(lwu(f + 0x10)) - 1;
    if (i < 0) return v1 + i;
    if (!(v1 < i)) return i;
    return ee_rem(i, v1 + 1);
}

// The follower's index (+0) advanced by one, wrapped modulo the count; stored and returned.
// orig: L1:0x0028b878
int32_t path_index_advance_l1(uint32_t f) {
    int32_t a2 = 0;
    if (!(static_cast<int8_t>(lbu(f + 4)) < 0)) a2 = lw(lwu(f + 0x10)) - 1;
    int32_t a1 = lw(f) + 1;
    if (a2 < a1) a1 = ee_rem(a1, a2 + 1);
    sw(f, static_cast<uint32_t>(a1));
    return a1;
}

// The tangent at `idx` of the follower's path: the points idx - 1 / idx / idx + 1 (from the follower's index +0),
// the two differences scaled by (1 - a)(1 - b) / 2 and (1 - a)(1 + b) / 2, summed, and limited to the shorter of the
// two segments * 1.1 (written to `out`; w untouched by the scaled math as the vector ports do).
// orig: L1:0x0028b8c8
void path_spline_tangent_l1(uint32_t out, uint32_t f, int32_t idx, float a, float b) {
    const uint32_t buf = ee_stack_alloc(0x40);
    libc_memset(buf, 0, 0xc);
    sw(buf, path_point_l1(f, lw(f) + idx - 1));
    sw(buf + 4, path_point_l1(f, lw(f) + idx));
    sw(buf + 8, path_point_l1(f, lw(f) + idx + 1));
    const float one = u2f(0x3f800000u), half = u2f(0x3f000000u);
    float f1 = one - b;
    float fa = one - a;
    float fb = b + one;
    f1 = fa * f1;
    fa = fa * fb;
    const float f23 = f1 * half;
    const float f22 = fa * half;
    const float d1 = vec3_distance(V(lwu(buf)), V(lwu(buf + 4)));
    const float d2 = vec3_distance(V(lwu(buf + 4)), V(lwu(buf + 8)));
    float f21 = d1 < d2 ? d1 : d2;
    const uint32_t s1 = buf + 0x10, s0 = buf + 0x20, s2 = buf + 0x30;
    vec3_sub(V(s1), V(lwu(buf + 4)), V(lwu(buf)));
    vec3_scale(V(s1), V(s1), f22);
    vec3_sub(V(s0), V(lwu(buf + 8)), V(lwu(buf + 4)));
    vec3_scale(V(s0), V(s0), f23);
    vec3_add(V(s2), V(s1), V(s0));
    f21 = f21 * u2f(0x3f8ccccdu);
    const float len = vec3_length(V(s2));
    if (f21 < len) vec3_set_length(V(out), V(s2), f21);
    else lq_sq(out, s2);
    ee_stack_free(0x40);
}

// The cubic Hermite curve at t between p0 and p1 with tangents m0 / m1: out = p0 (2t^3 - 3t^2 + 1) + m0 (t^3 - 2t^2 + t)
// + m1 (t^3 - t^2) + p1 (3t^2 - 2t^3).
// orig: L1:0x0028ba80
void hermite_l1(uint32_t out, uint32_t p0, uint32_t p1, uint32_t m0, uint32_t m1, float t) {
    const uint32_t buf = ee_stack_alloc(0x10);
    const float f0 = u2f(0x3f800000u);
    const float f23 = fpu_mul(t, t);
    const float f21 = fpu_mul(f23, t);
    const float f24 = fpu_mul(f23, u2f(0x40400000u));
    const float f25 = fpu_add(f21, f21);
    float f20 = fpu_add(f23, f23);
    vec3_scale(V(out), V(p0), fpu_add(fpu_sub(f25, f24), f0));
    f20 = fpu_sub(f21, f20);
    vec3_scale(V(buf), V(m0), fpu_add(f20, t));
    vec3_add(V(out), V(out), V(buf));
    vec3_scale(V(buf), V(m1), fpu_sub(f21, f23));
    vec3_add(V(out), V(out), V(buf));
    vec3_scale(V(buf), V(p1), fpu_sub(f24, f25));
    vec3_add(V(out), V(out), V(buf));
    ee_stack_free(0x10);
}

// A debug particle (kind 0x29, alloc_particle) at the point `pos`: its +4 = `a3`, its colour byte +9 / +3 from t0 / t2
// (t2 == 0xff: a random one from t0), +0xc = f12 * 210000, +2 from the table 0x1b2124 / Novalis 0x1b25a4 at `a2`, +0xa =
// t1, +8 = 0xa0 and its +0x20 = `a1`. Returns the particle (0: none free).
// (Shared: the same code on Aridia, Kerwan, Eudora, Blarg, Orxon and Oltanis.)
// orig: L1:0x00285768 L2:0x00271d80 L3:0x0025e9e0 L4:0x00263098 L6:0x0027f648 L10:0x002634e8 L14:0x00278390
uint32_t debug_particle_l1(uint32_t pos, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, int32_t t1, uint32_t t2, float f12) {
    const uint32_t s1 = t0 & 0xffu, s2 = t2 & 0xffu;
    const int32_t s7 = static_cast<int16_t>(static_cast<uint32_t>(t1) & 0xffffu);
    const uint32_t s0 = alloc_particle(0x29);
    if (s0 == 0) return 0;
    lq_sq(s0 + 0x10, pos);
    sw(s0 + 4, a3);
    if (s2 != 0xff) {
        sb(s0 + 9, s1);
        sb(s0 + 3, s2);
    } else {
        const int32_t v0 = float_to_int(u2f(0x40800000u));
        sb(s0 + 9, ((s1 >> 5) << 4) + static_cast<uint32_t>(v0));
        sb(s0 + 3, (s1 & 1u) != 0 ? 0x48u : 0x44u);
    }
    sb(s0 + 1, 0);
    const float f0 = f12 * u2f(0x484d1400u);
    swc1(s0 + 0xc, f0);
    sb(s0 + 2, lbu(lwu(OV(0x001b2124)) + a2));
    sh(s0 + 0xa, static_cast<uint32_t>(s7));
    sb(s0 + 8, 0xa0);
    sw(s0 + 0x20, a1);
    return s0;
}

// The length of the follower's path: each segment sampled by the Hermite curve from the tangents (t from 0.05 up in
// steps of 0.05 to 1), the pieces' distances summed into the segment's +0x1c and the total returned; the follower's
// index (+0) is restored. With `debug` set the samples are drawn as particles (debug_particle_l1).
// orig: L1:0x0028bb90
float path_measure_l1(uint32_t f, uint32_t debug, uint32_t color, float fa, float fb) {
    const uint32_t buf = ee_stack_alloc(0x70);
    float f26 = 0.0f;
    const int32_t saved = lw(f);
    const uint32_t path = lwu(f + 0x10);
    const int32_t count = lw(path);
    sw(f, 0);
    lq_sq(buf + 0x30, path + 0x10);
    if (debug != 0) debug_particle_l1(buf + 0x30, color, 0, 0x80101080u, 0x7f, -1, 0xff, u2f(0x3f2a7efau));
    int32_t s0 = 0;
    path_spline_tangent_l1(buf + 0x10, f, 0, fa, fb);
    if (count > 0) {
        for (;;) {
            lq_sq(buf, buf + 0x10);
            const uint32_t s4 = buf + 0x20;
            lq_sq(s4, buf + 0x30);
            if (debug != 0) debug_particle_l1(s4, color, static_cast<uint32_t>(ee_rem(s0, 0xf)) + 2u, 0x80108010u, 0x7f, -1, 0xff, u2f(0x3f800000u));
            const int32_t s7 = s0 + 1;
            const int32_t nx = path_index_wrap_l1(f, s0 + 1);
            float f20 = u2f(0x3d4ccccdu), f21 = 0.0f;
            const uint32_t v1 = lwu(f + 0x10);
            lq_sq(buf + 0x30, (static_cast<uint32_t>(nx) << 4) + v1 + 0x10);
            sw(f, static_cast<uint32_t>(s0));
            const float f23 = f20;
            path_spline_tangent_l1(buf + 0x10, f, 1, fa, fb);
            const float f22 = u2f(0x3f800000u);
            const uint32_t s5 = buf + 0x30, s2 = buf + 0x50, s1 = buf + 0x40;
            lq_sq(s2, buf + 0x20);
            const uint32_t s6 = static_cast<uint32_t>(s0) << 4;
            for (;;) {
                if (debug != 0) debug_particle_l1(s2, color, 0, 0x80404080u, 0x7f, -1, 0xff, u2f(0x3e4ccccdu));
                hermite_l1(s1, s4, s5, buf, buf + 0x10, f20);
                f20 = f20 + f23;
                const float d = vec3_distance(V(s2), V(s1));
                f21 = f21 + d;
                lq_sq(s2, buf + 0x40);
                if (!(f20 <= f22)) break;
            }
            s0 = s7;
            f26 = f26 + f21;
            swc1(lwu(f + 0x10) + s6 + 0x1c, f21);
            if (!(s0 < count)) break;
        }
    }
    sw(f, static_cast<uint32_t>(saved));
    ee_stack_free(0x70);
    return f26;
}

}  // namespace rac1
