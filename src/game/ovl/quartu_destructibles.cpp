// Quartu (level 15) Giant Clank destructible factory structures, ported by hand from the level's own code:
// - class 148 (L15:0x002a7880): destructible pillar / tower (shatters into rocks, cap piece if class 0x94, and dust)
// - class 154 (L15:0x002aa280): destructible wall / barrier oriented along moby yaw
// The original's order of calls and float operations is preserved.
#include <game/ovl/quartu_mobys.h>

#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {

uint32_t find_giant_hit(uint32_t m) {
    const uint32_t base = L15_ADDR(0x00178700u);
    for (uint32_t i = 0; i < 64; ++i) {
        const uint32_t e = base + (i << 6);
        if (lwu(e + 0x34) == m && (lwu(e + 0x24) & 0x80000u) != 0) {
            return e;
        }
    }
    return 0;
}

bool persistent_bit_set(uint32_t m) {
    const int32_t uid = static_cast<int32_t>(lh(m + 0xb2));
    const uint32_t lvl = lwu(0x0015ed84u);
    const uint32_t word_addr = 0x0014c190u + (lvl << 8) + static_cast<uint32_t>((uid >> 5) << 2);
    return ((lwu(word_addr) >> (uid & 0x1f)) & 1u) != 0;
}

}  // namespace

// Destructible pillar / tower (class 148).
// orig: L15:0x002a7880
void qua_class148_update_l15(uint32_t m) {
    if (lbu(0x001413f4u) != 2 && lbu(m + 0x20) == 1) return;

    const uint32_t hit = find_giant_hit(m);
    sb(m + 0xa4, 0xff);
    if (hit == 0) {
        if (lbu(m + 0x20) == 0) {
            sb(m + 0x30, 0xff);
            sb(m + 0x20, 1);
            sh(m + 0x32, 0x80);
            const float r = lwc1(m + 0x2c);
            swc1(m + 0x2c, fpu_add(r, r));
            if (lbu(0x0013dd50u) != 0 && persistent_bit_set(m)) {
                moby_retire(m);
            }
        }
        return;
    }

    const uint32_t sp = ee_stack_alloc(384);
    const uint32_t vel_buf = sp + 0x20;
    const uint32_t base_vel = sp + 0x30;
    const uint32_t rock_bias = sp + 0x40;
    const uint32_t pos_buf = sp + 0x50;
    const uint32_t origin = sp + 0x60;
    const uint32_t dir_buf = sp + 0x80;
    const uint32_t pdir_buf = sp + 0x90;

    vec3_set_length(V(base_vel), V(hit + 0x10), fpu_mul(lwc1(0x0015ed6cu), 10.0f));
    vec3_scale(V(rock_bias), V(base_vel), 2.0f);
    if (vec3_length(V(hit)) == 0.0f) {
        lq_sq(origin, m + 0x10);
        swc1(origin + 0x8, fpu_add(lwc1(origin + 0x8), 6.0f));
    } else {
        lq_sq(origin, hit);
    }

    for (int i = 0; i < 25; ++i) {
        float rad = rand_range_f(0.5f, 1.5f);
        const float ang = rand_angle();
        if (lh(m + 0xa6) == 0x94) {
            rad = fpu_mul(rad, 3.0f);
        }
        swc1(pos_buf + 0x0, fpu_mul(cos_vu(ang), rad));
        swc1(pos_buf + 0x4, fpu_mul(sin_vu(ang), rad));
        swc1(pos_buf + 0x8, rand_range_f(1.0f, 7.0f));
        vec3_add_inplace(V(pos_buf), V(m + 0x10));
        vec3_sub(V(dir_buf), V(origin), V(pos_buf));
        lq_sq(vel_buf, dir_buf);
        vec3_set_length(V(vel_buf), V(vel_buf), fpu_mul(lwc1(0x0015ed6cu), 5.0f));
        vec3_add_inplace(V(vel_buf), V(rock_bias));
        swc1(vel_buf + 0x8, fpu_add(lwc1(vel_buf + 0x8), fpu_mul(lwc1(0x0015ed6cu), 3.0f)));
        vec3_scale(V(vel_buf), V(vel_buf), rand_range_f(0.75f, 1.25f));
        const uint32_t rock_cls = lwu(L15_ADDR(0x00161568u) + (static_cast<uint32_t>(rand_mod(4)) << 2));
        const float scale = rand_range_f(u2f(0x3dcccccd), u2f(0x3e99999a));
        const int32_t life = rand_range(0x3c, 0x64);
        const float gravity = rand_range_f(3.0f, 7.0f);
        rock_spawn(pos_buf, vel_buf, rock_cls, life, 1, scale, gravity, u2f(0x3e19999a), 2.0f);  // (spin 0.15, wait 2 s)
    }

    if (lh(m + 0xa6) == 0x94) {
        lq_sq(pos_buf, m + 0x10);
        swc1(pos_buf + 0x8, fpu_add(lwc1(pos_buf + 0x8), 10.0f));
        vec3_sub(V(dir_buf), V(origin), V(pos_buf));
        lq_sq(vel_buf, dir_buf);
        vec3_set_length(V(vel_buf), V(vel_buf), fpu_mul(lwc1(0x0015ed6cu), 5.0f));
        vec3_add_inplace(V(vel_buf), V(rock_bias));
        swc1(vel_buf + 0x8, fpu_add(lwc1(vel_buf + 0x8), fpu_mul(lwc1(0x0015ed6cu), 3.0f)));
        vec3_scale(V(vel_buf), V(vel_buf), rand_range_f(0.75f, 1.25f));
        const int32_t life = rand_range(0x3c, 0x64);
        const uint32_t cap = rock_spawn(pos_buf, vel_buf, 0xc9, life, 1, 1.0f, 5.0f, 1.0f, 2.0f);
        if (cap != 0) {
            vec3_scale(V(cap + 0x40), V(cap + 0x40), u2f(0x3eb33333));
        }
    }

    vec_clear(V(vel_buf));
    explosion_effect(m, vel_buf, origin, 0x14, 3, 4, 0, 1, 1, -1, 0,
                     0.0f, 0.0f, 10.0f, 5.0f, 100000.0f, 3.0f, 15.0f);

    for (int i = 0; i < 20; ++i) {
        float rad = rand_range_f(0.0f, 1.5f);
        const float ang = rand_angle();
        if (lh(m + 0xa6) == 0x94) {
            rad = fpu_mul(rad, 3.0f);
        }
        swc1(pos_buf + 0x0, fpu_mul(cos_vu(ang), rad));
        swc1(pos_buf + 0x4, fpu_mul(sin_vu(ang), rad));
        sw(pos_buf + 0x8, 0);
        vec3_add(V(pos_buf), V(pos_buf), V(m + 0x10));
        swc1(pos_buf + 0x8, fpu_add(lwc1(pos_buf + 0x8), rand_range_f(1.0f, 5.5f)));
        vec3_sub(V(pdir_buf), V(origin), V(pos_buf));
        lq_sq(vel_buf, pdir_buf);
        vec3_set_length(V(vel_buf), V(vel_buf), fpu_mul(lwc1(0x0015ed6cu), 5.0f));
        vec3_add_inplace(V(vel_buf), V(base_vel));
        vec3_scale(V(vel_buf), V(vel_buf), rand_range_f(0.75f, 1.25f));
        const uint32_t col0 = rgba_lerp_ovl(0x7f000000u, 0x7f182030u, rand_range_f(0.25f, 1.0f));
        const uint32_t col1 = rgba_lerp_ovl(0u, 0x005f5f5fu, rand_range_f(0.5f, 1.0f));
        const float size = fpu_mul(rand_range_f(1.0f, 3.0f), 210000.0f);
        const uint32_t life = static_cast<uint32_t>(frame_scale_i(0xb4));
        const uint32_t tex = lwu(L15_ADDR(0x00161578u) + (static_cast<uint32_t>(rand_mod(3)) << 2));
        const uint32_t pt = spawn_particle_10(pos_buf, vel_buf, col0, col1, life, tex, size);
        if (pt != 0) {
            swc1(pt + 0x2c, fpu_sub(lwc1(m + 0x18), 0.5f));
        }
    }

    moby_drop_bolts(m, 0, -1);
    moby_retire(m);
    ee_stack_free(384);
}

// Destructible wall / barrier (class 154).
// orig: L15:0x002aa280
void qua_class154_update_l15(uint32_t m) {
    if (lbu(0x001413f4u) != 2 && lbu(m + 0x20) == 1) return;

    const uint32_t hit = find_giant_hit(m);
    sb(m + 0xa4, 0xff);
    if (hit == 0) {
        if (lbu(m + 0x20) == 0) {
            sh(m + 0x32, 0x80);
            sb(m + 0x30, 0xff);
            sb(m + 0x20, 1);
            if (lbu(0x0013dd50u) != 0 && persistent_bit_set(m)) {
                moby_retire(m);
            }
        }
        return;
    }

    const uint32_t sp = ee_stack_alloc(400);
    const uint32_t vel_buf = sp + 0x20;
    const uint32_t base_vel = sp + 0x30;
    const uint32_t rock_bias = sp + 0x40;
    const uint32_t pos_buf = sp + 0x50;
    const uint32_t origin = sp + 0x60;
    const uint32_t dir_buf = sp + 0x80;
    const uint32_t pdir_buf = sp + 0x90;

    vec3_set_length(V(base_vel), V(hit + 0x10), fpu_mul(lwc1(0x0015ed6cu), 10.0f));
    vec3_scale(V(rock_bias), V(base_vel), 2.0f);
    if (vec3_length(V(hit)) == 0.0f) {
        lq_sq(origin, m + 0x10);
        swc1(origin + 0x8, fpu_add(lwc1(origin + 0x8), 6.0f));
    } else {
        lq_sq(origin, hit);
    }

    const float half_pi = u2f(0x3fc90fdbu);
    for (int i = 0; i < 25; ++i) {
        const float cx = cos_vu(rand_range_f(lwc1(m + 0x48), half_pi));
        swc1(pos_buf + 0x0, fpu_mul(cx, rand_range_f(-3.0f, 3.0f)));
        const float sy = sin_vu(rand_range_f(lwc1(m + 0x48), half_pi));
        swc1(pos_buf + 0x4, fpu_mul(sy, rand_range_f(-3.0f, 3.0f)));
        swc1(pos_buf + 0x8, rand_range_f(1.0f, 7.0f));
        vec3_add_inplace(V(pos_buf), V(m + 0x10));
        vec3_sub(V(dir_buf), V(origin), V(pos_buf));
        lq_sq(vel_buf, dir_buf);
        vec3_set_length(V(vel_buf), V(vel_buf), fpu_mul(lwc1(0x0015ed6cu), 5.0f));
        vec3_add_inplace(V(vel_buf), V(rock_bias));
        swc1(vel_buf + 0x8, fpu_add(lwc1(vel_buf + 0x8), fpu_mul(lwc1(0x0015ed6cu), 3.0f)));
        vec3_scale(V(vel_buf), V(vel_buf), rand_range_f(0.75f, 1.25f));
        const uint32_t rock_cls = lwu(L15_ADDR(0x001615b8u) + (static_cast<uint32_t>(rand_mod(4)) << 2));
        const float scale = rand_range_f(u2f(0x3dcccccd), u2f(0x3e99999a));
        const int32_t life = rand_range(0x64, 0xa0);
        const float gravity = rand_range_f(3.0f, 7.0f);
        rock_spawn(pos_buf, vel_buf, rock_cls, life, 1, scale, gravity, u2f(0x3e19999a), 2.0f);  // (spin 0.15, wait 2 s)
    }

    vec_clear(V(vel_buf));
    explosion_effect(m, vel_buf, origin, 0x14, 3, 4, 0, 1, 1, -1, 0,
                     0.0f, 0.0f, 10.0f, 5.0f, 100000.0f, 3.0f, 15.0f);

    for (int i = 0; i < 20; ++i) {
        const float cx = cos_vu(rand_range_f(lwc1(m + 0x48), half_pi));
        swc1(pos_buf + 0x0, fpu_mul(cx, rand_range_f(-3.0f, 3.0f)));
        const float sy = sin_vu(rand_range_f(lwc1(m + 0x48), half_pi));
        swc1(pos_buf + 0x4, fpu_mul(sy, rand_range_f(-3.0f, 3.0f)));
        sw(pos_buf + 0x8, 0);
        vec3_add(V(pos_buf), V(pos_buf), V(m + 0x10));
        swc1(pos_buf + 0x8, fpu_add(lwc1(pos_buf + 0x8), rand_range_f(1.0f, 5.5f)));
        vec3_sub(V(pdir_buf), V(origin), V(pos_buf));
        lq_sq(vel_buf, pdir_buf);
        vec3_set_length(V(vel_buf), V(vel_buf), fpu_mul(lwc1(0x0015ed6cu), 5.0f));
        vec3_add_inplace(V(vel_buf), V(base_vel));
        vec3_scale(V(vel_buf), V(vel_buf), rand_range_f(0.75f, 1.25f));
        const uint32_t col0 = rgba_lerp_ovl(0x7f000000u, 0x7f182030u, rand_range_f(0.25f, 1.0f));
        const uint32_t col1 = rgba_lerp_ovl(0u, 0x005f5f5fu, rand_range_f(0.5f, 1.0f));
        const float size = fpu_mul(rand_range_f(1.0f, 3.0f), 210000.0f);
        const uint32_t life = static_cast<uint32_t>(frame_scale_i(0xb4));
        const uint32_t tex = lwu(L15_ADDR(0x001615c8u) + (static_cast<uint32_t>(rand_mod(3)) << 2));
        const uint32_t pt = spawn_particle_10(pos_buf, vel_buf, col0, col1, life, tex, size);
        if (pt != 0) {
            swc1(pt + 0x2c, fpu_sub(lwc1(m + 0x18), 0.5f));
        }
    }

    moby_drop_bolts(m, 0, -1);
    moby_retire(m);
    ee_stack_free(400);
}

}  // namespace rac1
