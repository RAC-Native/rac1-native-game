// Ported overlay code: shared helpers of the weapons' effects: the limited angle
// seek, the moby list removal, the contact record initialiser, the type-8 glow particle, the floor
// light registration, and the generic explosion effect 0x25e450 (debris, smoke and spark particles,
// shards, a camera shake and a sound, from the 18 arguments of the weapons' blasts).
#include <game/math/vecmath.h>
#include <game/enhance/fixes102.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

// The angle `cur` turned toward `target` by at most `limit` (the shortest way): d = target - cur
// wrapped; the step is +limit (-limit when d is not positive), cut to d when it is shorter.
// orig: 0x00200510
float angle_seek_limited(float cur, float target, float limit) {
    const float d = angle_sub(target, cur);
    float step = limit;
    if (!(0.0f < d)) step = -step;
    if (abs_f(d) < step) step = abs_f(d);
    else if (step < -abs_f(d)) step = -abs_f(d);
    return angle_add(cur, step);
}

// Takes the moby `m` out of the u16 list `list` (+0 the count, then entries: the moby's offset
// from the moby array 0x15ffd8 in 0x100 units): the last entry takes its place.
// orig: 0x0025f800
void moby_list_remove(uint32_t m, uint32_t list) {
    const int32_t n = lh(list);
    if (n <= 0) return;
    uint32_t at = list + 2;
    for (int32_t i = 1;; ++i, at += 2) {
        if (lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(static_cast<int32_t>(lh(at))) << 8) == m) {
            const uint32_t last = list + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(list)))) * 2;
            sh(at, lhu(last));
            sh(list, lhu(list) - 1u);
            return;
        }
        if (lh(list) < i + 1) return;
    }
}

// A contact record template at `rec`: the vector +0 cleared, +0x10 the owner, +0x14 the flags,
// +0x1c a float, +0x20 cleared.
// orig: 0x00259858
void hit_rec_init(uint32_t rec, uint32_t owner, uint32_t flags, float f) {
    sw(rec + 0x10, owner);
    sw(rec + 0x14, flags);
    swc1(rec + 0x1c, f);
    sw(rec + 0x20, 0);
    vec_clear(V(rec));
}

// A type 8 particle (a glow: colour `color`, +0x30 / +0x34 the colours, size from `size` over
// `life` frames) at `pos` moving by `vel`; none without a life.
// orig: 0x0026a4c8
void spawn_particle_08(uint32_t pos, uint32_t vel, uint32_t color, uint32_t color2, int32_t life, float size) {
    if (life == 0) return;
    const uint32_t p = alloc_particle(8);
    if (p == 0) return;
    V(p + 0x10) = V(pos);
    const uint32_t q = p + 0x20;
    sw(p + 4, color);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) - 0x60));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    sw(p + 0xc, 0);
    sb(p + 8, 0);
    sb(p + 2, lbu(lwu(OV(0x001b20a0))));
    V(q) = V(vel);
    sh(p + 0xa, static_cast<uint32_t>(life));
    sw(q + 0x18, static_cast<uint32_t>(life));
    swc1(q + 0x1c, fpu_div(size, int_to_float(life)));
    sw(q + 0x14, color2);
    sw(q + 0x10, color);
}

// Registers a floor light for the moby `m` (when its +0x31 is set and fewer than 0x20 are
// registered, count 0x15f434, entries of 0x20 at 0x16e080): its place, the floor probed 0.5 below
// (+8 the height 0.025 above, +0x10 the normal 0x173e80), +0xc `scale` times (8 - its height above the
// floor) / 8, at least 0.25.
// orig: 0x00259fe8
void floor_light_add(uint32_t m, float scale) {
    if (lbu(m + 0x31) == 0) return;
    if (!(lw(OV(0x0015f434)) < 0x20)) return;
    const uint32_t base = OV(0x0016e080);
    uint32_t e = base + (static_cast<uint32_t>(lw(OV(0x0015f434))) << 5);
    V(e) = V(m + 0x10);
    const float h = floor_probe_v(0.5f, V(m + 0x10), 0);
    if (sub_001f35d8() == 0) return;
    swc1(e + 8, h + u2f(0x3ccccccdu));
    V(e + 0x10) = V(OV(0x00173e80));
    const float above = abs_f(lwc1(m + 0x18) - h);
    float k = (8.0f - above) * u2f(0x3e000000u);
    if (k < u2f(0x3e800000u)) k = u2f(0x3e800000u);
    const uint32_t n = static_cast<uint32_t>(lw(OV(0x0015f434)));
    k = scale * k;
    e = base + (n << 5);
    sw(OV(0x0015f434), n + 1);
    swc1(e + 0xc, k);
}

// The generic blast of the weapons: at `pos` (null: the moby `src`'s place) `n1` debris sprays
// (particle 0xf, colours 0x4f007fff / 0x1f00007f swapped by `G`), `E` debris mobys 0x7a (a third
// kept) flung out (when the camera is within 14), `A` sparks (particles 0xb) and `B` smoke puffs
// (particle 8) in the colour tables 0x1e9030 / 0x1e9048, shards (0x2ac910) of the sizes `f27` / `f28`
// when `src` is set, a screen shake (0x166de0 / 0x166de8) when on screen, a moby sound `C`, and a
// `f26`-sized effect (0x1b0250, or 0x1b02a0 with G) when `F` is 0; a sphere of `radius` around
// `pos` hits the mobys as the contact record `rec` (owner `src`, flags 0x810001, f13).
// orig: 0x0025e450
void explosion_effect(uint32_t src, uint32_t a1_unused, uint32_t pos, int32_t n1, int32_t A, int32_t B, int32_t C, int32_t D,
                      int32_t E, int32_t F, uint32_t G, float radius, float f13, float f27, float f28, float f29, float S,
                      float f26) {
    (void)a1_unused;
    float f3 = lwc1(OV(0x0015f5d4)) + lwc1(OV(0x0015f5d0));
    Vec4 sp0{};  // the debris sparks' velocity: straight up
    sp0.z = (lwc1(0x0015ed6c) * 8.0f) * S;
    Vec4 sp10 = sp0;
    if (F == -1) {
        F = 0;
        if (u2f(0x3fd9999au) < f3) F = 1;
    }
    uint32_t s5 = pos;
    if (s5 == 0) {
        if (src == 0) return;
        s5 = src + 0x10;
    }
    if (0.0f < radius) {
        const uint32_t rec = ee_stack_alloc(0x30);
        hit_rec_init(rec, src, 0x810001u, f13);
        sb(rec + 0x18, 2);
        sb(rec + 0x19, 1);
        sh(rec + 0x1a, lhu(src + 0xa6));
        (void)mobys_in_sphere(s5, radius, 0x10, src, rec);
        ee_stack_free(0x30);
    }
    if (n1 > 0) {
        const int32_t scale_life = F * 10;
        for (int32_t s3 = n1; s3 != 0;) {
            const float f22 = S * u2f(0x471c4000u);
            --s3;
            const float pitch = rand_range_f(u2f(0x3e860a92u), u2f(0x3fb2b8c2u));
            const float yaw = rand_angle();
            const float r = rand_range_f(u2f(0x40e00000u), u2f(0x41280000u));
            Vec4 v20{};
            vec_from_spherical_v(v20, S * (r * lwc1(0x0015ed6c)), yaw, pitch);
            v20.z = v20.z + lwc1(0x0015ed6c) * 3.0f;
            const uint32_t c0 = color_swap_channels(0x4f007fffu, G);
            const uint32_t c1 = color_swap_channels(0x1f00007fu, G);
            const int32_t a = frame_scale_i(0x3c);
            const int32_t b = frame_scale_i(0x78);
            const int32_t life = rand_range(a, b);
            const uint32_t vel = ee_stack_alloc(0x10);
            V(vel) = v20;
            const uint32_t posv = s5;
            (void)spawn_particle_0f(posv, vel, c0, c1, static_cast<uint32_t>(life - scale_life), 1u, -1, -1, f22);
            ee_stack_free(0x10);
        }
    }
    vec3_sub(sp10, V(OV(0x00166dc0)), V(s5));
    const float dist = vec3_length(sp10);
    if (E != 0) {
        Vec4 sp30{};
        sp30.x = rand_range_f(-1.0f, 1.0f);
        sp30.y = rand_range_f(-1.0f, 1.0f);
        sp30.z = rand_range_f(-1.0f, 1.0f);
        Vec4 sp20 = sp30;
        if (dist < u2f(0x41600000u)) {
            const float f12 = fpu_div(dist, u2f(0x40a00000u));
            const float zadd = dist * 0.5f;
            sp10.z = sp10.z + zadd;
            vec3_set_length(sp20, sp20, f12 * lwc1(0x0015ed6c));
            vec3_set_length(sp10, sp10, (dist + dist) * lwc1(0x0015ed6c));
            vec3_add(sp20, sp20, sp10);
            vec3_clamp_length_vu(sp20, sp20, lwc1(0x0015ed6c) * u2f(0x41200000u));
            const int32_t a = frame_scale_i(0x3c);
            const int32_t b = frame_scale_i(0x5a);
            const int32_t life = rand_range(a, b);
            (void)debris_7a_spawn_v(V(s5), sp20, static_cast<uint32_t>(life), 0u, 0u);
        }
        int32_t s8 = E - 1;
        if (fixes102_on(2)) {  // (1.02 Fixes, fix 2: the 1.02 build, 0x25f4a8, leaves 10 moby slots free, 0x15ffbc)
            const int32_t free_slots = lw(OV(0x0015ffbc));
            if (free_slots < 0xb) s8 = 0;
            else if (free_slots - 0xa < s8) s8 = free_slots - 0xa;
        }
        if (s8 > 0) {
            int32_t i = 0;
            do {
                uint32_t keep = 1;
                const float pitch = rand_range_f(u2f(0x3e860a92u), u2f(0x3fa78d36u));
                const float yaw = rand_angle();
                const float r = rand_range_f(u2f(0x40e00000u), u2f(0x41280000u));
                vec_from_spherical_v(sp30, S * (r * lwc1(0x0015ed6c)), yaw, pitch);
                const int32_t rem = ee_rem(i, 3);
                sp30.z = sp30.z + (lwc1(0x0015ed6c) + lwc1(0x0015ed6c));
                ++i;
                if (rem != 0) keep = 0;
                const int32_t a = frame_scale_i(0x3c);
                const int32_t b = frame_scale_i(0x5a);
                const int32_t life = rand_range(a, b);
                (void)debris_7a_spawn_v(V(s5), sp30, static_cast<uint32_t>(life), keep, 0u);
            } while (i < s8);
        }
    }
    // the sparks (particle 0xb): `A`, fewer when the camera is close (its distance halved)
    int32_t sparks = A;
    if (dist < int_to_float(A) + int_to_float(A)) {
        const int32_t v = float_to_int(dist);
        sparks = (v + static_cast<int32_t>(static_cast<uint32_t>(v) >> 31)) >> 1;
    }
    float near_k = 0.0f;
    if (dist < u2f(0x40e00000u)) near_k = u2f(0x40e00000u) - dist;
    if (sparks > 0) {
        const uint32_t ta = OV(0x001e9030), tb = OV(0x001e9048);
        const int32_t F3 = F * 3, F2 = F * 2, F5 = F * 5;
        for (int32_t s4 = sparks; s4 != 0;) {
            const float r = rand_range_f(u2f(0x41000000u), u2f(0x41200000u));
            --s4;
            float f20 = r * lwc1(0x0015ed6c);
            const float f1 = near_k * lwc1(0x0015ed6c);
            f20 = f20 - f1;
            const uint32_t ia = static_cast<uint32_t>(rand_mod(6));
            f20 = f20 * S;
            const uint32_t s3 = color_swap_channels(lwu(ta + ia * 4), G);
            const uint32_t ib = static_cast<uint32_t>(rand_mod(6));
            const uint32_t s2 = color_swap_channels(lwu(tb + ib * 4), G);
            const float f21 = S * u2f(0x48c35000u);
            {
                const int32_t a = frame_scale_i(0xf);
                const int32_t b = frame_scale_i(0x14);
                const int32_t s1 = rand_range(a, b) - F3;
                const int32_t c = frame_scale_i(0x1e);
                const int32_t d = frame_scale_i(0x2d);
                const int32_t t1 = rand_range(c, d) - F5;
                spawn_particle_0b_v(V(s5), sp0, s3, s2, static_cast<uint32_t>(s1), static_cast<uint32_t>(t1), 0, 0, f21, f20);
            }
            {
                const int32_t a = frame_scale_i(5);
                const int32_t b = frame_scale_i(10);
                const int32_t s1 = rand_range(a, b) - F2;
                const int32_t c = frame_scale_i(0xf);
                const int32_t d = frame_scale_i(0x14);
                const int32_t t1 = rand_range(c, d) - F3;
                spawn_particle_0b_v(V(s5), sp0, 0x7fffffffu, 0x00ffffffu, static_cast<uint32_t>(s1), static_cast<uint32_t>(t1), 0, 0, f21, f20 * 0.5f);
            }
        }
    }
    // the smoke puffs (particle 8)
    if (B > 0) {
        const uint32_t ta = OV(0x001e9030), tb = OV(0x001e9048);
        const int32_t F5 = F * 5;
        for (int32_t s4 = B; s4 != 0;) {
            Vec4 sp80{};
            sp80.x = rand_range_f(-1.0f, 1.0f);
            --s4;
            sp80.y = rand_range_f(-1.0f, 1.0f);
            sp80.z = rand_range_f(-1.0f, 1.0f);
            Vec4 sp70 = sp80;
            const float rr = rand_range_f(0.0f, u2f(0x40400000u));
            vec3_set_length(sp70, sp70, S * (rr * lwc1(0x0015ed6c)));
            const uint32_t ia = static_cast<uint32_t>(rand_mod(6));
            const uint32_t s2 = color_swap_channels(lwu(ta + ia * 4), G);
            const uint32_t ib = static_cast<uint32_t>(rand_mod(6));
            const uint32_t s1 = color_swap_channels(lwu(tb + ib * 4), G);
            const int32_t a = frame_scale_i(0x1e);
            const int32_t b = frame_scale_i(0x2d);
            const int32_t life = rand_range(a, b) - F5;
            const uint32_t vel = ee_stack_alloc(0x10);
            V(vel) = sp70;
            spawn_particle_08(s5, vel, s2, s1, life, u2f(0x48435000u));
            ee_stack_free(0x10);
        }
    }
    if (src != 0) {
        if (0.0f < f27) {
            uint32_t t0 = G != 0 ? 0x3cu : 0x7fu;
            int32_t s1 = 0x96, s0 = 0x96;
            if (G != 0) {
                s1 = 0x46;
                s0 = 0x46;
            }
            const uint32_t s2 = t0 & 0xffu;
            if (lwc1(OV(0x0015f5d0)) < u2f(0x3f733333u) && f29 < dist) {
                (void)shard_spawn_v(src, V(s5), sp0, static_cast<uint32_t>(frame_scale_i(0x10)), static_cast<uint32_t>(s1), 0x96u, static_cast<uint32_t>(s0), 0x20u, f27);
                (void)shard_spawn_v(src, V(s5), sp0, static_cast<uint32_t>(frame_scale_i(0x16)), s2, 0x7fu, 0x50u, 0x20u, f27);
            }
            (void)shard_spawn_v(src, V(s5), sp0, static_cast<uint32_t>(frame_scale_i(0x1e)), s2, 0x7fu, 0u, 0x30u, f27);
        }
        if (0.0f < f28) (void)shard_spawn_v(src, V(s5), sp0, static_cast<uint32_t>(frame_scale_i(0x1b)), 0xffu, 0xffu, 0xffu, 0x20u, f28);
    }
    if (D != 0) {
        Vec4 sp20 = V(s5);
        sp20.w = 2.0f;
        if (point_on_screen_v(sp20, u2f(0x41200000u)) != -1) {
            if (dist < u2f(0x41a00000u)) swc1(OV(0x00166de0), u2f(0x3ecccccdu) - dist * u2f(0x3c8f5c29u));
            else swc1(OV(0x00166de0), u2f(0x3d4cccd0u));
            sw(OV(0x00166de8), static_cast<uint32_t>(frame_scale_i(0x19)));
        }
    }
    if (src != 0 && C != -1) (void)moby_sound_start(C, 0, src);
    if (f26 == 0.0f) return;
    if (F != 0) return;
    {
        Vec4 sp20 = V(s5);
        sp20.w = f26;
        if (point_on_screen_v(sp20, u2f(0x42c80000u)) == -1) return;
    }
    const uint32_t def = OV(0x001b0250);
    if (0.0f < f26) {
        swc1(def + 0x20, f26);
        swc1(def + 0x24, f26);
        swc1(def + 0x28, f26);
    } else {
        swc1(def + 0x20, u2f(0x41700000u));
        swc1(def + 0x24, u2f(0x41700000u));
        swc1(def + 0x28, u2f(0x41700000u));
    }
    if (G != 0) (void)effect_spawn(OV(0x001b02a0), s5, 0, 0);
    else (void)effect_spawn(def, s5, 0, 0);
}

}  // namespace rac1
