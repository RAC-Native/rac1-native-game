// Veldin's particle updates, part 2 of 2 (formerly particle_types_tr.cpp and particle_types4_tr.cpp,
// units veldin_particles and veldin_particles4): types 0x07, 0x27, 0x2a (with spawner 0x270758),
// 0x30, 0x32, 0x33, 0x36, 0x3a, 0x3d, 0x41, 0x43, 0x47, 0x49, 0x4b, 0x4c, 0x4f, and the alpha-step
// helper 0x259430. Declarations in particle_types_tr.h and particle_types4_tr.h.
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/particle_types4_tr.h>

#include <game/collision/collision.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/dev_271450.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/globals.h>
#include <game/ovl/metal.h>
#include <game/ovl/moby_joints.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_height_map.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_particle_kinds.h>

namespace rac1 {

namespace {
int16_t& h16(uint32_t a) { return *static_cast<int16_t*>(ee_ram_ptr(a)); }
int32_t& w32_ref(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }
int32_t div2_s32(int32_t x) {
    const int32_t adj = x >= 0 ? x : static_cast<int32_t>(static_cast<uint32_t>(x) + 1u);
    return adj >> 1;
}
}  // namespace

// Subtracts `step` from the signed top byte of RGBA word `*col_addr`, clamping at 0; returns 1 if 0.
// orig: 0x00259430
int32_t particle_alpha_dec(uint32_t col_addr, int32_t step) {
    const uint32_t col = lwu(col_addr);
    int32_t a = (static_cast<int32_t>(col) >> 24) - step;
    if (a < 0) a = 0;
    sw(col_addr, (col & 0x00ffffffu) | (static_cast<uint32_t>(a) << 24));
    return static_cast<uint32_t>(a) < 1u ? 1 : 0;
}

// Type 0x07: expanding puff fading its alpha over the last 6 scaled frames.
// orig: 0x0026a418
void particle_07_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i32(w32_ref(q)) != 0) {
        free_particle(p);
        return;
    }
    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), lwc1(q + 4)));
    if (lw(p + 0x20) < frame_scale_i(6)) {
        const int32_t fade = frame_scale_i(6);
        const int32_t cur = lw(p + 0x20);
        const int32_t a = ee_div((cur << 7) - cur, fade);
        sw(p + 4, (lwu(p + 4) & 0x00ffffffu) | (static_cast<uint32_t>(a) << 24));
    }
}

// Type 0x27: camera-offset animated sprite attached to moby *(p+0x24).
// orig: 0x0026fc38
void particle_27_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    swc1(p + 0xc, lwc1(OV(0x00160200)));
    sw(p + 4, lwu(OV(0x00160204)));
    sb(p + 8, (lbu(p + 8) + lbu(OV(0x0016020c))) & 0xffu);

    Vec4 off{};
    const uint32_t owner_pos = lwu(q + 4) + 0x10;
    vec3_sub(off, V(owner_pos), V(OV(0x00166dc0)));
    vec3_set_length(off, off, lwc1(OV(0x00160208)));
    vec3_add(V(p + 0x10), V(owner_pos), off);

    const int32_t frame = float_to_int(static_cast<float>(div2_s32(lw(q + 8))));
    sb(p + 2, lbu(lwu(OV(0x001b211c)) + static_cast<uint32_t>(frame)));

    const int32_t next = lw(q + 8) + 1;
    sw(q + 8, next < 31 ? static_cast<uint32_t>(next) : 0u);
    if (lh(p + 0xa) == 0) {
        free_particle(p);
    }
}

// Spawns a type 0x2a smoke/steam plume particle.
// orig: 0x00270758
uint32_t particle_2a_spawn(uint32_t pos, uint32_t vel, uint8_t flags, uint32_t owner) {
    const uint32_t p = alloc_particle(0x2a);
    if (p == 0) return 0;

    const uint32_t q = p + 0x20;
    lq_sq(p + 0x10, pos);
    lq_sq(q, vel);

    const float c = u2f(0x3f333333u);
    sw(p + 4, static_cast<uint32_t>(rgba_pack(c, c, c, u2f(0x3da3d70au))));
    sb(p + 1, 0);
    sb(p + 3, 0x44);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));

    const float rot = rand_range_f(0.0f, 1.0f);
    swc1(q + 0x10, rot);
    sb(p + 8, f2u(eer::cvt_w(fpu_mul(rot, 256.0f))));

    sb(p + 2, lbu(lwu(OV(0x001b20b0)) + (static_cast<uint32_t>(libc_rand()) & 7u)));
    sb(q + 0x18, flags);
    sh(q + 0x14, 0);
    sh(q + 0x16, 0);

    if (flags < 5u) {
        swc1(p + 0xc, rand_range_f(7000.0f, 10000.0f));
        const int32_t r = libc_rand();
        const int32_t base = static_cast<int32_t>(
            f2u(eer::cvt_w(fpu_div(8.0f, fpu_mul(lwc1(0x0015ed60), u2f(0x3ee2d0e5u))))));
        const int32_t mod = ee_div(base * 6, 7);
        sh(p + 0xa, static_cast<uint32_t>(base - ee_rem(r, mod)));
    } else {
        swc1(p + 0xc, 10000.0f);
        const uint32_t life =
            f2u(eer::cvt_w(fpu_div(8.0f, fpu_mul(lwc1(0x0015ed60), u2f(0x3ee2d0e5u)))));
        sh(p + 0xa, life);
    }
    sw(q + 0x1c, owner);
    return p;
}

// Type 0x2a: splitting, colliding smoke/steam plume with fade and spin.
// orig: 0x00270948
void particle_2a_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x50);
    const uint32_t q = p + 0x20;
    const uint32_t pos = p + 0x10;

    if (lh(p + 0xa) != 0 && timer_tick_i16(h16(p + 0xa)) != 0) {
        sb(q + 0x18, lbu(q + 0x18) | 0x88u);
        const float size = lwc1(p + 0xc);
        const float spread = fpu_div(size, 900000.0f);
        swc1(p + 0xc, fpu_mul(size, u2f(0x3f4ccccdu)));

        const float vel_jitter = u2f(0x3e4ccccdu);
        const float s0 = rand_range_f(0.5f, u2f(0x3f333333u));
        vec3_scale(V(sp), V(q), s0);

        const float neg_spread = -spread;
        swc1(sp + 0x10, fpu_add(lwc1(p + 0x10), rand_range_f(neg_spread, spread)));
        swc1(sp + 0x14, fpu_add(lwc1(p + 0x14), rand_range_f(neg_spread, spread)));
        swc1(sp + 0x18, fpu_add(lwc1(p + 0x18), rand_range_f(neg_spread, spread)));

        const float vx = lwc1(q + 0);
        swc1(sp + 0x0, fpu_add(lwc1(sp + 0x0), rand_range_f(fpu_mul(-vx, vel_jitter), fpu_mul(vx, vel_jitter))));
        const float vy = lwc1(q + 4);
        swc1(sp + 0x4, fpu_add(lwc1(sp + 0x4), rand_range_f(fpu_mul(-vy, vel_jitter), fpu_mul(vy, vel_jitter))));
        const float vz = lwc1(q + 8);
        swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), rand_range_f(fpu_mul(-vz, vel_jitter), 0.0f)));

        const uint32_t child = particle_2a_spawn(sp + 0x10, sp, 0x88, lwu(q + 0x1c));
        if (child != 0) {
            sh(child + 0xa, static_cast<uint32_t>(frame_scale_i(2)));
            swc1(child + 0xc, lwc1(p + 0xc));
        }

        const float s1 = rand_range_f(0.5f, u2f(0x3f333333u));
        vec3_scale(V(q), V(q), s1);

        const float nvx = lwc1(q + 0);
        swc1(q + 0, fpu_add(lwc1(q + 0), rand_range_f(fpu_mul(-nvx, vel_jitter), fpu_mul(nvx, vel_jitter))));
        const float nvy = lwc1(q + 4);
        swc1(q + 4, fpu_add(lwc1(q + 4), rand_range_f(fpu_mul(-nvy, vel_jitter), fpu_mul(nvy, vel_jitter))));
        const float nvz = lwc1(q + 8);
        swc1(q + 8, fpu_add(lwc1(q + 8), rand_range_f(fpu_mul(-nvz, vel_jitter), 0.0f)));

        swc1(p + 0x10, fpu_add(lwc1(p + 0x10), rand_range_f(neg_spread, spread)));
        swc1(p + 0x14, fpu_add(lwc1(p + 0x14), rand_range_f(neg_spread, spread)));
        swc1(p + 0x18, fpu_add(lwc1(p + 0x18), rand_range_f(neg_spread, spread)));
    }

    const uint32_t tex_tbl = lwu(OV(0x001b20b0));
    const uint32_t tex_idx =
        static_cast<uint32_t>(lh(p + 0xa) + lh(q + 0x16) + static_cast<int32_t>(lbu(p + 2) - lbu(tex_tbl))) & 7u;
    sb(p + 2, lbu(tex_tbl + tex_idx));

    vec3_add(V(sp), V(pos), V(q));
    if (frame_scale_i(3) < lh(q + 0x14)) {
        vec3_sub(V(sp + 0x10), V(pos), V(q));
    } else {
        lq_sq(sp + 0x10, pos);
    }
    vec3_scale(V(sp + 0x20), V(sp + 0x20), u2f(0x3c23d70au));

    bool hit = false;
    const uint8_t flags = lbu(q + 0x18);
    if ((flags & 8u) != 0) {
        if ((lwu(p + 4) & 0xff000000u) == 0) {
            free_particle(p);
            ee_stack_free(0x50);
            return;
        }
        sb(q + 0x18, flags | 8u);
        if (rand_mod(3) == 0) {
            hit = segment_collide_v(V(sp + 0x10), V(sp), 0, lwu(q + 0x1c), nullptr) > 0;
        }
    } else {
        sw(sp + 0x30, lwu(q + 0x1c));
        swc1(sp + 0x3c, 1.0f);
        lq_sq(sp + 0x20, q);
        sw(sp + 0x40, 1);
        sw(sp + 0x34, 1);
        if (rand_mod(3) == 0) {
            hit = segment_collide_v(
                      V(sp + 0x10), V(sp), 0, lwu(q + 0x1c),
                      static_cast<const uint32_t*>(ee_ram_ptr(sp + 0x20))) > 0;
        }
    }

    if (hit) {
        sb(q + 0x18, lbu(q + 0x18) | 8u);
    } else {
        lq_sq(pos, sp);
    }

    if ((lbu(q + 0x18) & 8u) != 0) {
        const float dt = lwc1(0x0015ed60);
        const float steps = static_cast<float>(
            static_cast<int32_t>(f2u(eer::cvt_w(fpu_div(8.0f, fpu_mul(dt, u2f(0x3ee2d0e5u)))))));
        sh(q + 0x16, static_cast<uint32_t>(lhu(q + 0x16) + 1u));
        const float dsize = fpu_mul(fpu_mul(fpu_div(260000.0f, steps), dt), 1.25f);
        const float nsize = fpu_sub(lwc1(p + 0xc), dsize);
        swc1(p + 0xc, nsize);
        if (nsize <= 0.0f) {
            free_particle(p);
            ee_stack_free(0x50);
            return;
        }
    } else {
        const float dt = lwc1(0x0015ed60);
        const float steps = static_cast<float>(
            static_cast<int32_t>(f2u(eer::cvt_w(fpu_div(8.0f, fpu_mul(dt, u2f(0x3ee2d0e5u)))))));
        const float dsize = fpu_mul(fpu_div(260000.0f, steps), dt);
        swc1(p + 0xc, fpu_add(lwc1(p + 0xc), dsize));
    }

    const float age = static_cast<float>(lh(q + 0x14));
    const float dt = lwc1(0x0015ed60);
    const float steps = static_cast<float>(
        static_cast<int32_t>(f2u(eer::cvt_w(fpu_div(8.0f, fpu_mul(dt, u2f(0x3ee2d0e5u)))))));
    const float a_max = u2f(0x3da3d70au);
    float alpha = fpu_add(fpu_mul(fpu_mul(fpu_div(a_max, steps), dt), fpu_div(age, steps)), a_max);
    if (alpha < 0.0f) alpha = 0.0f;
    if (a_max < alpha) alpha = a_max;
    if ((lbu(q + 0x18) & 8u) != 0) {
        const float denom = fpu_add(fpu_div(static_cast<float>(lh(q + 0x16)), 7.0f), 1.0f);
        alpha = fpu_div(alpha, denom);
    }
    const uint32_t a_byte = static_cast<uint32_t>(float_to_int(fpu_mul(alpha, 256.0f)));
    sw(p + 4, (lwu(p + 4) & 0x00ffffffu) | (a_byte << 24));

    swc1(q + 8, fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0x3c03126fu))));
    if (frame_scale_i(0x19) < lh(q + 0x14) || (lbu(q + 0x18) & 8u) != 0) {
        const float damp = u2f(0x3f733333u);
        swc1(q + 0, fpu_mul(lwc1(q + 0), damp));
        swc1(q + 4, fpu_mul(lwc1(q + 4), damp));
    }

    const uint8_t fl2 = lbu(q + 0x18);
    const float dir = (fl2 & 1u) != 0 ? 1.0f : -1.0f;
    if ((fl2 & 8u) != 0) {
        swc1(q + 8, fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0x3c03126fu))));
        const float cnt = static_cast<float>(lh(q + 0x16));
        const float drot = fpu_div(fpu_mul(dir, fpu_mul(lwc1(0x0015ed60), u2f(0xbc23d70au))), cnt);
        swc1(q + 0x10, fpu_add(lwc1(q + 0x10), drot));
    } else {
        const float drot = fpu_mul(dir, fpu_mul(lwc1(0x0015ed60), u2f(0xbc23d70au)));
        swc1(q + 0x10, fpu_add(lwc1(q + 0x10), drot));
    }

    float rot = lwc1(q + 0x10);
    if (1.0f < rot) {
        rot = fpu_sub(rot, 1.0f);
        swc1(q + 0x10, rot);
    }
    if (rot < 0.0f) {
        rot = fpu_add(rot, 1.0f);
        swc1(q + 0x10, rot);
    }
    sb(p + 8, f2u(eer::cvt_w(fpu_mul(rot, 256.0f))));
    ee_stack_free(0x50);
}

// Type 0x30: linear-motion spinning particle pulsing between two RGBA colours.
// orig: 0x00271c88
void particle_30_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    sb(p + 8, (lbu(p + 8) + lbu(q + 0x1c)) & 0xffu);
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));

    const float t = abs_f(fpu_sub(fpu_mul(static_cast<float>(lh(p + 0xa)), lwc1(q + 0x10)), 1.0f));
    sw(p + 4, rgba_lerp(lwu(q + 0x18), lwu(q + 0x14), t));
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
    }
}

// Type 0x32: falling droplet/debris particle growing if on texture 0x1b2148[0], freed below z = *(q+0x10).
// orig: 0x00271df8
void particle_32_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        swc1(q + 8, fpu_sub(lwc1(q + 8), lwc1(q + 0x18)));
    }
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    sb(p + 8, (lbu(p + 8) + lbu(q + 0x14)) & 0xffu);

    if (lbu(p + 2) == lbu(lwu(OV(0x001b2148)))) {
        const float grow = fpu_mul(fpu_mul(lwc1(OV(0x00160210)), lwc1(0x0015ed6c)), 210000.0f);
        swc1(p + 0xc, fpu_add(lwc1(p + 0xc), grow));
    }
    if (lwc1(p + 0x18) < lwc1(q + 0x10)) {
        free_particle(p);
    }
}

// Type 0x33: rising damped smoke puff fading alpha by 2 per tick and growing toward 210000.
// orig: 0x00271ec8
void particle_33_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x10);
    const uint32_t q = p + 0x20;
    dev_271450_1ff318(sp, q, u2f(0x3f733333u));
    swc1(q + 8, fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed70), 5.0f)));
    vec3_add_inplace(V(p + 0x10), V(q));

    const uint32_t col = lwu(p + 4) + 0xfe000000u;
    sw(p + 4, col);
    if (0x5effffffu < ((col & 0xff000000u) - 1u) || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        ee_stack_free(0x10);
        return;
    }
    const float sz = lwc1(p + 0xc);
    sb(p + 8, (lbu(p + 8) + 1u) & 0xffu);
    swc1(p + 0xc, fpu_add(sz, fpu_mul(fpu_sub(210000.0f, sz), u2f(0x3d8f5c29u))));
    ee_stack_free(0x10);
}

// Type 0x36: path-following projectile/flame damaging nearby mobys via mobys_in_sphere + moby_list_hit.
// orig: 0x00272438
void particle_36_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t q = p + 0x20;
    const uint32_t pos = p + 0x10;

    lq_sq(sp, pos);
    const float s = fpu_add(lwc1(q + 4), lwc1(q + 8));
    swc1(q + 4, s);
    path_point_even(lwu(p + 0x20), sp + 0x20, sp + 0x24, pos, s, lwc1(q + 0xc));

    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        ee_stack_free(0x30);
        return;
    }

    const float life = static_cast<float>(lh(p + 0xa));
    const float fade_len = fpu_mul(static_cast<float>(frame_scale_i(0x1e)), 0.25f);
    if (life < fade_len) {
        const float cur = static_cast<float>(lh(p + 0xa));
        const float c = u2f(0x3f333333u);
        const float a = fpu_div(fpu_mul(cur, u2f(0x3da3d70au)),
                                fpu_mul(static_cast<float>(frame_scale_i(0x1e)), 0.25f));
        sw(p + 4, static_cast<uint32_t>(rgba_pack(c, c, c, a)));
    }

    const uint32_t owner = lwu(q + 0x14);
    const int32_t count = mobys_in_sphere(pos, u2f(0x3e19999au), 0x10, owner, 0);
    lq_sq(sp + 0x10, pos);
    moby_list_hit(owner, sp + 0x10, coll_hit_list(), count, 0, NOT_ADDR(0x00810001u), 2, 1, 2.0f, 1.0f, 1.0f);
    ee_stack_free(0x30);
}

// Type 0x3a: 3-state flame jet (0: charge-up, 1: emitter spawning stage-2 puffs & damaging hero, 2: rising puff).
// orig: 0x00272bc0
void particle_3a_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x10);
    const uint32_t q = p + 0x20;
    const uint32_t pos = p + 0x10;

    switch (lbu(q + 0x14)) {
    case 0: {
        const float total = static_cast<float>(lbu(q + 0x11));
        const float t = fpu_div(fpu_sub(total, static_cast<float>(lh(p + 0xa))), total);
        sw(p + 4, rgba_lerp(0xff4080ffu, 0, t));
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            if (lbu(q + 0x10) == 1) {
                free_particle(p);
            } else {
                sb(q + 0x14, 1);
                sh(p + 0xa, lbu(q + 0x12));
            }
        }
        break;
    }
    case 1: {
        const uint32_t count = lbu(q + 0x13);
        for (uint32_t i = 0; i < count; ++i) {
            lq_sq(sp, pos);
            metal_25f8e0(sp, u2f(0x3d4ccccdu));
            float scale = 1.0f;
            if (lh(p + 0xa) < frame_scale_i(0xa)) {
                scale = fpu_div(static_cast<float>(lh(p + 0xa)), static_cast<float>(frame_scale_i(0xa)));
            }
            particle_3a_spawn(sp, 2, lbu(q + 0x11), lbu(q + 0x12), lbu(q + 0x13),
                              lbu(q + 0x15), lwu(q + 0x18), lwc1(q + 0x1c), scale);
        }
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            free_particle(p);
        }
        if (vec2_distance(V(pos), V(g::PLAYER + 0x80)) < u2f(0x3e99999au) &&
            lwc1(g::PLAYER + 0x88) < fpu_add(lwc1(p + 0x18), u2f(0x3f99999au))) {
            vec3_sub(V(sp), V(g::PLAYER + 0x80), V(pos));
            vec3_set_length(V(sp), V(sp), 1.0f);
            swc1(sp + 8, 1.0f);
            sub_00259bc8(lwu(g::PLAYER + 0x2080), lwu(q + 0x18), 1, pos, sp, lwc1(q + 0x1c));
        }
        break;
    }
    case 2: {
        vec3_add(V(pos), V(pos), V(q));
        const float total = static_cast<float>(lbu(q + 0x15));
        const float t = fpu_div(fpu_sub(total, static_cast<float>(lh(p + 0xa))), total);
        const uint32_t c0 = (static_cast<uint32_t>(lbu(q + 0x16)) << 24) | 0x00004060u;
        sw(p + 4, rgba_lerp(c0, 0x008fafffu, t));

        if (t < u2f(0x3f19999au)) {
            const float d = fpu_mul(fpu_mul(lwc1(0x0015ed60), u2f(0x3d3020c5u)), 210000.0f);
            swc1(p + 0xc, fpu_add(lwc1(p + 0xc), d));
        } else {
            const float d = fpu_mul(fpu_mul(lwc1(0x0015ed60), u2f(0x3da3d70au)), 210000.0f);
            swc1(p + 0xc, fpu_sub(lwc1(p + 0xc), d));
        }

        sb(p + 2, lbu(lwu(OV(0x001b2168)) + (static_cast<uint32_t>(libc_rand()) & 7u)));
        const int32_t idx = static_cast<int32_t>(p - lwu(OV(0x0016016c))) >> 6;
        sb(p + 8, (lbu(p + 8) + ((idx & 1) != 0 ? 1u : 0xffffffffu)) & 0xffu);
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            free_particle(p);
        }
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x10);
}

// Type 0x3d: animated glow tethered to a moby chain point, offset toward the camera.
// orig: 0x00273448
void particle_3d_update(uint32_t p) {
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    const uint32_t owner = lwu(p + 0x20);
    if (owner == 0 || static_cast<int8_t>(lbu(owner + 0x20)) < 0) {
        free_particle(p);
        return;
    }

    sh(q + 0xc, (lhu(q + 0xc) + 1u) & 0xfu);
    const float target_sz = 159600.0f;
    const float sz = lwc1(p + 0xc);
    if (sz < target_sz) {
        const float nsz = fpu_add(sz, fpu_mul(lwc1(0x0015ed60), u2f(0x45795fffu)));
        swc1(p + 0xc, target_sz < nsz ? target_sz : nsz);
    } else if (target_sz < sz) {
        const float nsz = fpu_sub(sz, fpu_mul(lwc1(0x0015ed60), u2f(0x46795fffu)));
        swc1(p + 0xc, nsz < target_sz ? target_sz : nsz);
    }

    const float rot = fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed60), u2f(0x3a83126fu)));
    swc1(q + 8, rot);
    sb(p + 8, static_cast<uint32_t>(float_to_int(fpu_mul(rot, 255.0f))));

    sb(p + 2, lbu(lwu(OV(0x001b2174)) + static_cast<uint32_t>(lh(q + 0xc))));
    if (lwc1(p + 0xc) <= target_sz || 0.0f < lwc1(q + 0x10)) {
        swc1(q + 4, fpu_add(lwc1(q + 4), lwc1(q + 0x10)));
    }

    const float max_a = lwc1(p + 0xc) <= target_sz ? u2f(0x3e99999au) : u2f(0x3e19999au);
    float a = lwc1(q + 4);
    if (max_a < a) {
        const float neg_step = -lwc1(q + 0x10);
        swc1(q + 0x10, neg_step);
        a = fpu_add(a, neg_step);
        swc1(q + 4, a);
    } else if (a < 0.0f) {
        free_particle(p);
        return;
    }

    const uint32_t a_byte = static_cast<uint32_t>(float_to_int(fpu_mul(a, 256.0f)));
    sw(p + 4, (lwu(p + 4) & 0x00ffffffu) + (a_byte << 24));

    const float pull_rate = fpu_mul(lwc1(0x0015ed60), u2f(0x3e333333u));
    const float pull = lwc1(q + 0x14);
    swc1(q + 0x14, fpu_add(pull, fpu_mul(fpu_sub(1.0f, pull), pull_rate)));

    Vec4 pt{};
    moby_chain_point_v(lwu(q), lh(q + 0xe), pt);
    Vec4 to_cam{};
    vec3_sub(to_cam, V(OV(0x00166dc0)), pt);
    vec3_set_length(to_cam, to_cam, lwc1(q + 0x14));
    vec3_add(V(p + 0x10), pt, to_cam);
}

// Type 0x41: colliding spark/ember shrinking and cooling through RGBA floats at q+0x10..q+0x1c.
// orig: 0x00273ee0
void particle_41_update(uint32_t p) {
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    const uint32_t pos = p + 0x10;
    const float dt = lwc1(0x0015ed60);

    swc1(q + 0x1c, fpu_add(lwc1(q + 0x1c), fpu_mul(dt, 0.0f)));
    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), fpu_mul(dt, -100.0f)));

    Vec4 next_pos{};
    vec3_add(next_pos, V(pos), V(q));

    if (lwc1(q + 0) != 0.0f || lwc1(q + 4) != 0.0f || lwc1(q + 8) != 0.0f) {
        if (segment_collide_v(V(pos), next_pos, 2, p, nullptr) != 0) {
            V(pos) = coll_hit_point();
            vec_clear(V(q));
            const float sz = lwc1(p + 0xc);
            swc1(p + 0xc, fpu_add(sz, sz));
        } else {
            V(pos) = next_pos;
            const float grav = lwc1(q + 0xc);
            if (grav != 0.0f) {
                swc1(q + 8, fpu_add(lwc1(q + 8), grav));
            } else {
                swc1(q + 8, fpu_add(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0xbbce703bu))));
            }
        }
    }

    const float eps = u2f(0x3c23d70au);
    if (lwc1(q + 0x1c) <= 0.0f || timer_tick_i16(h16(p + 0xa)) != 0 || lwc1(p + 0xc) < 0.0f ||
        next_pos.x < eps || next_pos.y < eps || next_pos.z < eps) {
        free_particle(p);
        return;
    }

    auto step_ch = [](uint32_t addr, float rate) {
        float v = fpu_add(lwc1(addr), fpu_mul(lwc1(0x0015ed60), rate));
        swc1(addr, v);
        if (v < 0.0f) swc1(addr, 0.0f);
    };
    step_ch(q + 0x10, u2f(0xbc23d70au));
    step_ch(q + 0x14, u2f(0xbcf5d788u));
    step_ch(q + 0x18, u2f(0xbd4ccccdu));
    step_ch(q + 0x1c, 0.0f);

    sw(p + 4, static_cast<uint32_t>(rgba_pack(lwc1(q + 0x10), lwc1(q + 0x14), lwc1(q + 0x18), lwc1(q + 0x1c))));
    sb(p + 8, (lbu(p + 8) - 1u) & 0xffu);
}

// Type 0x43: orbiting helical trail (mode 0 = fading trail segment, mode 1 = helix head, other = linked head).
// orig: 0x00274518
void particle_43_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t q = p + 0x20;
    const uint32_t pos = p + 0x10;

    if (lh(q + 0x1c) == 0) {
        if (particle_alpha_dec(p + 4, lh(q + 0x1a)) != 0) {
            free_particle(p);
        } else if (lwu(q + 0xc) != 0) {
            swc1(sp + 0, lwc1(q + 0));
            swc1(sp + 4, lwc1(q + 4));
            swc1(sp + 8, lwc1(q + 8));
            vec3_scale(V(sp), V(sp), static_cast<float>(lh(q + 0x18)));
            vec3_add(V(pos), V(lwu(q + 0xc) + 0x10), V(sp));
        }
        ee_stack_free(0x70);
        return;
    }

    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        ee_stack_free(0x70);
        return;
    }
    if (lh(p + 0xa) < frame_scale_i(0xa) && particle_alpha_dec(p + 4, 0x10) != 0) {
        free_particle(p);
        ee_stack_free(0x70);
        return;
    }

    const int32_t alpha_in = static_cast<int8_t>(lbu(p + 7));
    if (lh(q + 0x1c) == 1) {
        particle_43_spawn(0, pos, frame_scale_i(0xa), 0, static_cast<uint32_t>(alpha_in), 0, 0.0f, 0.0f);
    } else {
        swc1(sp + 0, lwc1(q + 0));
        swc1(sp + 4, lwc1(q + 4));
        swc1(sp + 8, lwc1(q + 8));
        const uint32_t child =
            particle_43_spawn(lwu(q + 0xc), pos, frame_scale_i(0xa), 0, static_cast<uint32_t>(alpha_in), sp, 0.0f, 0.0f);
        if (child != 0) {
            sh(child + 0x38, lhu(q + 0x18));
        }
    }

    if (lh(q + 0x1c) == 1) {
        lq_sq(pos, lwu(q + 0xc) + 0x10);
        const float r_major = 2.25f;
        const float r_minor = u2f(0x3e8f5c29u);
        const float a0 = lwc1(q + 0x10);
        const float c0 = fpu_mul(cos_vu(a0), r_major);
        const float s0 = fpu_mul(sin_vu(a0), r_major);
        swc1(sp + 8, c0);
        swc1(sp + 4, s0);
        sw(sp + 0, 0);

        const float a1 = lwc1(q + 0x14);
        const float c1 = fpu_mul(cos_vu(a1), r_minor);
        const float s1 = fpu_mul(sin_vu(a1), r_minor);
        sw(sp + 0x10, 0);
        swc1(sp + 0x18, c1);
        swc1(sp + 0x14, s1);

        vec_clear(V(sp + 0x20));
        swc1(sp + 0x24, lwc1(q + 0x10));
        euler_to_mat4_vu(M(sp + 0x30), V(sp + 0x20));
        mat4_transform(V(sp + 0x10), V(sp + 0x10), M(sp + 0x30));
        vec3_add(V(sp), V(sp), V(sp + 0x10));
        mat3_transform(V(sp), V(sp), M(lwu(q + 0xc) + 0xc0));
        vec3_add(V(pos), V(pos), V(sp));

        swc1(q + 0x10, angle_add(lwc1(q + 0x10), fpu_mul(lwc1(0x0015ed6c), u2f(0x3f75be0bu))));
        swc1(q + 0x14, angle_add(lwc1(q + 0x14), fpu_mul(lwc1(0x0015ed6c), u2f(0x408ba058u))));
    }
    ee_stack_free(0x70);
}

// Type 0x47: 3-phase drifting mote attracted to a target point when near the hero, with distance tint.
// orig: 0x00274e48
void particle_47_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t q = p + 0x20;
    const uint32_t pos = p + 0x10;

    if ((lwu(OV(0x0015f5cc)) & 1u) != 0) {
        sb(p + 8, (lbu(p + 8) + 1u) & 0xffu);
    }

    const float dist = vec3_distance(V(pos), V(g::PLAYER + 0xd0));
    if (dist < 5.0f) {
        const uint32_t target = lwu(q + 0x18);
        if (0.0f < lwc1(target + 8)) {
            vec3_sub(V(sp + 0x10), V(target), V(pos));
            if (0.5f < vec3_length(V(sp + 0x10))) {
                vec3_set_length(V(sp + 0x10), V(sp + 0x10), fpu_div(u2f(0x3dcccccd), dist));
                vec3_add(V(pos), V(pos), V(sp + 0x10));
            } else {
                vec3_set_length(V(sp + 0x10), V(sp + 0x10), u2f(0x3ba3d70au));
                dir_pack(sp + 0x10, p + 0x24);
            }
        }
    }

    float t = fpu_div(fpu_sub(2.5f, fpu_sub(dist, 0.5f)), 2.5f);
    if (1.0f < t) t = 1.0f;
    else if (t < 0.0f) t = 0.0f;
    sw(p + 4, rgba_lerp(lwu(q + 8), lwu(q + 0xc), t));

    if (lh(q + 0x10) != 0) {
        // Original writes 0 to hardware register 0x10000000 (T0_COUNT); no EE RAM effect.
        dir_unpack(V(sp), q);
        vec3_add(V(pos), V(pos), V(sp));
        const float ratio = fpu_div(static_cast<float>(lh(p + 0xa)), static_cast<float>(lh(q + 0x10)));
        const uint32_t col = lwu(p + 4);
        sw(p + 4, rgba_lerp(col & 0x00ffffffu, col, fpu_sub(1.0f, ratio)));
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            sh(q + 0x10, 0);
            sh(p + 0xa, lhu(q + 0x12));
        }
    } else if (lh(q + 0x12) != 0) {
        dir_unpack(V(sp + 0x10), q);
        dir_unpack(V(sp + 0x20), q + 4);
        const float ratio = fpu_div(static_cast<float>(lh(p + 0xa)), static_cast<float>(lh(q + 0x12)));
        vec3_lerp(V(sp), V(sp + 0x20), V(sp + 0x10), ratio);
        vec3_add(V(pos), V(pos), V(sp));
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            sh(q + 0x12, 0);
            sh(p + 0xa, lhu(q + 0x14));
        }
    } else {
        dir_unpack(V(sp), q + 4);
        vec3_add(V(pos), V(pos), V(sp));
        const uint32_t col = lwu(p + 4);
        const float ratio = fpu_div(static_cast<float>(lh(p + 0xa)), static_cast<float>(lh(q + 0x14)));
        sw(p + 4, rgba_lerp(col & 0x00ffffffu, col, ratio));
        if (timer_tick_i16(h16(p + 0xa)) != 0) {
            free_particle(p);
        }
    }
    ee_stack_free(0x30);
}

// Type 0x49: wind-blown falling raindrop/snowflake spawning a ground puff on landing.
// orig: 0x00275320
void particle_49_update(uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t q = p + 0x20;
    const uint32_t pos = p + 0x10;

    const float x = lwc1(p + 0x10);
    float ground_z = lwc1(q + 0xc);
    if (x < 0.0f) {
        free_particle(p);
        ee_stack_free(0x30);
        return;
    }
    const float y = lwc1(p + 0x14);
    if (y < 0.0f || 512.0f < x || 512.0f < y) {
        free_particle(p);
        ee_stack_free(0x30);
        return;
    }
    if (lwc1(p + 0x18) < ground_z) {
        lq_sq(sp, pos);
        ground_puff_at(sp);
        free_particle(p);
        ee_stack_free(0x30);
        return;
    }

    lq_sq(sp, q);
    const uint32_t ix = f2u(eer::cvt_w(x));
    const uint32_t iy = f2u(eer::cvt_w(y));
    swc1(sp + 0xc, 0.0f);
    vec3_add(V(pos), V(pos), V(sp));
    vec3_add(V(pos), V(pos), V(OV(0x00160250)));

    if (ix != f2u(eer::cvt_w(lwc1(p + 0x10))) || iy != f2u(eer::cvt_w(lwc1(p + 0x14)))) {
        lq_sq(sp + 0x10, pos);
        const float h = ground_height_at(V(sp + 0x10));
        const float min_h = lwc1(OV(0x00160260));
        if (h < min_h) {
            ground_z = min_h;
        } else {
            lq_sq(sp + 0x20, pos);
            ground_z = ground_height_at(V(sp + 0x20));
        }
        if (lwc1(p + 0x18) < ground_z) {
            lq_sq(sp + 0x10, pos);
            ground_puff_at(sp + 0x10);
            free_particle(p);
            ee_stack_free(0x30);
            return;
        }
    }

    sw(sp + 0, 0);
    sw(sp + 4, 0);
    swc1(sp + 8, -fpu_mul(lwc1(0x0015ed70), 0.5f));
    sw(sp + 0xc, 0);
    vec3_add(V(q), V(sp), V(q));
    swc1(q + 0xc, fpu_add(ground_z, lwc1(OV(0x00160258))));
    ee_stack_free(0x30);
}

// Type 0x4b: spinning damped particle with gravity, growing until timer expires then fading out.
// orig: 0x002756f0
void particle_4b_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    sb(p + 8, (lbu(p + 8) + lbu(q + 0x12)) & 0xffu);
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    swc1(q + 8, fpu_sub(lwc1(q + 8), fpu_mul(lwc1(0x0015ed64), u2f(0x3b449ba6u))));

    const float spd = vec2_length(V(q));
    const float k = fpu_div(fpu_add(fpu_mul(fpu_sub(fpu_mul(spd, u2f(0x3f7851ecu)), 1.0f), lwc1(0x0015ed60)), 1.0f), spd);
    swc1(q + 0, fpu_mul(lwc1(q + 0), k));
    swc1(q + 4, fpu_mul(lwc1(q + 4), k));

    if (timer_tick_i16(h16(p + 0xa)) == 0) {
        const float growth = lwc1(q + 0x14);
        const float nsz = fpu_add(lwc1(p + 0xc), fpu_add(growth, growth));
        swc1(p + 0xc, nsz < 15000.0f ? 15000.0f : nsz);
    } else if (particle_alpha_dec(p + 4, lw(q + 0x1c)) != 0) {
        sw(p + 0xc, 0);
        free_particle(p);
    }
}

// Type 0x4c: stationary particle fading by global 0x160240 until its timer expires.
// orig: 0x00275810
void particle_4c_update(uint32_t p) {
    particle_alpha_dec(p + 4, lw(OV(0x00160240)));
    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
    }
}

// Type 0x4f: moving (or owner-attached) particle fading alpha linearly to zero over its remaining life.
// orig: 0x00275d68
void particle_4f_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    const uint32_t owner = lwu(q + 0x1c);
    if (owner != 0) {
        swc1(q + 0, fpu_add(lwc1(q + 0), lwc1(q + 0x10)));
        swc1(q + 4, fpu_add(lwc1(q + 4), lwc1(q + 0x14)));
        swc1(q + 8, fpu_add(lwc1(q + 8), lwc1(q + 0x18)));
        vec3_add(V(p + 0x10), V(owner + 0x10), V(q));
    } else {
        swc1(p + 0x10, fpu_add(lwc1(p + 0x10), lwc1(q + 0x10)));
        swc1(p + 0x14, fpu_add(lwc1(p + 0x14), lwc1(q + 0x14)));
        swc1(p + 0x18, fpu_add(lwc1(p + 0x18), lwc1(q + 0x18)));
    }

    if (timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }

    const int32_t life = lh(p + 0xa);
    const float t = fpu_div(static_cast<float>(life - 1), static_cast<float>(life));
    const uint32_t col = lwu(p + 4);
    const uint32_t base = lwu(q + 0x1c) != 0 ? (col & 0x00ffffffu) : 0u;
    sw(p + 4, rgba_lerp(base, col, t));
}

}  // namespace rac1
