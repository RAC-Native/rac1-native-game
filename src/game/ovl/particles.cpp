// Ported overlay code: the particles' update. The pool (base 0x16016c, highest slot 0x160174,
// 0x40-byte records: +0 the type, +1 the alive flag (negative: free), +4 the colour, +0xa the
// frames left, +0xc the size, +0x10 the position, +0x20.. the type's own fields) is walked once
// a frame and each live particle handed to its type's function (table 0x1b1e80, filled by
// 0x268900).
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>
#include <game/ovl/vu0_macro.h>
#include <game/ovl/particle_16_spawn.h>

#include <algorithm>

namespace rac1 {

namespace {
int16_t& h16(uint32_t a) { return ee_ref<int16_t>(a); }
}  // namespace

// Every live particle's type function, in pool order. The walk's next record (0x160190) and its
// end (0x160194) live in memory: a type function that frees or spawns sees them.
// orig: 0x00267c08
void particles_update() {
    const uint32_t NEXT = OV(0x00160190), END = OV(0x00160194);
    uint32_t p = lwu(OV(0x0016016c));
    uint32_t end = static_cast<uint32_t>((lw(OV(0x00160174)) + 1) << 6) + p;
    sw(END, end);
    while (p != end) {
        const int32_t alive = static_cast<int8_t>(lbu(p + 1));
        const int32_t type = static_cast<int8_t>(lbu(p));
        p += 0x40;
        if (alive < 0) continue;
        sw(NEXT, p);
        ovl_call_moby_fn(lwu(OV(0x001b1e80) + static_cast<uint32_t>(type << 2)), p - 0x40);
        p = lwu(NEXT);
        end = lwu(END);
    }
}

// The colour lerp's overlay entry (the base 0x200298 unchanged).
// orig: 0x0025a400
uint32_t rgba_lerp_ovl(uint32_t a, uint32_t b, float t) { return rgba_lerp(a, b, t); }

// Type 0x13: moves (+0x10 and +0x20 by +0x30) and fades, the colour's top byte and the size word
// both stepping down 6 a frame, freed when it goes negative.
// orig: 0x0026c7f0
void particle_13_update(uint32_t p) {
    vec3_add(V(p + 0x10), V(p + 0x10), V(p + 0x30));
    vec3_add(V(p + 0x20), V(p + 0x20), V(p + 0x30));
    const uint32_t v = lwu(p + 4) + 0xfa000000u;
    sw(p + 4, v);
    sw(p + 0xc, v);
    if (static_cast<int32_t>(v) < 0) free_particle(p);
}

// Type 0x16 (hand-written VU0 macro code): its frames +0xa counted down, freed (0x267a08) at 0; else it moves (+0x10 += the
// velocity +0x20, then the velocity's z += its w: gravity), its angle byte +8 turns by 1, its size +0xc grows by +0x3c, and its
// colour is the lerp of the end colour +0x34 to the start colour +0x30 by frames left / frames (+0x38), both alphas faded over the
// last 8 frames (x min(8, left) / 8). Kerwan's ambient particles (its type 0x16 at L3:0x255cc8, the same code): unported, they never
// died and filled the 2048-slot pool, and no other particle (the Blaster's glows) could be spawned there.
// orig: 0x00267b10
void particle_16_update(uint32_t p) {
    using namespace vu0m;
    const uint32_t q = p + 0x20;
    const int32_t left = static_cast<int32_t>(lh(p + 0xa)) - 1;
    const int32_t total = lh(q + 0x18);
    const uint32_t c_start = lwu(q + 0x10), c_end = lwu(q + 0x14);
    if (left <= 0) {
        free_particle(p);
        return;
    }
    const float vleft = static_cast<float>(left), vtotal = static_cast<float>(total);  // VITOF0.x
    sh(p + 0xa, static_cast<uint32_t>(left));
    Vf pos = lqc2(p + 0x10), vel = lqc2(q);
    Vf a, b;  // the start / end colours' bytes as floats (PEXTLB / PEXTLH with zero, VITOF0)
    for (int i = 0; i < 4; ++i) {
        a.v[i] = static_cast<float>((c_start >> (8 * i)) & 0xff);
        b.v[i] = static_cast<float>((c_end >> (8 * i)) & 0xff);
    }
    const int32_t fade = std::min(8, left);                                     // PMINW
    const float k = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(fade) << 9)) / 4096.0f;  // SLL 9, VITOF12
    const float Q = div(vleft, vtotal);
    sb(p + 8, lbu(p + 8) + 1u);
    for (int i = 0; i < 3; ++i) pos.v[i] = add(pos.v[i], vel.v[i]);
    vel.v[2] = add(vel.v[2], vel.v[3]);
    a.v[3] = mul(a.v[3], k);
    b.v[3] = mul(b.v[3], k);
    const float w = sub(1.0f, Q), x = add(0.0f, Q);  // VSUBq.w vf3, vf0, Q / VADDq.x vf3, vf0, Q
    sqc2(p + 0x10, pos);
    sqc2(q, vel);
    uint32_t col = 0;
    for (int i = 0; i < 4; ++i) col |= (ftoi(madd(mul(b.v[i], w), a.v[i], x), 1.0f) & 0xffu) << (8 * i);  // VMULAw / VMADDx, VFTOI0, PPACH / PPACB
    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), lwc1(q + 0x1c)));
    sw(p + 4, col);
}

// Type 0x15 (the hit flash 0x26cbb0 spawns): over its frames it shrinks and lerps its colour
// (+0x30 to +0x34) and falls under gravity; with +0x3a set it sheds a child every other frame
// thrown off at a random angle, else its speed drops 4%.
// orig: 0x0026ccb0
void particle_15_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    float f = int_to_float(lh(p + 0xa));
    f = fpu_div(f * lwc1(q + 0x1c), int_to_float(lh(q + 0x18)));
    swc1(p + 0xc, f);
    const float t = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(lh(q + 0x18)));
    sw(p + 4, rgba_lerp_ovl(lwu(q + 0x14), lwu(q + 0x10), t));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(q + 8, lwc1(q + 8) - lwc1(0x0015ed64) * u2f(0x3a83126fu));
    if (lh(q + 0x1a) != 0 && (lw(OV(0x0015f5cc)) & 1) != 0) {
        Vec4 d;
        d.x = rand_range_f(-1.0f, 1.0f);
        d.y = rand_range_f(-1.0f, 1.0f);
        d.z = rand_range_f(-1.0f, 1.0f);
        d.w = 0.0f;
        const float len = vec3_length(V(q));
        vec3_set_length(d, d, len * rand_range_f(u2f(0x3e570a3du), 0.25f));
        vec3_add(d, d, V(q));
        vec3_scale(d, d, rand_range_f(0.75f, u2f(0x3f733333u)));
        const int32_t n = frame_scale_i(rand_range(0xf, 0x14));
        spawn_particle_15_v(V(p + 0x10), d, lwu(p + 4), lwu(q + 0x14), static_cast<uint32_t>(n), 0, lwc1(p + 0xc));
        return;
    }
    vec3_scale(V(q), V(q), u2f(0x3f75c28fu));
}

namespace {
// orig: (helper) repeated in 0x00274a70 — the level bounds it keeps its particles in: 2 .. 1021 on every axis
bool out_of_level(uint32_t p) {
    const float lo = 2.0f, hi = u2f(0x447f4000u);
    for (uint32_t i = 0; i < 3; ++i) {
        const float c = lwc1(p + 0x10 + i * 4);
        if (c < lo || hi < c) return true;
    }
    return false;
}
}  // namespace

// Type 0x45: stays on a moby (+0x3c) by +0x36 (1 / 3 / 2: the points at pvars +0xd0 / +0xe0 /
// +0x1f0, 4: its position 0.2 higher, else it drifts by +0x20), freed outside the level; turns
// (+8) at random, and fades its alpha (+0x30 per frame left, times +0x34) over +0x38's colour.
// orig: 0x00274a70
void particle_45_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (out_of_level(p)) {
        free_particle(p);
        return;
    }
    const float z = lwc1(p + 0x18);
    const int32_t mode = lh(q + 0x16);
    if (mode == 1) {
        V(p + 0x10) = V(lwu(lwu(q + 0x1c) + 0x78) + 0xd0);
    } else if (mode == 3) {
        V(p + 0x10) = V(lwu(lwu(q + 0x1c) + 0x78) + 0xe0);
    } else if (mode == 2) {
        V(p + 0x10) = V(lwu(lwu(q + 0x1c) + 0x78) + 0x1f0);
    } else if (mode == 4) {
        V(p + 0x10) = V(lwu(q + 0x1c) + 0x10);
        swc1(p + 0x18, z + u2f(0x3e4ccccdu));
    } else {
        vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    }
    if (out_of_level(p)) {
        free_particle(p);
        return;
    }
    int32_t a = static_cast<int32_t>(lbu(p + 8));
    a += float_to_int(rand_range_signed_f(0.0f, 64.0f));
    if (!(a < 0x100)) a -= 0xff;
    sb(p + 8, static_cast<uint32_t>(a));
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float f = int_to_float(lh(p + 0xa)) * lwc1(q + 0x10) * int_to_float(lh(q + 0x14));
    sw(p + 4, (static_cast<uint32_t>(float_to_int(f)) << 24) | lwu(q + 0x18));
}

// A direction packed in a word: each axis scaled by 1 / (n * 0.0001) around 127, n (1..255, in
// the top byte) from the longest axis (n = length * 10000 / 63).
// orig: 0x00261480
void dir_pack(uint32_t v, uint32_t out) {
    const float ax = abs_f(lwc1(v));
    float m = abs_f(lwc1(v + 4));
    const float az = abs_f(lwc1(v + 8));
    if (m < ax) m = ax;
    const float top = az < m ? m : az;
    const int32_t i = float_to_int(fpu_div(top * 10000.0f, 63.0f));
    int32_t n = i < 0x100 ? i : 0xff;
    if (!(0 < n)) n = 1;
    const float fn = static_cast<float>(n);
    const float k = fpu_div(1.0f, fn * u2f(0x38d1b717u));
    Vec4 e;
    e.x = lwc1(v) * k + 127.0f;
    e.y = lwc1(v + 4) * k + 127.0f;
    e.z = lwc1(v + 8) * k + 127.0f;
    e.w = fn;
    sw(out, vec_to_rgba(e));
}

// The direction 0x261480 packed: (bytes - 127) * top byte * 0.0001.
// orig: 0x002615a8
void dir_unpack(Vec4& d, uint32_t packed) {
    Vec4 v;
    rgba_to_vec(v, lwu(packed));
    const float k = v.w * u2f(0x38d1b717u);
    vec3_sub(v, v, Vec4{127.0f, 127.0f, 127.0f, 0.0f});
    vec3_scale(d, v, k);
}

// A kind 2 particle (a streak) at `pos`, from direction `a` to `b` (packed by 0x261480, their w
// scaled to +0x36 / +0x38), colours `c0` / `c1`, `n` frames, +0x32 `h32`, +0x34 `h34`; texture
// `tex` (-1: 0x1b20dc's, else table 0x1b2080, bit 16: additive).
// orig: 0x002690b8
uint32_t spawn_particle_02(uint32_t pos, uint32_t a, uint32_t b, uint32_t c0, uint32_t c1, uint32_t n, uint32_t h32, uint32_t h34, int32_t tex) {
    const uint32_t p = alloc_particle(2);
    if (p == 0) return 0;
    V(p + 0x10) = V(pos);
    sw(p + 4, c0);
    const int32_t s = float_to_int(4.0f);
    sw(p + 0xc, 0);
    sb(p + 1, 0);
    sb(p + 9, static_cast<uint32_t>(s + 0x40));
    sb(p + 3, 0x44);
    sb(p + 8, static_cast<uint32_t>(float_to_int(rand_range_f(lwc1(p + 0xc), 255.0f))));
    if (tex == -1) {
        sb(p + 2, lbu(lwu(OV(0x001b20dc))));
    } else {
        sb(p + 2, lbu(lwu(OV(0x001b2080) + (static_cast<uint32_t>(tex) & 0xffffu) * 4)));
        if ((static_cast<uint32_t>(tex) & 0x10000u) != 0) sb(p + 3, 0x48);
    }
    const uint32_t q = p + 0x20;
    sh(p + 0xa, n);
    sh(q + 0x16, static_cast<uint32_t>(float_to_int(fpu_div(lwc1(a + 0xc) * 210000.0f, 1000.0f))));
    sh(q + 0x18, static_cast<uint32_t>(float_to_int(fpu_div(lwc1(b + 0xc) * 210000.0f, 1000.0f))));
    dir_pack(a, q);
    dir_pack(b, q + 4);
    // (the original unpacks both again into its stack frame, unused)
    sh(q + 0x14, h34);
    sw(q + 8, c0);
    sw(q + 0xc, c1);
    sh(q + 0x10, n);
    sh(q + 0x12, h32);
    return p;
}

// A kind 0xf particle (a spark, moved by type 0xf) at `pos` moving `vel`: colours `c0` / `c1`,
// `n` frames (none without), +0x3a `h3a`, texture `tex` (-1: 0x1b20bc's), blend `blend` (-1:
// additive 0x48), size `size`.
// orig: 0x0026bb70
uint32_t spawn_particle_0f(uint32_t pos, uint32_t vel, uint32_t c0, uint32_t c1, uint32_t n, uint32_t h3a, int32_t tex, int32_t blend, float size) {
    if (n == 0) return 0;
    const uint32_t p = alloc_particle(0xf);
    if (p == 0) return 0;
    V(p + 0x10) = V(pos);
    sw(p + 4, c0);
    const int32_t s = float_to_int(4.0f);
    sb(p + 9, static_cast<uint32_t>(s - 0x60));
    sb(p + 3, blend == -1 ? 0x48u : static_cast<uint32_t>(blend));
    sb(p + 1, 0);
    swc1(p + 0xc, size);
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    sb(p + 2, tex != -1 ? static_cast<uint32_t>(tex) : lbu(lwu(OV(0x001b20bc))));
    const uint32_t q = p + 0x20;
    sh(p + 0xa, n);
    sh(q + 0x18, n);
    swc1(q + 0x1c, size);
    sw(q + 0x10, c0);
    sw(q + 0x14, c1);
    sh(q + 0x1a, h3a);
    V(q) = V(vel);
    return p;
}

// A kind 0x40 particle (smoke) at `pos` moving `vel`: `n` frames, colour `color`, +0x38 `w38`,
// additive with `add`; size `size` (* 210000), +0x30 / +0x34 `a` / `b`; drawn further back (+9)
// within 80 of the hero.
// orig: 0x002738e8
uint32_t spawn_particle_40(uint32_t pos, uint32_t vel, uint32_t n, uint32_t color, uint32_t w38, uint32_t add, float size, float a, float b) {
    const uint32_t p = alloc_particle(0x40);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    V(p + 0x10) = V(pos);
    if (vec2_distance(V(0x0013f3d0), V(pos)) < 80.0f) sb(p + 9, static_cast<uint32_t>(float_to_int(1.0f) + 0x30));
    else sb(p + 9, static_cast<uint32_t>(float_to_int(1.0f) + 0x20));
    sb(p + 3, add != 0 ? 0x48u : 0x44u);
    sb(p + 1, 0);
    sw(p + 4, color);
    sb(p + 2, lbu(lwu(OV(0x001b2180)) + 1));
    swc1(p + 0xc, size * 210000.0f);
    const float r = rand_range_f(0.0f, 255.0f);
    sh(p + 0xa, n);
    sb(p + 8, static_cast<uint32_t>(float_to_int(r)));
    V(q) = V(vel);
    sw(q + 0x18, w38);
    swc1(q + 0x10, a);
    swc1(q + 0x14, b);
    return p;
}

// Type 2 (the streak 0x2690b8 spawns) in three phases: along the first packed direction (+0x20)
// for +0x30 frames fading in, between the two (+0x20 -> +0x24) for +0x32 frames lerping colour
// and size, then along the second for +0x34 frames fading out; the texture turns (+8) every
// other frame.
// orig: 0x00269290
void particle_02_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if ((lw(OV(0x0015f5cc)) & 1) != 0) sb(p + 8, lbu(p + 8) + 1u);
    Vec4 d;
    if (lh(q + 0x10) != 0) {
        // (the original also clears the EE timer 0 count, 0x10000000: a hardware register)
        dir_unpack(d, q);
        vec3_add(V(p + 0x10), V(p + 0x10), d);
        const float t = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(lh(q + 0x10)));
        sw(p + 4, rgba_lerp(lwu(q + 8) & 0xffffffu, lwu(q + 8), 1.0f - t));
        swc1(p + 0xc, static_cast<float>(static_cast<int32_t>(lh(q + 0x16)) * 1000));
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            const uint32_t n = lhu(q + 0x12);
            sh(q + 0x10, 0);
            sh(p + 0xa, n);
        }
        return;
    }
    if (lh(q + 0x12) != 0) {
        Vec4 a, b;
        dir_unpack(a, q);
        dir_unpack(b, p + 0x24);
        const float t = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(lh(q + 0x12)));
        vec3_lerp(d, b, a, t);
        vec3_add(V(p + 0x10), V(p + 0x10), d);
        const float lo = static_cast<float>(static_cast<int32_t>(lh(q + 0x18)));
        swc1(p + 0xc, (static_cast<float>(lh(q + 0x16) - lh(q + 0x18)) * t + lo) * 1000.0f);
        sw(p + 4, rgba_lerp(lwu(q + 0xc), lwu(q + 8), t));
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            const uint32_t n = lhu(q + 0x14);
            sh(q + 0x12, 0);
            sh(p + 0xa, n);
        }
        return;
    }
    dir_unpack(d, p + 0x24);
    vec3_add(V(p + 0x10), V(p + 0x10), d);
    const uint32_t c = lwu(q + 0xc) & 0xffffffu;
    const float t = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(lh(q + 0x14)));
    sw(p + 4, rgba_lerp(c, lwu(q + 0xc), t));
    swc1(p + 0xc, static_cast<float>(static_cast<int32_t>(lh(q + 0x18)) * 1000));
    if (timer_tick_i16(h16(p + 0xa)) != 0) free_particle(p);
}

// Type 0xb (the explosion's spark 0x26a9f0 spawns): flies (+0x20, gravity with flag 4, slowed
// 20% / 40% with flag 2), splitting once at 85% of its life into 5 smaller ones (up to two
// generations, +0x3a), grows to the size of its curve, near the camera smaller; its colour
// lerps (+0x30 -> +0x34, or to white with flag 2). At its end it either flies on faster (flag
// 2 -> 4) or turns into smoke (flag 1): rising, slowing 2%, fading out.
// orig: 0x0026ac78
void particle_0b_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    sb(p + 8, lbu(p + 8) + 1u);
    if (((lbu(q + 0x19) ^ 1u) & 1u) != 0) {
        vec3_add(V(p + 0x10), V(p + 0x10), V(q));
        if ((lbu(q + 0x19) & 4) != 0) swc1(q + 8, lwc1(q + 8) + lwc1(0x0015ed70) * u2f(0x411ccccdu));
        vec3_scale(V(q), V(q), (lbu(q + 0x19) & 2) != 0 ? u2f(0x3f19999au) : u2f(0x3f4ccccdu));
        const int32_t left = lh(p + 0xa);
        if (left == float_to_int(static_cast<float>(lbu(q + 0x1b)) * u2f(0x3f59999au)) && lbu(q + 0x1a) < 2 && (lbu(q + 0x19) & 4) == 0) {
            for (int32_t i = 4; i >= 0; --i) {
                const float r = rand_range_f(u2f(0x3f666666u), u2f(0x3f8ccccdu));
                const int32_t n = float_to_int(static_cast<float>(lbu(q + 0x1b)) * r);
                const uint32_t b38 = lbu(q + 0x18);
                const float k = rand_range_f(u2f(0x3f59999au), 1.0f);
                spawn_particle_0b(p + 0x10, q, lwu(q + 0x10), lwu(q + 0x14), static_cast<uint32_t>(n), b38, lbu(q + 0x1a) + 1u, lbu(q + 0x19),
                                  lwc1(q + 0x1c), lwc1(q + 0xc) * k);
            }
        }
        if ((lbu(q + 0x19) & 4) == 0) {
            const float sz = lwc1(q + 0x1c);
            float life = static_cast<float>(lbu(q + 0x1b));
            const float f4 = sz * 8.0f;
            float a = life * 3.0f;
            float b = life * 8.0f;
            const float c0 = f4 * sz;
            const float c2 = a * sz;
            const float c3 = sz * -8.0f;
            b = fpu_div(b - c2, c0 + f4);
            a = fpu_div(a + b * 8.0f * sz, c3);
            life = life - int_to_float(lh(p + 0xa));
            const float root = sqrt_f(b * b - a * 4.0f * life);
            const float nb = -b;
            const float aa = a + a;
            const float r1 = fpu_div(nb - root, aa);
            const float r2 = fpu_div(nb + root, aa);
            swc1(p + 0xc, r1 < r2 ? r2 : r1);
            const float dist = vec3_distance(V(p + 0x10), V(OV(0x00166dc0)));
            if (dist < 10.0f) swc1(p + 0xc, lwc1(p + 0xc) * (dist * u2f(0x3d99999au) + 0.25f));
        }
        uint32_t c;
        if ((lbu(q + 0x19) & 2) != 0) c = rgba_lerp(lwu(q + 0x10), 0x0fffffffu, fpu_div(static_cast<float>(lh(p + 0xa)), static_cast<float>(lbu(q + 0x1b))));
        else c = rgba_lerp(lwu(q + 0x14), lwu(q + 0x10), fpu_div(static_cast<float>(lh(p + 0xa)), static_cast<float>(lbu(q + 0x1b))));
        sw(p + 4, c);
        countdown_u8(p + 0x38);
        if (timer_tick_i16(h16(p + 0xa)) == 0) return;
        const uint32_t fl = lbu(q + 0x19);
        if ((fl & 2) != 0) {
            const uint32_t n = lbu(q + 0x1b) * 3u;
            sh(p + 0xa, n);
            sb(q + 0x1b, n);
            sb(q + 0x19, (lbu(q + 0x19) & 0xfdu) | 4u);
            const float r = rand_range_f(u2f(0x3fcccccdu), 2.0f);
            vec3_set_length(V(q), V(q), lwc1(q + 0xc) * r);
            return;
        }
        sw(q + 0x10, lwu(q + 0x14));
        const uint32_t b = lbu(q + 0x1b);
        sb(q + 0x19, fl | 1u);
        sw(q + 0x14, lbu(p + 3) == 0x48 ? 0u : 0x0f2f3f3fu);
        int32_t n = rand_range(static_cast<int32_t>(b << 1), static_cast<int32_t>((b << 1) + b));
        if ((lbu(q + 0x19) & 8) != 0) n = n * 5;
        sb(q + 0x1b, static_cast<uint32_t>(n));
        sh(p + 0xa, static_cast<uint32_t>(n) & 0xffu);
        return;
    }
    const int32_t full = float_to_int(static_cast<float>(lbu(q + 0x1b)) * 0.75f) + 1;
    const int32_t fade = float_to_int(static_cast<float>(lbu(q + 0x1b)) * 0.25f) + 1;
    swc1(q + 8, lwc1(q + 8) + lwc1(0x0015ed70) * 3.0f);
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    vec3_scale(V(q), V(q), u2f(0x3f7ae148u));
    const int32_t left = lh(p + 0xa);
    if (full < left) {
        sw(p + 4, rgba_lerp(lwu(q + 0x14), lwu(q + 0x10), fpu_div(static_cast<float>(left - full), static_cast<float>(fade))));
    } else if (lbu(p + 3) == 0x48) {
        sh(p + 0xa, 0);
    } else {
        const int32_t a = (left * 16 - left) / full;
        sw(p + 4, (lwu(p + 4) & 0xffffffu) | (static_cast<uint32_t>(a) << 24));
    }
    if (1.0f < lwc1(OV(0x0015f5d0))) timer_tick_i16(h16(p + 0xa));
    if (timer_tick_i16(h16(p + 0xa)) != 0) free_particle(p);
}

// Type 0xf (the spark 0x26bb70 spawns): as type 0x15, shrinking and lerping its colour over its
// frames, falling (14.6 g) and, with +0x3a set, shedding a child (its own texture and blend)
// every other frame; else slowing 4%.
// orig: 0x0026bca8
void particle_0f_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    float f = int_to_float(lh(p + 0xa));
    f = fpu_div(f * lwc1(q + 0x1c), int_to_float(lh(q + 0x18)));
    swc1(p + 0xc, f);
    const float t = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(lh(q + 0x18)));
    sw(p + 4, rgba_lerp_ovl(lwu(q + 0x14), lwu(q + 0x10), t));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(q + 8, lwc1(q + 8) - lwc1(0x0015ed70) * u2f(0x4169999au));
    if (lh(q + 0x1a) != 0 && (lw(OV(0x0015f5cc)) & 1) != 0) {
        const uint32_t d = ee_stack_alloc(0x10);  // (the child's velocity goes by address)
        Vec4 r;
        r.x = rand_range_f(-1.0f, 1.0f);
        r.y = rand_range_f(-1.0f, 1.0f);
        r.z = rand_range_f(-1.0f, 1.0f);
        r.w = 0.0f;
        V(d) = r;
        const float len = vec3_length(V(q));
        vec3_set_length(V(d), V(d), len * rand_range_f(u2f(0x3e19999au), 0.25f));
        vec3_add(V(d), V(d), V(q));
        vec3_scale(V(d), V(d), rand_range_f(0.75f, u2f(0x3f733333u)));
        const int32_t n = frame_scale_i(rand_range(0xf, 0x1e));
        spawn_particle_0f(p + 0x10, d, lwu(p + 4), lwu(q + 0x14), static_cast<uint32_t>(n), 0, lbu(p + 2), lbu(p + 3), lwc1(p + 0xc));
        ee_stack_free(0x10);
        return;
    }
    vec3_scale(V(q), V(q), u2f(0x3f75c28fu));
}

// Type 0x22 (dust, 0x26f080): grows (to +0x2c) and drifts, slowing, rising toward 1.3 a frame
// and swaying across the camera (+0x34 the phase, +0x38 its step, +0x3c the width); above the
// height +0x30 (or now and then within 52 of it) it fades out over the last 14 frames.
// orig: 0x0026f280
void particle_22_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    auto cut = [&] {
        if (frame_scale_i(0xe) - 2 < lh(p + 0xa)) sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0xe) - 2));
    };
    if (lwc1(q + 0x10) < lwc1(p + 0x18)) {
        sw(q + 8, 0);
        cut();
    }
    if (lwc1(q + 0x10) < lwc1(p + 0x18) + lwc1(0x0015ed6c) * u2f(0x3fa66666u) * 40.0f && rand_mod(0x14) == 0) cut();
    if (lh(p + 0xa) < frame_scale_i(0xe)) {
        const int32_t a = static_cast<int32_t>(lbu(p + 7)) * lh(p + 0xa) / frame_scale_i(0xe);
        sw(p + 4, (lwu(p + 4) & 0xffffffu) | (static_cast<uint32_t>(a) << 24));
    }
    swc1(p + 0xc, lwc1(p + 0xc) + lwc1(0x0015ed60) * u2f(0x44520001u));
    if (lwc1(q + 0xc) < lwc1(p + 0xc)) swc1(p + 0xc, lwc1(q + 0xc));
    Vec4 v = V(q);
    v.w = 0.0f;
    const float len = vec2_length(v);
    vec2_set_length(v, v, lwc1(0x0015ed60) * u2f(0xbcf5c280u) * len + len);
    vec3_add(V(p + 0x10), V(p + 0x10), v);
    approach_f_v(v.z, lwc1(0x0015ed6c) * u2f(0x3fa66666u), lwc1(0x0015ed70) * 5.0f);
    swc1(q + 8, v.z);
    swc1(q, v.x);
    swc1(q + 4, v.y);
    swc1(q + 0x14, angle_add(lwc1(q + 0x14), lwc1(q + 0x18)));
    const float a = angle_add(lwc1(OV(0x00166dd8)), u2f(0x3fc90fdbu));
    const float w = sin_vu(lwc1(q + 0x14)) * lwc1(q + 0x1c);
    Vec4 s;
    s.x = cos_vu(a) * w;
    s.y = sin_vu(a) * w;
    s.z = 0.0f;
    s.w = 0.0f;
    vec3_add(V(p + 0x10), V(p + 0x10), s);
}

// A kind 0x42 particle (a splash ring) at `pos`: colour `colour` (-1: grey, alpha 0x14 .. 0x19),
// size `size` (* 210000), 4 frames, spreading 0.0015 .. 0.0025 a frame in a random direction,
// +0x30 `z`.
// orig: 0x002741a0
uint32_t spawn_particle_42(uint32_t pos, int32_t colour, float size, float z) {
    const uint32_t p = alloc_particle(0x42);
    if (p == 0) return 0;
    V(p + 0x10) = V(pos);
    uint32_t c = static_cast<uint32_t>(colour);
    if (colour == -1) c = (static_cast<uint32_t>(rand_mod(6) + 0x14) << 24) | 0x808080u;
    sw(p + 4, c);
    sb(p + 9, static_cast<uint32_t>(float_to_int(2.0f) + 0x20));
    sb(p + 3, 0x48);
    sb(p + 1, 1);
    const uint32_t t = lwu(OV(0x001b2188));
    sb(p + 2, (rand_mod(0x100) & 1) != 0 ? lbu(t) : lbu(t + 1));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    const uint32_t q = p + 0x20;
    swc1(p + 0xc, size * 210000.0f);
    sh(p + 0xa, 4);
    const float a = rand_angle();
    const float r = rand_range_f(u2f(0x3ac49ba6u), u2f(0x3b23d70au));
    swc1(q, cos_vu(a) * r);
    swc1(q + 0x10, z);
    sw(q + 8, 0);
    swc1(q + 4, sin_vu(a) * r);
    return p;
}

// A kind 0x10 particle (steam) at `pos` moving `vel`: none without frames, and only one in 3 / 2
// / never when the frame load 0x15f5d0 is over 0.9 / 0.95 / 1.0; colours `c0` / `c1`, `n`
// frames, texture `h` of the set 0x1b20c0 (3: its first) also +0x3a, size `size`, +0x2c its
// height less 0.5.
// orig: 0x0026bed0
uint32_t spawn_particle_10(uint32_t pos, uint32_t vel, uint32_t c0, uint32_t c1, uint32_t n, uint32_t h, float size) {
    if (n == 0) return 0;
    if (u2f(0x3f666666u) < lwc1(OV(0x0015f5d0)) && rand_mod(3) == 0) return 0;
    if (u2f(0x3f733333u) < lwc1(OV(0x0015f5d0)) && rand_mod(2) == 0) return 0;
    if (1.0f < lwc1(OV(0x0015f5d0)) && rand_mod(1) == 0) return 0;
    const uint32_t p = alloc_particle(0x10);
    if (p == 0) return 0;
    V(p + 0x10) = V(pos);
    sw(p + 4, c0);
    const int32_t s = float_to_int(4.0f);
    sb(p + 1, 0);
    swc1(p + 0xc, size);
    sb(p + 3, 0x44);
    sb(p + 9, static_cast<uint32_t>(s + 0x40));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    const uint32_t set = lwu(OV(0x001b20c0));
    const uint32_t q = p + 0x20;
    sb(p + 2, lbu((h ^ 3u) == 0 ? set : set + h));
    sh(p + 0xa, n);
    sh(q + 0x18, n);
    swc1(q + 0x1c, size);
    sw(q + 0x10, c0);
    sw(q + 0x14, c1);
    sh(q + 0x1a, h);
    V(q) = V(vel);
    swc1(q + 0xc, lwc1(pos + 8) - 0.5f);
    return p;
}

// Type 0x23 (a droplet, 0x26f548) by its mode +0x30: 0 flies free; 1 / other until it hits the
// level (mode 1: a splash 0x2715e8 on plain ground; other: a ring 0x2741a0 on water); 3 down to
// the water height 0x13f640 (a splash). It falls (+0x3c a frame), turns, and fades out once its
// frames +0x34 are spent or it is below +0x38.
// orig: 0x0026f660
void particle_23_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const int32_t mode = lw(q + 0x10);
    auto hits = [&](Vec4& to) {
        vec3_add(to, V(p + 0x10), V(q));
        return segment_collide_v(V(p + 0x10), to, 2, 0, nullptr) != 0;
    };
    if (mode == 0) {
        vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    } else if (mode == 1) {
        Vec4 to;
        if (hits(to)) {
            if (sub_001f35d8() == 0) spawn_particle_2d(OV(0x00173e60), 0, 0xffffffffu, 0.2f, u2f(0x45a41000u));
            free_particle(p);
            return;
        }
        V(p + 0x10) = to;
    } else if (mode == 3) {
        vec3_add(V(p + 0x10), V(p + 0x10), V(q));
        const float w = lwc1(0x0013f640);
        if (lwc1(p + 0x18) < w) {
            swc1(p + 0x18, w);
            spawn_particle_2d(p + 0x10, 0, 0xffffffffu, 0.2f, u2f(0x45a41000u));
            free_particle(p);
            return;
        }
    } else {
        Vec4 to;
        if (hits(to)) {
            if (sub_001f35d8() != 0) {
                swc1(OV(0x00173e68), lwc1(OV(0x00173e68)) + u2f(0x3d4ccccdu));
                spawn_particle_42(OV(0x00173e60), -1, 0.25f, u2f(0x4544e000u));
            }
            free_particle(p);
            return;
        }
        V(p + 0x10) = to;
    }
    swc1(q + 8, lwc1(q + 8) - lwc1(q + 0x1c));
    sb(p + 8, lbu(p + 8) + 2u);
    const int32_t left = lw(q + 0x14) - 1;
    sw(q + 0x14, static_cast<uint32_t>(left));
    if (0 < left && !(lwc1(p + 0x18) < lwc1(q + 0x18))) return;
    const uint32_t c = lwu(p + 4) + 0xf0000000u;
    sw(p + 4, c);
    if (static_cast<int32_t>(c) < 0) free_particle(p);
}

// Type 0x40 (a drop of steam 0x2738e8 spawns): falls (from its frames out, faster by +0x34)
// until it reaches the water height +0x30, then bursts into a spark (0x26bb70: usually a small
// sideways one, one in 0x160228 a big one up) and, in level 10, now and then a splash and a puff
// of steam; it is freed there.
// orig: 0x00273a68
void particle_40_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) swc1(q + 8, lwc1(q + 8) - lwc1(q + 0x14));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    sb(p + 8, lbu(p + 8) + 2u);
    if (!(lwc1(p + 0x18) < lwc1(q + 0x10))) return;
    const uint32_t c = lwu(p + 4);
    const uint32_t r = c & 0xffu, g = (c & 0xff00u) >> 8, b = (c >> 16) & 0xffu;
    float size;
    int32_t n;
    if (rand_mod(lw(OV(0x00160228)) - 1) == 0) {
        swc1(p + 0x18, lwc1(q + 0x10));
        swc1(p + 0x20, lwc1(p + 0x20) * rand_range_f(1.0f, 3.5f));
        swc1(q + 4, lwc1(q + 4) * rand_range_f(1.0f, 3.5f));
        swc1(q + 8, lwc1(q + 8) * rand_range_f(-1.0f, -1.5f));
        size = lwc1(p + 0xc) * 0.5f;
        n = rand_range(frame_scale_i(0x3c), frame_scale_i(0x78));
    } else {
        swc1(p + 0x18, lwc1(q + 0x10));
        swc1(p + 0x20, lwc1(p + 0x20) + rand_range_f(-5.0f, 5.0f) * lwc1(0x0015ed6c));
        swc1(q + 4, lwc1(q + 4) + rand_range_f(-5.0f, 5.0f) * lwc1(0x0015ed6c));
        swc1(q + 8, lwc1(q + 8) * rand_range_f(-0.5f, -1.0f));
        size = lwc1(p + 0xc) * 0.25f;
        n = rand_range(frame_scale_i(0x1e), frame_scale_i(0x3c));
    }
    spawn_particle_0f(p + 0x10, q, lwu(p + 4), lwu(q + 0x18), static_cast<uint32_t>(n), 0, lbu(p + 2), lbu(p + 3), size);
    if (lw(0x0015ed84) == 10) {
        const uint32_t sp = ee_stack_alloc(0x10);  // (the spawns take the point by address)
        if (rand_mod(0x13) == 0) {
            V(sp) = V(p + 0x10);
            swc1(sp + 8, lwc1(sp + 8) + u2f(0x3dcccccdu));
            const uint32_t col = (static_cast<uint32_t>(rand_mod(6) + 0x34) << 24) | (b << 16) | (g << 8) | r;
            spawn_particle_2d(sp, 0, col, u2f(0x3d4ccccdu), u2f(0x46241000u));
        }
        if (rand_mod(9) == 0) {
            const uint32_t full = (b << 16) | (g << 8) | 0x7f000000u | r;
            const uint32_t c0 = rgba_lerp_ovl(0x7f000000u, full, rand_range_f(0.25f, 1.0f));
            const uint32_t c1 = rgba_lerp_ovl(0u, 0x5f00u, rand_range_f(0.5f, 1.0f));
            swc1(q, rand_range_f(-1.0f, 1.0f));
            const float y = rand_range_f(-1.0f, 1.0f);
            sw(q + 8, 0);
            swc1(q + 4, y);
            vec3_set_length(V(q), V(q), rand_range_f(1.5f, 3.0f) * lwc1(0x0015ed6c));
            swc1(q + 8, lwc1(0x0015ed6c) * 3.0f);
            V(sp) = V(p + 0x10);
            swc1(sp + 8, lwc1(sp + 8) - 1.5f);
            const float sz = rand_range_f(u2f(0x48cd1400u), u2f(0x4919cf00u));
            const int32_t k = rand_range(frame_scale_i(0x5a), frame_scale_i(0x96));
            spawn_particle_10(sp, q, c0, c1, static_cast<uint32_t>(k), 3, sz);
        }
        ee_stack_free(0x10);
    }
    free_particle(p);
}

// Type 0x1c (a glow): its strength +0x24 rises by +0x2c (or falls, slower below 0.03 while it
// grows), bounces back down at 0.12, the particle freed at 0.0244 or when its frames end; it
// grows, its alpha follows the strength, it turns (+0x28) and drifts by +0x30.
// orig: 0x0026e150
void particle_1c_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const float rate = lwc1(q + 0xc);
    const float k = u2f(0x3be56042u);
    if (0.0f < rate) {
        swc1(q + 4, lwc1(q + 4) + lwc1(0x0015ed60) * k * rate);
    } else if (u2f(0x3cf5c28fu) < lwc1(q + 4)) {
        swc1(q + 4, lwc1(q + 4) + lwc1(0x0015ed60) * k * rate);
    } else {
        const float g = lwc1(0x0015ed60);
        swc1(q + 4, lwc1(q + 4) + g * k * rate * u2f(0x3e4ccccdu));
        swc1(p + 0xc, lwc1(p + 0xc) + g * u2f(0x45aaa000u));
    }
    if (lwc1(q + 4) <= u2f(0x3cc7e282u) || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float s = lwc1(q + 4);
    if (u2f(0x3df5c28fu) <= s) {
        const float r = -lwc1(q + 0xc);
        swc1(q + 0xc, r);
        swc1(q + 4, s + lwc1(0x0015ed60) * k * r);
    }
    swc1(p + 0xc, lwc1(p + 0xc) + lwc1(0x0015ed60) * u2f(0x45aaa000u));
    const int32_t a = float_to_int(lwc1(q + 4) * 255.0f);
    sw(p + 4, (lwu(p + 4) & 0xffffffu) | (static_cast<uint32_t>(a) << 24));
    float t = lwc1(q + 8) + lwc1(0x0015ed60) * u2f(0x3b03126fu);
    swc1(q + 8, t);
    if (1.0f < t) swc1(q + 8, t - 1.0f);
    sb(p + 8, static_cast<uint32_t>(float_to_int(lwc1(q + 8) * 255.0f)));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q + 0x10));
}

// Type 0x35 (a puff): its size eases from +0x24 toward the middle of +0x20 / +0x24 over its
// first 5 frames then toward +0x20 (and its alpha from the byte +0x2b to half of it, then to
// 0); it turns (+0x2a), drifts (+0x2c, the last component slowing by +0x38), freed outside
// the level or when its frames run out.
// orig: 0x002721f0
void particle_35_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    sb(p + 8, lbu(p + 8) + lbu(q + 0xa));
    const int32_t age = lh(q + 8) - lh(p + 0xa);
    float alpha;
    if (age < 6) {
        const float t = fpu_div(static_cast<float>(age), 5.0f);
        const float a = lwc1(q + 4);
        swc1(p + 0xc, a + ((lwc1(q) + a) * 0.5f - a) * t);
        const uint32_t b = lbu(q + 0xb);
        alpha = static_cast<float>(static_cast<int32_t>(b)) + static_cast<float>(static_cast<int32_t>((b >> 1) - b)) * t;
    } else {
        const float n = int_to_float(lh(q + 8) - 6);
        const float a = lwc1(q);
        const float t = fpu_div(static_cast<float>(lh(p + 0xa)), n);
        swc1(p + 0xc, a + ((a + lwc1(q + 4)) * 0.5f - a) * t);
        alpha = static_cast<float>(static_cast<int32_t>(lbu(q + 0xb) >> 1)) * t + 0.0f;
    }
    const int32_t al = float_to_int(alpha);
    sw(p + 4, (static_cast<uint32_t>(al) << 24) | (lwu(p + 4) & 0xffffffu));
    Vec4 d;
    d.x = lwc1(q + 0xc);
    d.y = lwc1(q + 0x10);
    d.z = lwc1(q + 0x14);
    d.w = 0.0f;
    vec3_add(V(p + 0x10), V(p + 0x10), d);
    swc1(q + 0x14, lwc1(q + 0x14) - lwc1(q + 0x18));
    if (out_of_level(p)) {
        free_particle(p);
        return;
    }
    if (timer_tick_i16(h16(p + 0xa)) != 0) free_particle(p);
}

// Type 4 (the trail spark 0x269958 spawns): its size from +0x38 to +0x3a (thousands) over its
// +0x3c frames, slowing 2% a frame, its colour lerped +0x30 -> +0x34; it turns (+8).
// orig: 0x00269a70
void particle_04_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const int32_t total = lw(q + 0x1c);
    const int32_t n = lh(q + 0x18) * (total - lh(p + 0xa)) / total + lh(q + 0x1a);
    const float size = int_to_float(n) * 1000.0f;
    const float k = lwc1(0x0015ed60) * u2f(0xbca3d700u) + 1.0f;
    swc1(p + 0xc, size);
    vec3_scale(V(q), V(q), k);
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    sb(p + 8, lbu(p + 8) + 1u);
    const float t = fpu_div(static_cast<float>(lh(p + 0xa)), int_to_float(lw(q + 0x1c)));
    sw(p + 4, rgba_lerp_ovl(lwu(q + 0x14), lwu(q + 0x10), t));
    if (timer_tick_i16(h16(p + 0xa)) != 0) free_particle(p);
}

namespace {
// orig: (helper) repeated in 0x00274308 / 0x00274de0 / 0x002719f0 — every `period` frames the
// alpha steps down 1; freed when it reaches 0
void alpha_countdown(uint32_t p, uint32_t period) {
    const uint32_t h = lhu(p + 0xa) - 1u;
    sh(p + 0xa, h);
    if ((h & 0xffffu) == 0) {
        sh(p + 0xa, period);
        sw(p + 4, lwu(p + 4) + 0xff000000u);
    }
    if ((lwu(p + 4) & 0xff000000u) == 0) free_particle(p);
}
}  // namespace

// Type 0xd (a glint): turns (+0x28 a frame), rises (+0x30), shrinks (times +0x2c); it fades in
// to alpha 0x20 .. 0x80 over 10 frames, holds 0x7f for 30, then fades out over 30.
// orig: 0x0026b910
void particle_0d_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    sb(p + 8, lbu(p + 8) + lbu(q + 8));
    swc1(p + 0x18, lwc1(p + 0x18) + lwc1(q + 0x10));
    swc1(p + 0xc, lwc1(p + 0xc) * lwc1(q + 0xc));
    if (lw(q + 4) == 0) {
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            sw(q + 4, 1);
            sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sw(p + 4, lwu(q + 0x14) | 0x7f000000u);
            return;
        }
        const int32_t n = frame_scale_i(0xa);
        const float k = fpu_div(96.0f, static_cast<float>(frame_scale_i(0xa)));
        const int32_t a = float_to_int(static_cast<float>(n - lh(p + 0xa)) * k) + 0x20;
        sw(p + 4, (static_cast<uint32_t>(a) << 24) | lwu(q + 0x14));
        return;
    }
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float k = fpu_div(127.0f, static_cast<float>(frame_scale_i(0x1e)));
    const int32_t a = float_to_int(static_cast<float>(lh(p + 0xa)) * k);
    sw(p + 4, (static_cast<uint32_t>(a) << 24) | lwu(q + 0x14));
}

// Type 0x2e (a ripple): drifts, grows, turns ever slower (+0x34 times 0.98 onto +0x30), kept just
// above the height at *(+0x38) while near it; its alpha fades a step a frame.
// orig: 0x002719f0
void particle_2e_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(p + 0xc, lwc1(p + 0xc) + u2f(0x45b7c000u));
    const float spin = lwc1(q + 0x14) * u2f(0x3f7ae148u);
    const float turn = lwc1(q + 0x10) + spin;
    swc1(q + 0x14, spin);
    swc1(q + 0x10, turn);
    sb(p + 8, static_cast<uint32_t>(float_to_int(turn)));
    if (lwu(q + 0x18) != 0 && abs_f(lwc1(p + 0x18) - lwc1(lwu(q + 0x18))) < u2f(0x3e4ccccdu))
        swc1(p + 0x18, lwc1(lwu(q + 0x18)) + u2f(0x3ca3d70au));
    alpha_countdown(p, 1);
}

// Type 0x2f (a dust ring): slows 2.5%, drifts, grows, turns by +0x30, its alpha down 2 a frame.
// orig: 0x00271be8
void particle_2f_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    vec3_scale(V(q), V(q), u2f(0x3f79999au));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(p + 0xc, lwc1(p + 0xc) + u2f(0x4665b000u));
    const uint32_t c = lwu(p + 4) + 0xfe000000u;
    sw(p + 4, c);
    sb(p + 8, lbu(p + 8) + lbu(q + 0x10));
    if (static_cast<int32_t>(c) < 0) free_particle(p);
}

// Type 0x29: no motion of its own; with a life +0xa (-1: none), freed when it runs out (seen on Blarg).
// orig: 0x00270710
void particle_29_update(uint32_t p) {
    if (lh(p + 0xa) == -1) return;
    if (timer_tick_i16(ee_ref<int16_t>(p + 0xa)) != 0) free_particle(p);
}

// Type 0x3b: on the hero (mode +0x30 1: at the hero plus +0x20), for its frames.
// orig: 0x00273088
void particle_3b_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (lw(q + 0x10) == 1) vec3_add(V(p + 0x10), V(0x0013f3d0), V(q));
    if (timer_tick_i16(h16(p + 0xa)) != 0) free_particle(p);
}

// Type 0x3e: kept at its moby (mode +0x22 2: *(+0x24)'s position plus +0x30; 1: no change at
// all), its alpha down by +0x20 a frame, freed at 0.
// orig: 0x00273800
void particle_3e_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const int32_t mode = lh(q + 2);
    if (mode == 1) return;
    if (mode == 2) {
        swc1(p + 0x10, lwc1(lwu(q + 4) + 0x10) + lwc1(q + 0x10));
        swc1(p + 0x14, lwc1(lwu(q + 4) + 0x14) + lwc1(q + 0x14));
        swc1(p + 0x18, lwc1(lwu(q + 4) + 0x18) + lwc1(q + 0x18));
    }
    const uint32_t c = lwu(p + 4);
    const int32_t a = (static_cast<int32_t>(c) >> 24) - lh(p + 0x20);
    if (!(0 < a)) {
        free_particle(p);
        return;
    }
    sw(p + 4, (c & 0xffffffu) | (static_cast<uint32_t>(a) << 24));
}

// Type 0x42 (the splash ring 0x2741a0 spawns): drifts, grows by +0x30, turns; its alpha down a
// step every 2 frames.
// orig: 0x00274308
void particle_42_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(p + 0xc, lwc1(p + 0xc) + lwc1(q + 0x10));
    sb(p + 8, lbu(p + 8) + 1u);
    alpha_countdown(p, 2);
}

// Type 0x46: its alpha down a step every 2 frames.
// orig: 0x00274de0
void particle_46_update(uint32_t p) { alpha_countdown(p, 2); }

// Type 0x12 (a pulse): its strength +0x24 changes by +0x2c (bouncing back at 0.22, freed at 0
// or when its frames end); it grows, its alpha follows the strength, it turns (+0x28) and
// sinks 0.02 a frame.
// orig: 0x0026c558
void particle_12_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const float k = u2f(0x3c54fdf4u);
    const float s = lwc1(q + 4) + lwc1(0x0015ed60) * k * lwc1(q + 0xc);
    swc1(q + 4, s);
    if (s <= 0.0f || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float cur = lwc1(q + 4);
    if (u2f(0x3e6147aeu) <= cur) {
        const float r = -lwc1(q + 0xc);
        swc1(q + 0xc, r);
        swc1(q + 4, cur + lwc1(0x0015ed60) * k * r);
    }
    swc1(p + 0xc, lwc1(p + 0xc) + lwc1(0x0015ed60) * u2f(0x45fff000u));
    const int32_t a = float_to_int(lwc1(q + 4) * 255.0f);
    sw(p + 4, (lwu(p + 4) & 0xffffffu) | (static_cast<uint32_t>(a) << 24));
    const float t = lwc1(q + 8) + lwc1(0x0015ed60) * u2f(0x3c449ba6u);
    swc1(q + 8, t);
    if (1.0f < t) swc1(q + 8, t - 1.0f);
    const float turn = lwc1(q + 8) * 255.0f;
    swc1(p + 0x18, lwc1(p + 0x18) - u2f(0x3ca3d70au));
    sb(p + 8, static_cast<uint32_t>(float_to_int(turn)));
}

// Type 0x2d (a splash): drifts, freed outside the level; grows by +0x30, turns, kept just above
// the height at *(+0x34) while near it; its alpha down a step every 4 frames.
// orig: 0x00271748
void particle_2d_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    if (out_of_level(p)) {
        free_particle(p);
        return;
    }
    const float z = lwc1(p + 0x18);
    swc1(p + 0xc, lwc1(p + 0xc) + lwc1(q + 0x10));
    sb(p + 8, lbu(p + 8) + 1u);
    if (lwu(q + 0x14) != 0 && abs_f(z - lwc1(lwu(q + 0x14))) < u2f(0x3e4ccccdu)) swc1(p + 0x18, lwc1(lwu(q + 0x14)) + u2f(0x3ca3d70au));
    alpha_countdown(p, 4);
}

// Type 0x3c: drifting with the hero (+0x3c 1: its offset +0x30 from the hero kept), freed outside
// the level or when its frames end; its alpha fades by a frame's share.
// orig: 0x00273298
void particle_3c_update(uint32_t p) {
    const uint32_t q = p + 0x20, H = 0x0013f3d0;
    if (lw(q + 0x1c) == 1) {
        swc1(p + 0x10, lwc1(H) + lwc1(q + 0x10));
        swc1(p + 0x14, lwc1(H + 4) + lwc1(q + 0x14));
        swc1(p + 0x18, lwc1(H + 8) + lwc1(q + 0x18));
        vec3_add(V(p + 0x10), V(p + 0x10), V(q));
        swc1(q + 0x10, lwc1(p + 0x10) - lwc1(H));
        swc1(q + 0x14, lwc1(p + 0x14) - lwc1(H + 4));
        swc1(q + 0x18, lwc1(p + 0x18) - lwc1(H + 8));
    } else {
        vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    }
    if (out_of_level(p) || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const uint32_t c = lwu(p + 4) & 0xffffffu;
    const float t = fpu_div(int_to_float(lh(p + 0xa) - 1), int_to_float(lh(p + 0xa)));
    sw(p + 4, rgba_lerp(c, lwu(p + 4), t));
}

// Type 0x10 (the steam 0x26bed0 spawns): lerps its colour over its frames (ticking twice with
// kind 3 under load), fades in its last 6 frames and near the camera (freed within 4), drifts and
// falls by its kind's gravity (+0x3a: 4.8, 24.5, 9.8 or -0.24 g); below its floor +0x2c it is
// freed, kind 1 leaving a puff of steam behind.
// orig: 0x0026c088
void particle_10_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const float g = lwc1(0x0015ed70);
    float fall[4];
    fall[3] = -(g * u2f(0x3e75c28fu));
    fall[0] = g * u2f(0x4099999au);
    fall[1] = g * 24.5f;
    fall[2] = g * u2f(0x411ccccdu);
    if (lh(q + 0x1a) == 3 && u2f(0x3f733333u) < lwc1(OV(0x0015f5d0))) timer_tick_i16(h16(p + 0xa));
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float t = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(lh(q + 0x18)));
    const uint32_t c = rgba_lerp_ovl(lwu(q + 0x14), lwu(q + 0x10), t);
    int32_t a = static_cast<int32_t>(c) >> 24;
    sw(p + 4, c);
    const float dist = vec3_distance(V(p + 0x10), V(OV(0x00166dc0)));
    if (lh(p + 0xa) < frame_scale_i(6)) a = a * lh(p + 0xa) / frame_scale_i(6);
    if (dist < 4.0f) {
        free_particle(p);
        return;
    }
    if (dist < 8.0f) a = float_to_int(int_to_float(a) * (dist - 4.0f) * 0.25f);
    sw(p + 4, (static_cast<uint32_t>(a) << 24) | (lwu(p + 4) & 0xffffffu));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    const int32_t k = lh(q + 0x1a);
    swc1(q + 8, lwc1(q + 8) - fall[k]);
    if (!(lwc1(p + 0x18) < lwc1(q + 0xc))) {
        sb(p + 8, lbu(p + 8) + 1u);
        return;
    }
    if (k == 1) {
        const uint32_t c0 = rgba_lerp_ovl(0x7f000000u, 0x7f182030u, rand_range_f(0.25f, 1.0f));
        const uint32_t c1 = rgba_lerp_ovl(0u, 0x005f5f5fu, rand_range_f(0.5f, 1.0f));
        sw(q + 8, 0);
        const float size = rand_range_f(u2f(0x48435000u), u2f(0x48927c00u));
        const int32_t n = rand_range(frame_scale_i(0x5a), frame_scale_i(0x96));
        spawn_particle_10(p + 0x10, q, c0, c1, static_cast<uint32_t>(n), 3, size);
    }
    free_particle(p);
}

// Type 0x17: grows its texture index (+8) by +0x28 and drifts by +0x30; its size scales by
// (+0x2c - 1) per frame. Phase +0x24: 0 fades in over 10 frames (then 1 for 20 frames at full
// alpha, then freed), 3 fades from +0x2b towards +0x2a's alpha over 30 frames and moves on to 2,
// which fades out by the timer over +0x2b. Its colour: +0x20.
// orig: 0x0026d1b0
void particle_17_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    sb(p + 8, lbu(p + 8) + lbu(q + 8));
    vec3_add(V(p + 0x10), V(p + 0x10), V(q + 0x10));
    swc1(p + 0xc, lwc1(p + 0xc) * ((lwc1(q + 0xc) - 1.0f) * lwc1(0x0015ed60) + 1.0f));
    const uint32_t phase = lwu(q + 4);
    if (phase == 0) {
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            sw(q + 4, 1);
            sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x14)));
            sw(p + 4, lwu(q) | 0x7f000000u);
            return;
        }
        const int32_t n = frame_scale_i(10);
        const float k = fpu_div(96.0f, int_to_float(frame_scale_i(10)));
        const int32_t a = float_to_int(int_to_float(n - lh(p + 0xa)) * k) + 0x20;
        sw(p + 4, (static_cast<uint32_t>(a) << 24) | lwu(q));
        return;
    }
    if (phase == 2) {
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            free_particle(p);
            return;
        }
        const float d = int_to_float(lbu(q + 0xb));
        const float t = fpu_div(int_to_float(lh(p + 0xa)), d);
        const int32_t a = float_to_int(int_to_float(lbu(q + 0xa)) * t + 0.0f);
        sw(p + 4, (static_cast<uint32_t>(a) << 24) | (lwu(q) & 0x00ffffffu));
        return;
    }
    if (phase == 3) {
        timer_tick_i16(h16(p + 0xa));
        float t = int_to_float(static_cast<int32_t>(lbu(q + 0xb)) - lh(p + 0xa));
        t = fpu_div(t, int_to_float(frame_scale_i(0x1e)));
        if (1.0f < t) t = 1.0f;
        const int32_t a = float_to_int(int_to_float(lbu(q + 0xa)) * t + 0.0f);
        sw(p + 4, (static_cast<uint32_t>(a) << 24) | (lwu(q) & 0x00ffffffu));
        const int32_t left = static_cast<int32_t>(lbu(q + 0xb)) - lh(p + 0xa);
        if (left < frame_scale_i(0x1e)) return;
        const int32_t n = frame_scale_i(0x1e);
        sw(q + 4, 2);
        sb(q + 0xb, lbu(q + 0xb) - static_cast<uint32_t>(n));
        return;
    }
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const float k = fpu_div(127.0f, int_to_float(frame_scale_i(0x14)));
    const int32_t a = float_to_int(int_to_float(lh(p + 0xa)) * k);
    sw(p + 4, (static_cast<uint32_t>(a) << 24) | lwu(q));
}

// Type 0x19 (a spark): moves by +0x20 until it hits the level (it stops there and doubles its
// size; else +0x28 falls by +0x2c, or by gravity when that is 0), shrinks by 100 per frame, and
// is freed once its alpha (+0x3c) or size runs out, its timer ends, or it leaves the level. Its
// colour +0x30.. (negative: red and green swap their fade rates) fades towards black.
// orig: 0x0026d700
void particle_19_update(uint32_t p) {
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    float r = lwc1(q + 0x10), g = lwc1(q + 0x14), b = lwc1(q + 0x18);
    int32_t neg = 0;
    if (r < 0.0f || g < 0.0f || b < 0.0f) {
        r = -r;
        g = -g;
        b = -b;
        neg = 1;
    }
    const float fs = lwc1(0x0015ed60);
    swc1(q + 0x1c, lwc1(q + 0x1c) + fs * 0.0f);
    swc1(p + 0xc, lwc1(p + 0xc) + fs * -100.0f);
    Vec4 to;
    vec3_add(to, V(p + 0x10), V(q));
    const bool still = lwc1(q) == 0.0f && lwc1(q + 4) == 0.0f && lwc1(q + 8) == 0.0f;
    if (!still && segment_collide_v(V(p + 0x10), to, 2, lwu(0x001413d0), nullptr) != 0) {
        V(p + 0x10) = V(OV(0x00173e60));
        vec_clear(V(q));
        swc1(p + 0xc, lwc1(p + 0xc) + lwc1(p + 0xc));
    } else if (!still) {
        V(p + 0x10) = to;
        if (lwc1(q + 0xc) == 0.0f) swc1(q + 8, lwc1(q + 8) + lwc1(0x0015ed64) * u2f(0xbbce703bu));
        else swc1(q + 8, lwc1(q + 8) + lwc1(q + 0xc));
    }
    const float lo = u2f(0x3c23d70au);
    if (lwc1(q + 0x1c) <= 0.0f || timer_tick_i16(h16(p + 0xa)) != 0 || lwc1(p + 0xc) < 0.0f || to.x < lo || to.y < lo
        || to.z < lo) {
        free_particle(p);
        return;
    }
    const float f = lwc1(0x0015ed60);
    const float slow = u2f(0xbc23d70au), fast = u2f(0xbcf5d788u);
    if (neg == 0) {
        r += f * slow;
        if (r < 0.0f) r = 0.0f;
        g += f * fast;
        if (g < 0.0f) g = 0.0f;
    } else {
        g += f * slow;
        if (g < 0.0f) g = 0.0f;
        r += f * fast;
        if (r < 0.0f) r = 0.0f;
    }
    b += f * u2f(0xbd4ccccdu);
    if (b < 0.0f) b = 0.0f;
    const float a = lwc1(q + 0x1c) + f * 0.0f;
    swc1(q + 0x1c, a);
    if (a < 0.0f) swc1(q + 0x1c, 0.0f);
    sw(p + 4, rgba_pack(r, g, b, lwc1(q + 0x1c)));
    if (neg > 0) {
        swc1(q + 0x10, -r);
        swc1(q + 0x14, -g);
        swc1(q + 0x18, -b);
    } else {
        swc1(q + 0x10, r);
        swc1(q + 0x14, g);
        swc1(q + 0x18, b);
    }
    sb(p + 8, lbu(p + 8) - 1);
}

// orig: (none) — registers the ported particle types for the update's indirect calls.
void particle_fns_register() {
    ovl_register_moby_fn(0x0026c7f0, particle_13_update);
    ovl_register_moby_fn(0x00267b10, particle_16_update);
    ovl_register_moby_fn(0x00271fc0, particle_20_update);
    ovl_register_moby_fn(0x0026ccb0, particle_15_update);
    ovl_register_moby_fn(0x00274a70, particle_45_update);
    ovl_register_moby_fn(0x00269290, particle_02_update);
    ovl_register_moby_fn(0x0026ac78, particle_0b_update);
    ovl_register_moby_fn(0x0026bca8, particle_0f_update);
    ovl_register_moby_fn(0x0026f280, particle_22_update);
    ovl_register_moby_fn(0x0026f660, particle_23_update);
    ovl_register_moby_fn(0x00273a68, particle_40_update);
    ovl_register_moby_fn(0x0026e150, particle_1c_update);
    ovl_register_moby_fn(0x002721f0, particle_35_update);
    ovl_register_moby_fn(0x00269a70, particle_04_update);
    ovl_register_moby_fn(0x0026b910, particle_0d_update);
    ovl_register_moby_fn(0x002719f0, particle_2e_update);
    ovl_register_moby_fn(0x00271be8, particle_2f_update);
    ovl_register_moby_fn(0x00270710, particle_29_update);
    ovl_register_moby_fn(0x00273088, particle_3b_update);
    ovl_register_moby_fn(0x00273800, particle_3e_update);
    ovl_register_moby_fn(0x00274308, particle_42_update);
    ovl_register_moby_fn(0x00274de0, particle_46_update);
    ovl_register_moby_fn(0x0026c558, particle_12_update);
    ovl_register_moby_fn(0x00271748, particle_2d_update);
    ovl_register_moby_fn(0x00273298, particle_3c_update);
    ovl_register_moby_fn(0x0026c088, particle_10_update);
    ovl_register_moby_fn(0x0026d1b0, particle_17_update);
    ovl_register_moby_fn(0x0026d700, particle_19_update);
}

}  // namespace rac1
