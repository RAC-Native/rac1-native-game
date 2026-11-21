// Pokitaru's (level 11) sky cargo train (class 1264, L11:0x3172c0), its carriages (class 1265, L11:0x3181f0),
// its falling bombs/debris (class 1524, L11:0x31ab08), and their helpers (0x317678, 0x317820, 0x318488, 0x317c98,
// 0x3180a0, 0x31ada0, 0x31ae68, 0x31b098), ported by hand.
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/pokitaru_own.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l11.h>

namespace rac1 {

// Interpolates the train locomotive's position and pitch/yaw along its closed path (pvar +0xf0, segment +0x68, frac +0x6c).
// orig: L11:0x00317678
void pok_317678_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t i0 = lw(pv + 0x68);
    const uint32_t path = lwu(pv + 0xf0);
    const int32_t n = lw(path);
    const int32_t i1 = ee_rem(i0 + 1, n);
    const int32_t i2 = ee_rem(i0 + 2, n);
    const uint32_t sp = ee_stack_alloc(0x30);
    lq_sq(sp + 0x0, path + 0x10 + (static_cast<uint32_t>(i0) << 4));
    lq_sq(sp + 0x10, path + 0x10 + (static_cast<uint32_t>(i1) << 4));
    lq_sq(sp + 0x20, path + 0x10 + (static_cast<uint32_t>(i2) << 4));
    const float yaw0 = atan2_f(fpu_sub(lwc1(sp + 0x0), lwc1(sp + 0x10)), fpu_sub(lwc1(sp + 0x4), lwc1(sp + 0x14)));
    const float yaw1 = atan2_f(fpu_sub(lwc1(sp + 0x10), lwc1(sp + 0x20)), fpu_sub(lwc1(sp + 0x14), lwc1(sp + 0x24)));
    const float pitch0 = atan2_f(vec2_distance(V(sp + 0x0), V(sp + 0x10)), fpu_sub(lwc1(sp + 0x18), lwc1(sp + 0x8)));
    const float pitch1 = atan2_f(vec2_distance(V(sp + 0x10), V(sp + 0x20)), fpu_sub(lwc1(sp + 0x28), lwc1(sp + 0x18)));
    const float t = lwc1(pv + 0x6c);
    swc1(m + 0x44, angle_add(fpu_mul(angle_sub(pitch1, pitch0), t), pitch0));
    swc1(m + 0x48, angle_add(fpu_mul(angle_sub(yaw1, yaw0), t), yaw0));
    vec3_lerp(V(m + 0x10), V(sp + 0x0), V(sp + 0x10), t);
    ee_stack_free(0x30);
}

// Places each active carriage (pvar +0x70[0..count-1]) trailing behind the locomotive along the path.
// orig: L11:0x00317820
void pok_317820_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lwu(pv + 0xf0);
    const uint32_t sp = ee_stack_alloc(0x10);
    vec3_set_length(V(sp), V(m + 0xc0), lwc1(L11_ADDR(0x00162328u)));
    vec3_add(V(sp), V(sp), V(m + 0x10));
    for (int32_t i = 0; i < lw(pv + 0x64); ++i) {
        const uint32_t off2 = static_cast<uint32_t>(i) << 1;
        const uint32_t off4 = static_cast<uint32_t>(i) << 2;
        const int32_t n = lw(path);
        int32_t s1 = lh(pv + 0x90 + off2);
        int32_t s3 = ee_rem(s1 + 1, n);
        int32_t s2 = ee_rem(s1 + 2, n);
        float d1 = vec3_distance(V(path + 0x10 + (static_cast<uint32_t>(s1) << 4)), V(sp));
        float d2 = vec3_distance(V(path + 0x10 + (static_cast<uint32_t>(s3) << 4)), V(sp));
        while (lwc1(L11_ADDR(0x00162324u)) < d2) {
            const int32_t nn = lw(path);
            s1 = s3;
            s3 = ee_rem(s1 + 1, nn);
            s2 = ee_rem(s3 + 1, nn);
            d1 = d2;
            d2 = vec3_distance(V(path + 0x10 + (static_cast<uint32_t>(s3) << 4)), V(sp));
        }
        if (lwc1(path + 0x1c + (static_cast<uint32_t>(s1) << 4)) == 42.0f) {
            const uint32_t car = lwu(pv + 0x70 + off4);
            const uint32_t cpv = lwu(car + 0x78);
            if (lw(cpv + 0x28) == 0) {
                if (lw(L11_ADDR(0x0015f5c4u)) == 0) moby_sound_start(2, 0, car);
                sw(cpv + 0x28, static_cast<uint32_t>(frame_scale_i(lw(L11_ADDR(0x0016232cu)))));
            }
        }
        const float spacing = lwc1(L11_ADDR(0x00162324u));
        float frac = (spacing < d1) ? fpu_div(fpu_sub(d1, spacing), fpu_sub(d1, d2)) : 0.0f;
        sh(pv + 0x90 + off2, static_cast<uint32_t>(s1));
        swc1(pv + 0xa0 + off4, frac);
        const float shift = lwc1(pv + 0xc0 + off4);
        if (shift != 0.0f) {
            float f20 = fpu_sub(frac, shift);
            int32_t whole = float_to_int(f20);
            f20 = fpu_sub(f20, static_cast<float>(float_to_int(f20)));
            if (f20 < 0.0f) {
                --whole;
                f20 = fpu_add(f20, 1.0f);
            }
            const int32_t nn = lw(path);
            s1 = ee_rem(s1 + whole + nn, nn);
            s3 = ee_rem(s3 + whole + nn, nn);
            s2 = ee_rem(s2 + whole + nn, nn);
            frac = f20;
        }
        const uint32_t car = lwu(pv + 0x70 + off4);
        const uint32_t p1 = path + 0x10 + (static_cast<uint32_t>(s1) << 4);
        const uint32_t p3 = path + 0x10 + (static_cast<uint32_t>(s3) << 4);
        const uint32_t p2 = path + 0x10 + (static_cast<uint32_t>(s2) << 4);
        vec3_lerp(V(car + 0x10), V(p1), V(p3), frac);
        lq_sq(sp, car + 0x10);
        const float yaw0 = atan2_f(fpu_sub(lwc1(p1), lwc1(p3)), fpu_sub(lwc1(p1 + 4), lwc1(p3 + 4)));
        const float yaw1 = atan2_f(fpu_sub(lwc1(p3), lwc1(p2)), fpu_sub(lwc1(p3 + 4), lwc1(p2 + 4)));
        const float pitch0 = -atan2_f(vec2_distance(V(p3), V(p1)), fpu_sub(lwc1(p1 + 8), lwc1(p3 + 8)));
        const float pitch1 = -atan2_f(vec2_distance(V(p2), V(p3)), fpu_sub(lwc1(p3 + 8), lwc1(p2 + 8)));
        swc1(car + 0x44, angle_add(fpu_mul(angle_sub(pitch1, pitch0), frac), pitch0));
        swc1(car + 0x48, angle_add(fpu_mul(angle_sub(yaw1, yaw0), frac), yaw0));
        moby_update_transform(car);
        vec3_set_length(V(sp), V(car + 0xc0), lwc1(L11_ADDR(0x00162328u)));
        vec3_add(V(sp), V(sp), V(car + 0x10));
    }
    ee_stack_free(0x10);
}

// Detaches a carriage (class 1265): switches it to state 1 (explode), clears its model, sets tumble rates.
// orig: L11:0x00318488
void pok_318488_l11(uint32_t car) {
    const uint32_t pv = lwu(car + 0x78);
    sb(car + 0x20, 1);
    sw(car + 0x94, 0);
    vec3_sub(V(pv + 0x10), V(car + 0x10), V(pv));
    const float rad = u2f(0x3c8efa35u);
    swc1(pv + 0x20, fpu_mul(fpu_mul(rand_range_f(20.0f, 60.0f), rad), lwc1(0x0015ed6cu)));
    swc1(pv + 0x24, fpu_mul(fpu_mul(rand_range_f(20.0f, 60.0f), rad), lwc1(0x0015ed6cu)));
}

// Handles hits on the train's carriages, detaching one per 10 HP lost and destroying the locomotive when empty.
// orig: L11:0x00317c98
void pok_317c98_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t last_hit = -1;
    float total_dmg = 0.0f;
    for (int32_t i = 0; i < lw(pv + 0x64); ++i) {
        const uint32_t car = lwu(pv + 0x70 + (static_cast<uint32_t>(i) << 2));
        if (const uint32_t h = sub_0025a420(car, 0xffffffffu, 0); h != 0) {
            total_dmg = fpu_add(total_dmg, lwc1(h + 0x2c));
            last_hit = i;
            sb(car + 0xa4, 0xff);
        }
    }
    approach_f(pv + 0xe0, 0.0f, total_dmg);
    if (total_dmg == 0.0f) return;
    if (!(fpu_mul(fpu_div(lwc1(pv + 0xe0), 80.0f), 8.0f) <= static_cast<float>(lw(pv + 0x64) - 1))) return;

    sh(0x00141c18u, 0xffffu);
    const int32_t rem = lw(pv + 0x64) - 1;
    sw(pv + 0x64, static_cast<uint32_t>(rem));
    const uint32_t tail_car = lwu(pv + 0x70 + (static_cast<uint32_t>(rem) << 2));
    pok_318488_l11(tail_car);
    const uint32_t hit_car = lwu(pv + 0x70 + (static_cast<uint32_t>(last_hit) << 2));
    lq_sq(tail_car + 0x10, hit_car + 0x10);
    lq_sq(tail_car + 0x40, hit_car + 0x40);
    moby_sound_start(1, 0, tail_car);

    if (last_hit < lw(pv + 0x64)) {
        const float delta = static_cast<float>(lh(pv + 0x90 + (static_cast<uint32_t>(last_hit) << 1)) -
                                               lh(pv + 0x90 + (static_cast<uint32_t>(last_hit + 1) << 1)));
        for (int32_t k = last_hit; k < lw(pv + 0x64); ++k) {
            const uint32_t o = pv + 0xc0 + (static_cast<uint32_t>(k) << 2);
            swc1(o, lwc1(o + 4));
        }
        const uint32_t slot = pv + 0xc0 + (static_cast<uint32_t>(last_hit) << 2);
        float s = fpu_add(lwc1(slot), delta);
        if (s < 0.0f) s = fpu_add(s, int_to_float(lw(lwu(pv + 0xf0))));
        swc1(slot, s);
    }
    for (int32_t k = last_hit; k < lw(pv + 0x64); ++k) {
        const uint32_t o = pv + 0x90 + (static_cast<uint32_t>(k) << 1);
        sh(o, lhu(o + 2));
    }

    if (lw(pv + 0x64) != 0) return;
    const uint32_t sp = ee_stack_alloc(0x20);
    const float sc = lwc1(0x0015ed60u);
    vec3_scale(V(sp + 0x10), V(m + 0xc0), fpu_mul(lwc1(L11_ADDR(0x00162330u)), sc));
    swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), fpu_mul(lwc1(0x0015ed60u), u2f(0x3da3d70au))));
    moby_sound_start(2, 0, m);
    explosion_burst(m, m + 0x10, -1, 3.0f, 13.0f);
    const uint32_t zero_v = L11_ADDR(0x0015f580u);
    const uint32_t life1 = static_cast<uint32_t>(frame_scale_i(0x5a));
    debris_spawn(m, 0x602, m + 0x10, m + 0x40, static_cast<int32_t>(life1), 0, sp + 0x10, zero_v, zero_v,
                 fpu_mul(lwc1(0x0015ed70u), 12.0f));
    const uint32_t life2 = static_cast<uint32_t>(frame_scale_i(0x5a));
    debris_spawn(m, 0x603, m + 0x10, m + 0x40, static_cast<int32_t>(life2), 0, sp + 0x10, zero_v, zero_v,
                 fpu_mul(lwc1(0x0015ed70u), 12.0f));
    const uint32_t life3 = static_cast<uint32_t>(frame_scale_i(0x5a));
    debris_spawn(m, 0x784, m + 0x10, m + 0x40, static_cast<int32_t>(life3), 0, sp + 0x10, zero_v, zero_v,
                 fpu_mul(lwc1(0x0015ed70u), 12.0f));
    ee_stack_free(0x20);
    moby_retire(m);
}

// Distributes class 1319 (0x527) bombers in group pvar +0xe4 evenly along path `path_idx`.
// orig: L11:0x003180a0
void pok_3180a0_l11(uint32_t m, int32_t path_idx) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t grp = lh(pv + 0xe4);
    uint32_t e = lvd::group(grp);
    if (e == 0) return;
    const uint32_t mobys = lwu(L11_ADDR(0x0015ffd8u));
    int32_t count = 0;
    for (uint32_t p = e;; p += 2) {
        const uint16_t raw = lhu(p);
        const uint32_t o = mobys + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        if (lh(o + 0xa6) == 0x527) count = static_cast<int16_t>(count + 1);
        if (static_cast<int16_t>(raw) < 0) break;
    }
    const uint32_t path = lvd::path(path_idx);
    const int32_t step = static_cast<int16_t>(ee_div(lw(path), count));
    int32_t idx = 0;
    for (uint32_t p = lvd::group(grp);; p += 2) {
        const uint16_t raw = lhu(p);
        const uint32_t o = mobys + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        if (lh(o + 0xa6) == 0x527) {
            lq_sq(o + 0x10, path + 0x10 + (static_cast<uint32_t>(idx) << 4));
            const uint32_t opv = lwu(o + 0x78);
            sw(opv + 0x64, static_cast<uint32_t>(idx));
            sw(opv + 0x68, 0);
            sw(opv + 0x60, static_cast<uint32_t>(path_idx));
            idx = static_cast<int16_t>(idx + step);
        }
        if (static_cast<int16_t>(raw) < 0) break;
    }
}

// Class 1264 update: sky cargo train locomotive.
// orig: L11:0x003172c0
void pok_class1264_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        pok_3180a0_l11(m, lw(pv + 0x60));
        if (lbu(0x0014c050u + lbu(m + 0xb0) + (lwu(0x0015ed84u) << 4)) == 0xff) {
            moby_retire(m);
            return;
        }
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0x200);
        const uint32_t path = lvd::path(lw(pv + 0xec));
        sw(pv + 0xf0, path);
        lq_sq(m + 0x10, path + 0x10);
        sw(pv + 0xf4, 0xffffffffu);
        swc1(pv + 0xe0, 80.0f);
        sw(pv + 0x64, 8);
        sw(pv + 0x68, 0);
        sw(pv + 0x6c, 0);
        sb(m + 0x20, 1);
        for (uint32_t k = 0; k < 8; ++k) {
            const uint32_t car = moby_spawn(0x4f1);
            sw(pv + 0x70 + (k << 2), car);
            sh(car + 0x32, 0x200);
            sb(car + 0x30, 0xff);
            sb(car + 0x31, 1);
            sd(car + 0x38, ld(m + 0x38));
            sh(car + 0x34, lhu(m + 0x34) & 0xffdfu);
            const float sc = lwc1(lwu(car + 0x24) + 0x24);
            swc1(car + 0x2c, fpu_add(sc, sc));
            lq_sq(car + 0x10, m + 0x10);
            lq_sq(car + 0x40, m + 0x40);
            sh(pv + 0x90 + (k << 1), lhu(lwu(pv + 0xf0)) - 0x64);
            sw(pv + 0xc0 + (k << 2), 0);
        }
        return;
    }
    if (state != 1 && state != 2) return;
    if (state == 1 && lw(0x001413d4u) == 0x32) {
        sb(m + 0x20, 2);
        const uint32_t path = lvd::path(lw(pv + 0x60));
        sw(pv + 0x68, 0);
        sw(pv + 0xf0, path);
        for (uint32_t k = 0; k < 8; ++k) {
            sh(pv + 0x90 + (k << 1), lhu(lwu(pv + 0xf0)) - 0x64);
            sw(pv + 0xc0 + (k << 2), 0);
        }
    }
    float t = fpu_add(lwc1(pv + 0x6c), lwc1(L11_ADDR(0x00162320u)));
    if (1.0f < t) {
        t = fpu_sub(t, 1.0f);
        swc1(pv + 0x6c, t);
        sw(pv + 0x68, static_cast<uint32_t>(ee_rem(lw(pv + 0x68) + 1, lw(lwu(pv + 0xf0)))));
    } else {
        swc1(pv + 0x6c, t);
    }
    for (uint32_t k = 0; k < 8; ++k) {
        const uint32_t slot = pv + 0xc0 + (k << 2);
        const float before = lwc1(slot);
        approach_f(slot, 0.0f, fpu_mul(lwc1(0x0015ed6cu), 5.0f));
        if (before != 0.0f && lwc1(slot) == 0.0f) {
            if (const uint32_t car = lwu(pv + 0x70 + (k << 2)); car != 0) moby_sound_start(0, 0, car);
        }
    }
    pok_317678_l11(m);
    pok_317820_l11(m);
    if (lbu(m + 0x20) == 2) pok_317c98_l11(m);
    if (sub_0028d8c0(m, lw(pv + 0xf4)) == 0) {
        sw(pv + 0xf4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }
}

// Spawns a class 1524 (0x5f4) falling sludge/bomb drop at `pos` with velocity `vel`, water-height ptr `water_ptr`, scale `scale`.
// orig: L11:0x0031ada0
uint32_t pok_31ada0_l11(uint32_t pos, uint32_t vel, uint32_t water_ptr, float scale) {
    const uint32_t drop = moby_spawn(0x5f4);
    if (drop == 0) return 0;
    const uint32_t pv = lwu(drop + 0x78);
    sb(drop + 0x20, 1);
    lq_sq(drop + 0x10, pos);
    swc1(drop + 0x40, rand_angle());
    swc1(drop + 0x48, rand_angle());
    sb(drop + 0x30, 0xff);
    sb(drop + 0x31, 1);
    sh(drop + 0x32, 0xff);
    swc1(drop + 0x2c, fpu_mul(lwc1(lwu(drop + 0x24) + 0x24), scale));
    sw(pv + 0x24, water_ptr);
    lq_sq(pv, vel);
    return drop;
}

// Class 1265 update: cargo train carriage (state 0 drips sludge drops; state 1 explodes into 3 debris pieces).
// orig: L11:0x003181f0
void pok_class1265_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x28))) == 0 &&
            rand_mod(lw(L11_ADDR(0x00162340u))) == 0 && lbu(m + 0x31) != 0 &&
            lw(L11_ADDR(0x0015ffbcu)) >= 71) {
            const uint32_t sp = ee_stack_alloc(0x40);
            mat3_transform(V(sp + 0x10), V(L11_ADDR(0x00162350u)), M(m + 0xc0));
            vec3_add(V(sp + 0x10), V(sp + 0x10), V(m + 0x10));
            rand_sphere_point(sp + 0x30, 0.0f, fpu_mul(lwc1(L11_ADDR(0x00162368u)), lwc1(0x0015ed6cu)));
            vec3_sub(V(sp + 0x20), V(m + 0x10), V(pv));
            const float dt = lwc1(0x0015ed6cu);
            swc1(sp + 0x28, fpu_sub(lwc1(sp + 0x28), fpu_add(dt, dt)));
            vec3_add(V(sp + 0x20), V(sp + 0x20), V(sp + 0x30));
            const float sc = rand_range_f(lwc1(L11_ADDR(0x00162360u)), lwc1(L11_ADDR(0x00162364u)));
            pok_31ada0_l11(sp + 0x10, sp + 0x20, L11_ADDR(0x00161e18u), sc);
            ee_stack_free(0x40);
        }
        lq_sq(pv, m + 0x10);
        return;
    }
    if (state == 1) {
        explosion_burst(m, m + 0x10, -1, 5.0f, 13.0f);
        const uint32_t sp = ee_stack_alloc(0x20);
        vec3_scale(V(sp + 0x10), V(m + 0xc0), fpu_mul(lwc1(L11_ADDR(0x0016236cu)), lwc1(0x0015ed60u)));
        swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), fpu_mul(lwc1(0x0015ed60u), u2f(0x3da3d70au))));
        const uint32_t zero_v = L11_ADDR(0x0015f580u);
        const uint32_t life1 = static_cast<uint32_t>(frame_scale_i(0x5a));
        debris_spawn(m, 0x785, m + 0x10, m + 0x40, static_cast<int32_t>(life1), 0, sp + 0x10, zero_v, zero_v,
                     fpu_mul(lwc1(0x0015ed70u), 12.0f));
        const uint32_t life2 = static_cast<uint32_t>(frame_scale_i(0x5a));
        debris_spawn(m, 0x786, m + 0x10, m + 0x40, static_cast<int32_t>(life2), 0, sp + 0x10, zero_v, zero_v,
                     fpu_mul(lwc1(0x0015ed70u), 12.0f));
        const uint32_t life3 = static_cast<uint32_t>(frame_scale_i(0x5a));
        debris_spawn(m, 0x787, m + 0x10, m + 0x40, static_cast<int32_t>(life3), 0, sp + 0x10, zero_v, zero_v,
                     fpu_mul(lwc1(0x0015ed70u), 12.0f));
        ee_stack_free(0x20);
        sb(m + 0x20, 2);
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
    }
}

// Spawns `n_droplets` splash particles (type 0x23) and `n_rings` surface ripples (type 0x2e) at water height `*water_ptr`.
// orig: L11:0x0031ae68
void pok_31ae68_l11(uint32_t m, uint32_t water_ptr, int32_t n_droplets, int32_t n_rings) {
    const uint32_t sp = ee_stack_alloc(0x10);
    for (int32_t i = 0; i < n_droplets; ++i) {
        const float a = rand_angle();
        const float dt = lwc1(0x0015ed6cu);
        const float r = rand_range_f(fpu_mul(dt, 0.0f), fpu_mul(dt, 6.0f));
        swc1(sp + 0x0, fpu_mul(cos_vu(a), r));
        swc1(sp + 0x4, fpu_mul(sin_vu(a), r));
        const float dt2 = lwc1(0x0015ed6cu);
        swc1(sp + 0x8, rand_range_f(fpu_mul(dt2, 3.0f), fpu_mul(dt2, 6.5f)));
        const uint32_t life = static_cast<uint32_t>(rand_range(0x5a, 0x78));
        spawn_particle_23(m + 0x10, sp, rand_mod(2), life);
    }
    for (int32_t i = 0; i < n_rings; ++i) {
        rand_sphere_point(sp, 1.0f, 1.0f);
        vec3_add(V(sp), V(sp), V(m + 0x10));
        swc1(sp + 0x8, fpu_add(lwc1(water_ptr), u2f(0x3d4ccccdu)));
        const float sz = rand_range_f(3.0f, 6.0f);
        const float dir = (i == 0) ? 1.0f : -1.0f;
        if (const uint32_t p = spawn_particle_2e(sp, L11_ADDR(0x0015f580u), water_ptr, sz, dir); p != 0) {
            sb(p + 0x9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
            sh(p + 0xa, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(30.0f, 90.0f)))));
            sw(p + 0x4, rgba_lerp(lwu(L11_ADDR(0x00162420u)), lwu(L11_ADDR(0x00162424u)), rand_range_f(0.0f, 1.0f)));
        }
    }
    ee_stack_free(0x10);
}

// Spawns a trailing smoke/sludge particle (type 0x02) behind a falling class 1524 drop.
// orig: L11:0x0031b098
void pok_31b098_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x30);
    rand_sphere_point(sp + 0x10, 0.0f, fpu_mul(lwc1(L11_ADDR(0x00162428u)), lwc1(0x0015ed6cu)));
    vec3_add(V(sp + 0x10), V(sp + 0x10), V(pv));
    vec_clear(V(sp + 0x20));
    const float vz = fpu_mul(rand_range_f(lwc1(L11_ADDR(0x0016242cu)), lwc1(L11_ADDR(0x00162430u))), lwc1(0x0015ed6cu));
    swc1(sp + 0x1c, lwc1(L11_ADDR(0x00162434u)));
    swc1(sp + 0x2c, lwc1(L11_ADDR(0x00162438u)));
    swc1(sp + 0x28, vz);
    const uint32_t c0 = rgba_lerp(lwu(L11_ADDR(0x0016243cu)), lwu(L11_ADDR(0x00162440u)), rand_range_f(0.0f, 1.0f));
    const uint32_t c1 = rgba_lerp(lwu(L11_ADDR(0x00162444u)), lwu(L11_ADDR(0x00162448u)), rand_range_f(0.0f, 1.0f));
    const float f08 = u2f(0x3f4ccccdu);
    const float f12 = u2f(0x3f99999au);
    const float b0 = int_to_float(lw(L11_ADDR(0x0016244cu)));
    const uint32_t t1 = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(fpu_mul(b0, f08), fpu_mul(b0, f12)))));
    const float b1 = int_to_float(lw(L11_ADDR(0x00162450u)));
    const uint32_t t2 = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(fpu_mul(b1, f08), fpu_mul(b1, f12)))));
    const float b2 = int_to_float(lw(L11_ADDR(0x00162454u)));
    const uint32_t t3 = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(fpu_mul(b2, f08), fpu_mul(b2, f12)))));
    spawn_particle_02(m + 0x10, sp + 0x10, sp + 0x20, c0, c1, t1, t2, t3, -1);
    ee_stack_free(0x30);
}

// Class 1524 update: falling sludge/bomb drop that splashes into water and shrinks away.
// orig: L11:0x0031ab08
void pok_class1524_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x31) == 0) {
        const int16_t off_t = static_cast<int16_t>(lhu(pv + 0x22) + 1);
        sh(pv + 0x22, static_cast<uint32_t>(off_t));
        if (frame_scale_i(0xf) < off_t) {
            moby_retire(m);
            return;
        }
    } else {
        sh(pv + 0x22, 0);
    }

    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t state = lbu(m + 0x20);
    if (state == 1) {
        swc1(pv + 0x8, fpu_sub(lwc1(pv + 0x8), fpu_mul(lwc1(L11_ADDR(0x00162400u)), lwc1(0x0015ed70u))));
        if (fpu_add(lwc1(lwu(pv + 0x24)), 3.0f) < lwc1(m + 0x18) && rand_mod(lw(L11_ADDR(0x00162410u))) == 0) {
            pok_31b098_l11(m);
        }
        if (lwc1(m + 0x18) < lwc1(lwu(pv + 0x24))) {
            vec3_scale(V(pv), V(pv), lwc1(L11_ADDR(0x0016240cu)));
            sb(m + 0x20, 2);
            pok_31ae68_l11(m, lwu(pv + 0x24), 8, 4);
        }
    } else if (state == 2) {
        swc1(pv + 0x8, fpu_sub(lwc1(pv + 0x8), fpu_mul(lwc1(L11_ADDR(0x00162404u)), lwc1(0x0015ed70u))));
        const float shrink = fpu_div(lwc1(lwu(m + 0x24) + 0x24), static_cast<float>(frame_scale_i(0x3c)));
        if ((lwu(L11_ADDR(0x0015f5ccu)) & 3u) == 0) {
            pok_31ae68_l11(m, lwu(pv + 0x24), 0, 1);
            rand_sphere_point(sp, 0.0f, lwc1(0x0015ed6cu));
            swc1(sp + 0x8, abs_f(lwc1(sp + 0x8)));
            if (const uint32_t p = spawn_particle_22(m + 0x10, sp, lwc1(L11_ADDR(0x00162414u)), lwc1(lwu(pv + 0x24))); p != 0) {
                sw(p + 0x4, rgba_lerp(lwu(L11_ADDR(0x00162418u)), lwu(L11_ADDR(0x0016241cu)), rand_range_f(0.0f, 1.0f)));
            }
        }
        approach_f(m + 0x2c, 0.0f, shrink);
        if (lwc1(m + 0x2c) == 0.0f) {
            ee_stack_free(0x20);
            moby_retire(m);
            return;
        }
    }
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    swc1(sp + 0x10, vec2_length(V(pv)));
    approach_f(sp + 0x10, 0.0f, fpu_mul(lwc1(L11_ADDR(0x00162408u)), lwc1(0x0015ed70u)));
    vec2_set_length(V(pv), V(pv), lwc1(sp + 0x10));
    ee_stack_free(0x20);
}

}  // namespace rac1
