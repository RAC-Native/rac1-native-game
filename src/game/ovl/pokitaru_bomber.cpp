// Pokitaru's (level 11) Blarg bomber (class 1319, L11:0x319838, helper 0x30f5a8), energy ball (class 1297,
// L11:0x318b30, helpers 0x319028, 0x3191c0), rising double-panel bridge (class 1248, L11:0x316320), and
// fighter-jet skill-point monitor (class 1350, L11:0x31aa80), ported by hand.
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/pokitaru_own.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1319.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// Class 1248 update: rising double-panel bridge / hatch (spawns two class 0x4df halves).
// orig: L11:0x00316320
void pok_class1248_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float rad = u2f(0x3c8efa35u);
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b740)
    case 0: {  // (0x0031637c)
        const float sc = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(sc, sc));
        auto spawn_half = [&](uint32_t slot_off, uint32_t extra_flag, bool neg_roll) {
            const uint32_t h = moby_spawn(0x4df);
            sw(pv + slot_off, h);
            sh(h + 0x32, 0x40);
            sb(h + 0x31, 1);
            sd(h + 0x38, ld(lwu(0x001413d0u) + 0x38));
            sh(h + 0x34, lhu(m + 0x34) | extra_flag);
            lq_sq(h + 0x10, m + 0x10);
            swc1(h + 0x18, fpu_sub(lwc1(h + 0x18), 4.0f));
            lq_sq(h + 0x40, m + 0x40);
            const float rx = fpu_mul(lwc1(L11_ADDR(0x0016230cu)), rad);
            const float ry = fpu_mul(lwc1(L11_ADDR(0x00162310u)), rad);
            swc1(h + 0x40, neg_roll ? -rx : rx);
            swc1(h + 0x44, ry);
            const uint32_t hpv = lwu(h + 0x78);
            sw(hpv + 0x8, hpv + 0x20);
            sw(hpv + 0x5c, lwu(hpv + 0x5c) | 1u);
        };
        spawn_half(0x0, 0x20u, true);
        spawn_half(0x4, 0x8020u, false);
        sb(m + 0x20, 1);
        break;
    }
    case 1: {  // (0x003164fc)
        if (vec2_distance(V(m + 0x10), V(0x0013f3d0u)) < 12.0f && point_in_box(0x0013f3d0u, lw(pv + 0x10)) != 0) {
            sw(pv + 0x8, 0);
            sb(m + 0x20, 2);
            moby_sound_start(0, 4, lwu(pv + 0x0));
        }
        break;
    }
    case 2: {  // (0x00316560)
        const float dt2 = lwc1(0x0015ed70u);
        seek_1d(pv + 0x8, pv + 0xc, 1.0f, fpu_mul(dt2, 8.0f), fpu_mul(dt2, 16.0f), fpu_mul(lwc1(0x0015ed6cu), 8.0f));
        const uint32_t sp = ee_stack_alloc(0x10);
        vec3_set_length(V(sp), V(m + 0xc0), 0.5f);
        for (uint32_t slot = 0; slot <= 4; slot += 4) {
            const uint32_t h = lwu(pv + slot);
            vec3_add(V(h + 0x10), V(m + 0x10), V(sp));
            swc1(h + 0x18, fpu_sub(fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(pv + 0x8), 4.0f)), 4.0f));
        }
        ee_stack_free(0x10);
        moby_update_transform(lwu(pv + 0x0));
        moby_update_transform(lwu(pv + 0x4));
        if (1.0f <= lwc1(pv + 0x8)) {
            sw(pv + 0x8, 0);
            sb(m + 0x20, 3);
        }
        break;
    }
    case 3: {  // (0x00316654)
        const float dt2 = lwc1(0x0015ed70u);
        seek_1d(pv + 0x8, pv + 0xc, 1.0f, fpu_mul(dt2, 10.0f), fpu_mul(dt2, 20.0f), fpu_mul(lwc1(0x0015ed6cu), 10.0f));
        const float ry = fpu_mul(fpu_sub(1.0f, lwc1(pv + 0x8)), fpu_mul(lwc1(L11_ADDR(0x00162310u)), rad));
        swc1(lwu(pv + 0x0) + 0x44, ry);
        swc1(lwu(pv + 0x4) + 0x44, ry);
        moby_update_transform(lwu(pv + 0x0));
        moby_update_transform(lwu(pv + 0x4));
        if (1.0f <= lwc1(pv + 0x8)) {
            sw(pv + 0x8, 0);
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4: {  // (0x003166f8)
        const float dt2 = lwc1(0x0015ed70u);
        seek_1d(pv + 0x8, pv + 0xc, 1.0f, fpu_mul(dt2, 12.0f), fpu_mul(dt2, 24.0f), fpu_mul(lwc1(0x0015ed6cu), 12.0f));
        swc1(lwu(pv + 0x0) + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(pv + 0x8), 0.25f)));
        swc1(lwu(pv + 0x4) + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(pv + 0x8), 0.25f)));
        const float rx = fpu_mul(lwc1(pv + 0x8), u2f(0x3fc90fdbu));
        swc1(lwu(pv + 0x0) + 0x40, -rx);
        swc1(lwu(pv + 0x4) + 0x40, rx);
        moby_update_transform(lwu(pv + 0x0));
        moby_update_transform(lwu(pv + 0x4));
        if (1.0f <= lwc1(pv + 0x8)) {
            sw(pv + 0x8, 0);
            sb(m + 0x20, 5);
        }
        break;
    }
    case 6: {  // (0x003167cc)
        const uint32_t sp = ee_stack_alloc(0x10);
        vec3_set_length(V(sp), V(m + 0xc0), 0.5f);
        for (uint32_t slot = 0; slot <= 4; slot += 4) {
            const uint32_t h = lwu(pv + slot);
            vec3_add(V(h + 0x10), V(m + 0x10), V(sp));
            swc1(h + 0x18, fpu_add(lwc1(m + 0x18), lwc1(L11_ADDR(0x00162308u))));
            const float rx = fpu_mul(lwc1(L11_ADDR(0x0016230cu)), rad);
            const float ry = fpu_mul(lwc1(L11_ADDR(0x00162310u)), rad);
            swc1(h + 0x40, (slot == 0) ? -rx : rx);
            swc1(h + 0x44, ry);
            moby_update_transform(h);
        }
        ee_stack_free(0x10);
        break;
    }
    default:  // (0x00316894: state 5 or >=7)
        break;
    }
}

// Spawns 4 glowing core layers (particle 0x3b) for class 1297's energy ball along the camera vector.
// orig: L11:0x00319028
void pok_319028_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x20);
    vec3_sub(V(sp), V(L11_ADDR(0x001677c0u)), V(m + 0x10));
    vec3_set_length(V(sp), V(sp), u2f(0xbe99999au));
    vec3_set_length(V(sp + 0x10), V(sp), u2f(0x3dcccccd));
    vec3_add(V(sp), V(sp), V(m + 0x10));
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t col = rgba_lerp(lwu(L11_ADDR(0x00162380u)), lwu(L11_ADDR(0x00162384u)),
                                       fpu_mul(static_cast<float>(i), u2f(0x3eaa7efau)));
        const uint32_t ang_slot = pv + 0x10 + static_cast<uint32_t>(i);
        const uint32_t ang = (lbu(ang_slot) + lbu(ang_slot + 4)) & 0xffu;
        sb(ang_slot, ang);
        const uint32_t rot_b = static_cast<uint32_t>(float_to_int(static_cast<float>(ang))) & 0xffu;
        const float sz = fpu_mul(fpu_mul(lwc1(pv + 0x1c), int_to_float(static_cast<int32_t>(lbu(ang_slot + 8)))), u2f(0x3ccccccdu));
        spawn_particle_3b(sp, col, rot_b, 0x35, 1, 2, 0, sz);
        vec3_add(V(sp), V(sp), V(sp + 0x10));
    }
    ee_stack_free(0x20);
}

// Spawns a trailing spark particle (type 0x02) for a flying class 1297 energy ball.
// orig: L11:0x003191c0
void pok_3191c0_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x30);
    const float dt = lwc1(0x0015ed6cu);
    rand_sphere_point(sp + 0x10, fpu_mul(lwc1(L11_ADDR(0x001623acu)), dt), fpu_mul(lwc1(L11_ADDR(0x001623b0u)), dt));
    const float dt2 = lwc1(0x0015ed6cu);
    rand_sphere_point(sp + 0x20, fpu_mul(lwc1(L11_ADDR(0x001623acu)), dt2), fpu_mul(lwc1(L11_ADDR(0x001623b0u)), dt2));
    swc1(sp + 0x1c, lwc1(L11_ADDR(0x001623b4u)));
    swc1(sp + 0x2c, lwc1(L11_ADDR(0x001623b8u)));
    const uint32_t c0 = rgba_lerp(lwu(L11_ADDR(0x001623bcu)), lwu(L11_ADDR(0x001623c0u)), rand_range_f(0.0f, 1.0f));
    const uint32_t c1 = rgba_lerp(lwu(L11_ADDR(0x001623bcu)), lwu(L11_ADDR(0x001623c0u)), rand_range_f(0.0f, 1.0f));
    spawn_particle_02(m + 0x10, sp + 0x10, sp + 0x20, c0, c1, lwu(L11_ADDR(0x001623c4u)),
                      lwu(L11_ADDR(0x001623c8u)), lwu(L11_ADDR(0x001623ccu)), lw(L11_ADDR(0x001623a8u)));
    ee_stack_free(0x30);
}

// Class 1297 update: charging/flying energy ball spawned by class 1231.
// orig: L11:0x00318b30
void pok_class1297_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    pok_319028_l11(m);
    const uint32_t state = lbu(m + 0x20);
    if (state == 1) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) != 0) {
            const float n1 = static_cast<float>(frame_scale_i(0x14));
            const float target_sc = lwc1(lwu(m + 0x24) + 0x24);
            approach_f(m + 0x2c, target_sc, fpu_div(target_sc, n1));
            const float n2 = static_cast<float>(frame_scale_i(0x14));
            const float target_sz = lwc1(L11_ADDR(0x00162388u));
            approach_f(pv + 0x1c, target_sz, fpu_div(target_sz, n2));
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x28))) != 0) moby_retire(m);
        return;
    }
    if (state == 2) {
        pok_3191c0_l11(m);
        const uint32_t sp = ee_stack_alloc(0x30);
        lq_sq(sp + 0x20, m + 0x10);
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        const bool hit = (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) != 0) ||
                         (segment_collide_v(V(sp + 0x20), V(m + 0x10), 0, m, nullptr) != 0) ||
                         (mobys_in_sphere(m + 0x10, u2f(0x3eaa7efau), 0, lwu(pv + 0x24), 0) != 0);
        if (hit) {
            explosion_effect(m, pv, m + 0x10, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 1.5f, 2.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
            moby_retire(m);
        }
        ee_stack_free(0x30);
    }
}

// Spawns a pickup crate (`cls`: 0x4c2 or 0x4c4) at `pos` with velocity `vel`, tilted at 45 degrees.
// orig: L11:0x0030f5a8
uint32_t pok_30f5a8_l11(uint32_t pos, uint32_t vel, uint32_t cls) {
    const uint32_t crate = moby_spawn(cls);
    if (crate == 0) return 0;
    sh(crate + 0x32, 0xff);
    sb(crate + 0x30, 0xff);
    sb(crate + 0x31, 1);
    sb(crate + 0x20, 0);
    sb(crate + 0xbc, 0);
    lq_sq(crate + 0x10, pos);
    lq_sq(lwu(crate + 0x78) + 0x10, vel);
    sw(crate + 0x40, 0);
    swc1(crate + 0x44, u2f(0x3f490fdbu));
    swc1(crate + 0x48, rand_angle());
    moby_update_transform(crate);
    moby_copy_hero_color(crate);
    return crate;
}

// Class 1319 update: Blarg bomber (patrols path in states 1/2, respawns in state 3, dogfights hero in state 5/6).
// orig: L11:0x00319838
void pok_class1319_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class1319_hit(m);
    auto hide_state = [&](uint32_t st) {
        sb(m + 0x20, st);
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
    };
    auto explode_and_drop = [&]() {
        explosion_burst(m, m + 0x10, -1, 2.0f, 13.0f);
        pok_30f5a8_l11(m + 0x10, L11_ADDR(0x0015f580u), (rand_mod(2) != 0) ? 0x4c4u : 0x4c2u);
        moby_sound_start(1, 0, m);
    };

    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b800)
    case 0: {  // (0x003198a4)
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        if (lw(pv + 0x88) != 0) {
            hide_state(4);
            break;
        }
        sb(m + 0x20, 1);
        swc1(pv + 0x6c, rand_range_signed_f(u2f(0x3e99999au), 0.5f));
        swc1(pv + 0x70, rand_angle());
        swc1(pv + 0x74, fpu_mul(fpu_mul(rand_range_signed_f(60.0f, 120.0f), u2f(0x3c8efa35u)), lwc1(0x0015ed6cu)));
        swc1(pv + 0x78, rand_range_f(8.0f, 12.0f));
        break;
    }
    case 1: case 2: {  // (0x00319938)
        class1319_trails_record(m);
        const float t = fpu_add(lwc1(pv + 0x68), lwc1(pv + 0x6c));
        if (1.0f < t) {
            swc1(pv + 0x68, fpu_sub(t, 1.0f));
            const int32_t n = lw(lvd::path(lw(pv + 0x60)));
            sw(pv + 0x64, static_cast<uint32_t>(ee_rem(lw(pv + 0x64) + 1, n)));
        } else if (t < 0.0f) {
            swc1(pv + 0x68, fpu_add(t, 1.0f));
            const int32_t n = lw(lvd::path(lw(pv + 0x60)));
            sw(pv + 0x64, static_cast<uint32_t>(ee_rem(lw(pv + 0x64) + n - 1, n)));
        } else {
            swc1(pv + 0x68, t);
        }
        (void)angle_add(lwc1(pv + 0x70), lwc1(pv + 0x74));
        class1319_path_pose(m);
        if (lbu(m + 0x20) == 2) {
            explode_and_drop();
            sh(0x00141c18u, 0xffffu);
            hide_state(3);
        } else if (lbu(m + 0x31) == 0 && vec3_distance(V(m + 0x10), V(0x0013f3d0u)) < 32.0f) {
            hide_state(3);
        }
        break;
    }
    case 3: {  // (0x00319af4)
        const uint32_t sp = ee_stack_alloc(0x10);
        lq_sq(sp, m + 0x10);
        swc1(sp + 0xc, 3.0f);
        const int32_t vis = point_on_screen(m + 0x10, 512.0f);
        ee_stack_free(0x10);
        if (vis != -1) {
            sw(pv + 0x84, 0);
            break;
        }
        if (32.0f < vec3_distance(V(m + 0x10), V(0x0013f3d0u))) {
            const int32_t w = lw(pv + 0x84) + 1;
            sw(pv + 0x84, static_cast<uint32_t>(w));
            if (frame_scale_i(0x3c) < w) {
                sb(m + 0x20, 1);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
                sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
                sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            }
        } else {
            sw(pv + 0x84, 0);
        }
        break;
    }
    case 5: {  // (0x00319bc0)
        const uint32_t sp = ee_stack_alloc(0xd0);
        swc1(sp + 0xc0, 0.0f);
        lq_sq(sp + 0x30, L11_ADDR(0x001677d0u));
        swc1(sp + 0x30, 0.0f);
        const float pitch = lwc1(sp + 0x34);
        if (u2f(0x3e860a92u) < pitch) swc1(sp + 0x34, u2f(0x3e860a92u));
        else if (pitch < u2f(0xbe860a92u)) swc1(sp + 0x34, u2f(0xbe860a92u));
        euler_to_mat3_vu(M(sp + 0x40), V(sp + 0x30));
        vec3_sub(V(sp + 0x20), V(0x0013f3d0u), V(pv + 0xb0));
        lq_sq(pv + 0xb0, 0x0013f3d0u);
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x20));

        const float slot_ang = angle_add(u2f(0x3f490fdbu), fpu_mul(int_to_float(lw(pv + 0x88)), u2f(0x3fc90fdbu)));
        vec3_set_length(V(sp), V(sp + 0x50), fpu_mul(sin_vu(slot_ang), 8.0f));
        vec3_add(V(sp + 0x10), V(L11_ADDR(0x001677c0u)), V(sp));
        vec3_set_length(V(sp), V(sp + 0x60), fpu_mul(cos_vu(slot_ang), 8.0f));
        vec3_add(V(sp + 0x10), V(sp + 0x10), V(sp));
        vec3_set_length(V(sp), V(sp + 0x40), 20.0f);
        vec3_add(V(sp + 0x10), V(sp + 0x10), V(sp));
        vec3_sub(V(sp), V(sp + 0x10), V(m + 0x10));
        const float dist = vec3_length(V(sp));
        const float dt2 = lwc1(0x0015ed70u);
        const float dt = lwc1(0x0015ed6cu);
        const float acc = fpu_add(dt2, dt2);
        const float dec = fpu_mul(dt2, 20.0f);
        if ((lbu(0x0014095fu) & 2u) != 0) {
            seek_1d(sp + 0xc0, pv + 0x6c, dist, acc, dec, dt);
        } else if (lbu(m + 0xbc) != 0) {
            seek_1d(sp + 0xc0, pv + 0x6c, dist, acc, dec, fpu_mul(dt, 15.0f));
        } else {
            seek_1d(sp + 0xc0, pv + 0x6c, dist, acc, dec, fpu_mul(dt, 5.0f));
        }
        vec3_clamp_length(sp, lwc1(pv + 0x6c));
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp));

        if (lbu(m + 0xbc) != 0) {
            if (lbu(m + 0x31) != 0) sw(pv + 0x98, lwu(pv + 0x98) + 1);
            if (vec3_distance(V(sp + 0x10), V(m + 0x10)) < 2.0f || frame_scale_i(0x78) < lw(pv + 0x98)) {
                sb(m + 0xbc, 0);
            }
        } else {
            vec3_sub(V(sp + 0x70), V(0x0013f3d0u), V(m + 0x10));
            const float rate = u2f(0x41490fdbu);
            const float yaw_acc = fpu_mul(lwc1(0x0015ed70u), rate);
            const float yaw_max = fpu_mul(lwc1(0x0015ed6cu), rate);
            yaw_seek(m + 0x48, pv + 0x8c, atan2_f(lwc1(sp + 0x70), lwc1(sp + 0x74)), yaw_acc, yaw_acc, yaw_max);
            const float pitch_acc = fpu_mul(lwc1(0x0015ed70u), rate);
            const float pitch_max = fpu_mul(lwc1(0x0015ed6cu), rate);
            yaw_seek(m + 0x44, pv + 0x90, -atan2_f(vec2_length(V(sp + 0x70)), lwc1(sp + 0x78)), pitch_acc, pitch_acc, pitch_max);

            if (ee_rem(lw(L11_ADDR(0x0015f5ccu)), 10) == 0) {
                moby_sound_start(2, 0, m);
                approach_f(pv + 0x94, 0.0f, 0.25f);
                vec3_set_length(V(sp + 0xb0), V(sp + 0x20), lwc1(pv + 0x94));
                vec3_add(V(sp + 0xb0), V(sp + 0xb0), V(0x0013f3d0u));
                const uint32_t muzzle_off = (ee_rem(lw(L11_ADDR(0x0015f5ccu)), 20) == 0) ? L11_ADDR(0x001f14b0u) : L11_ADDR(0x001f14c0u);
                mat3_transform(V(sp + 0xa0), V(muzzle_off), M(m + 0xc0));
                vec3_add(V(sp + 0xa0), V(sp + 0xa0), V(m + 0x10));
                vec3_sub(V(sp + 0x80), V(sp + 0xb0), V(sp + 0xa0));
                vec3_set_length(V(sp + 0x90), V(sp + 0x80), fpu_mul(lwc1(L11_ADDR(0x001623d0u)), lwc1(0x0015ed6cu)));
                vec3_add(V(sp + 0x90), V(sp + 0x90), V(sp + 0x20));
                if (const uint32_t shot = class1017_launch(m, sp + 0x90, sp + 0xa0, 128.0f, -1.0f, 10.0f); shot != 0) {
                    vec3_sub(V(sp + 0x90), V(sp + 0x90), V(sp + 0x20));
                    swc1(shot + 0x44, -atan2_f(vec2_length(V(sp + 0x90)), lwc1(sp + 0x98)));
                    swc1(shot + 0x48, atan2_f(lwc1(sp + 0x90), lwc1(sp + 0x94)));
                }
            }
            if (lbu(m + 0x31) == 0) hide_state(4);
        }
        ee_stack_free(0xd0);
        break;
    }
    case 6: {  // (0x0031a0c0)
        sh(0x00141c18u, 0xffffu);
        const uint32_t sp = ee_stack_alloc(0x10);
        vec3_sub(V(sp), V(0x0013f3d0u), V(pv + 0xb0));
        lq_sq(pv + 0xb0, 0x0013f3d0u);
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp));
        ee_stack_free(0x10);
        explode_and_drop();
        hide_state(4);
        break;
    }
    default:  // (0x0031a184: state 4 or >=7)
        break;
    }
}

// Class 1350 update: awards Pokitaru's air-combat skill point (0x13d41b) once group pvar +4 is cleared.
// orig: L11:0x0031aa80
void pok_class1350_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x0) == -1) {
        moby_retire(m);
        return;
    }
    const int32_t grp = lw(pv + 0x4);
    if (grp == -1 || moby_group_count_l1(grp, -1) != 0) return;
    if (lbu(0x0013d41bu) != 0) return;
    sb(0x0013d41bu, 1);
    sound_play_indexed(1, 0, 0);
    sub_00263d40(TXT(0x53d6), -1);
}

}  // namespace rac1
