// Oltanis (level 14), class 31: the hovering bombers. Hidden (1) until a target enters its zone (the polygon of path
// +0xd4), a bomber flies in along its entry path (+0xd0, or +0x150 from the other station) (2), hovers at its station
// (4) turning its mortar toward each point of its drop path (+0xd8) (5) and lobs a mortar bomb (class 1193) at it (6);
// it crosses to the other station along +0xdc when its target comes near (7) and flies back out when the target has
// left (3). Shot down, it sheds both wings (class 81) and explodes (9).
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

#include <cstdint>

namespace rac1 {

namespace {

void stop_bomber_sound(uint32_t m, uint32_t pv) {
    const int32_t ch = lw(pv + 0x108);
    if (ch != -1) {
        const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
        if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) {
            sub_0028d918(ch);
        }
    }
    sw(pv + 0x108, static_cast<uint32_t>(-1));
}

// orig: L14:0x002b5290 -- hit handling and damage flash for class 31
void class31_hit_update(uint32_t m) {
    const uint8_t cur_st = lbu(m + 0x20);
    if (cur_st == 0 || cur_st == 9) return;

    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
    uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    if (hit != 0) {
        const uint32_t src = lwu(hit + 0x20);
        if (src != 0) {
            const int16_t src_cls = lh(src + 0xa6);
            if (src_cls == lh(m + 0xa6) || src_cls == 0x370) {
                hit = 0;
            }
        }
    }

    int32_t st = 0;
    float dmg = 0.0f;
    uint32_t react = static_cast<uint32_t>(moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4));
    if (st != 1 && lbu(m + 0x20) != 9) {
        const float hp = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, hp);
        if (hp <= 0.0f) {
            react = 1;
        }
        // jump table L14:0x001fbfb0 (12 entries)
        switch (react) {
        case 0:
        case 11:  // (0x002b53f0)
            break;
        case 1:
        case 2:  // (0x002b53c4)
            sb(m + 0x20, 9);
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xefffu));
            sb(pv + 0x67, 0xfa);
            moby_drop_bolts(m, 0, -1);
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:  // (0x002b5394)
            moby_anim_set(m, 2, 0, 5);
            swc1(m + 0x58, 1.75f);
            sb(pv + 0x67, 0xfa);
            break;
        case 9:
        case 10:  // (0x002b53b8)
            sb(pv + 0x67, 0xfa);
            break;
        default:
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }

    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// orig: L14:0x002b5428 -- hover bob and bank sway for class 31
void class31_hover_bob(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    if (st == 0 || st == 9 || st == 1) return;

    const uint32_t pv = lwu(m + 0x78);
    float bank_target = lwc1(L14_ADDR(0x00161540));
    if (static_cast<uint32_t>(st - 2u) < 2u || st == 7) {
        bank_target = fpu_mul(bank_target, 1.5f);
    } else {
        spring_to(pv + 0x168, pv + 0x16c, lwc1(L14_ADDR(0x0016153c)),
                  u2f(0x3b03126fu), u2f(0x3e4ccccdu), 0.0f);
        sub_00262b00(m, pv + 0xc0, pv + 0xc4, lwc1(L14_ADDR(0x00161538)), lwc1(pv + 0x168));
    }
    spring_to(pv + 0x160, pv + 0x164, bank_target, u2f(0x3b449ba6u), u2f(0x3e4ccccdu), 0.0f);
    sub_00262b80(m, pv + 0xc8, pv + 0xcc, lwc1(pv + 0x160), lwc1(L14_ADDR(0x00161544)),
                 lwc1(L14_ADDR(0x00161548)));
}

// orig: L14:0x002b5530 -- refresh target selection against the trigger polygon path
void class31_target_update(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    if (st == 0 || st == 9) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t zone_path = lvd::path(lw(pv + 0xd4));
    target_select(m, pv + 0x70, 0, 0, zone_path + 0x10u, lw(zone_path), 255.0f);
    if (lwu(pv + 0xb0) == 0) {
        sw(pv + 0xb0, lwu(0x001413d0u));
    }
}

// orig: L14:0x002b55b0 -- precompute segment lengths on the three flight paths
void class31_paths_measure(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    auto measure = [](uint32_t path) {
        const int32_t n = lw(path) - 1;
        for (int32_t i = 0; i < n; ++i) {
            const uint32_t p0 = path + 0x10u + (static_cast<uint32_t>(i) << 4);
            const uint32_t p1 = path + 0x20u + (static_cast<uint32_t>(i) << 4);
            lvd::set_path_f32(path + 0x1cu + (static_cast<uint32_t>(i) << 4), vec3_distance(V(p0), V(p1)));
        }
    };
    measure(lvd::path(lw(pv + 0xd0)));
    measure(lvd::path(lw(pv + 0x150)));
    measure(lvd::path(lw(pv + 0xdc)));
    sw(pv + 0x10c, 0);
}

// orig: L14:0x002b5730 -- advance along a path, spring position, and yaw toward velocity
int32_t class31_follow_path(uint32_t m, uint32_t path, uint32_t seg_addr, uint32_t dist_addr,
                            uint32_t vel_addr, uint32_t yaw_vel_addr, float step,
                            float pos_k, float pos_d, float pos_lim,
                            float yaw_k, float yaw_d, float yaw_lim) {
    int32_t seg = lw(seg_addr);
    float dist = lwc1(dist_addr);
    Vec4 target{};
    const uint32_t done = static_cast<uint32_t>(path_advance_v(path, target, &seg, &dist, 0, step));
    sw(seg_addr, static_cast<uint32_t>(seg));
    swc1(dist_addr, dist);

    spring_to(m + 0x10, vel_addr, target.x, pos_k, pos_d, pos_lim);
    spring_to(m + 0x14, vel_addr + 4, target.y, pos_k, pos_d, pos_lim);
    spring_to(m + 0x18, vel_addr + 8, target.z, pos_k, pos_d, pos_lim);

    const float yaw = atan2_f(lwc1(vel_addr), lwc1(vel_addr + 4));
    yaw_turn_damped(m, yaw_vel_addr, yaw, yaw_k, yaw_d, yaw_lim);
    return (done != 0) ? 1 : 0;
}

// orig: L14:0x002b5850 -- hold hover position and yaw while attacking
void class31_hold_hover(uint32_t m, uint32_t target_addr, float target_yaw,
                        float pos_k, float pos_d, float pos_lim,
                        float yaw_k, float yaw_d, float yaw_lim) {
    const uint8_t st = lbu(m + 0x20);
    if (static_cast<uint32_t>(st - 4u) >= 3u && st != 8) return;

    const uint32_t pv = lwu(m + 0x78);
    spring_to(m + 0x10, pv + 0xf0, lwc1(target_addr), pos_k, pos_d, pos_lim);
    spring_to(m + 0x14, pv + 0xf4, lwc1(target_addr + 4), pos_k, pos_d, pos_lim);
    spring_to(m + 0x18, pv + 0xf8, lwc1(target_addr + 8), pos_k, pos_d, pos_lim);
    yaw_turn_damped(m, pv + 0xe4, target_yaw, yaw_k, yaw_d, yaw_lim);
}

// orig: L14:0x002b5968 -- spawn mortar muzzle smoke, sparks, and flash effect
void class31_mortar_muzzle_fx(uint32_t m, const Vec4& joint_pos, float rel_yaw) {
    const float ang = angle_add(rel_yaw, lwc1(m + 0x48));
    const float r = u2f(0x3f4ccccdu);  // 0.8f
    Vec4 dir{fpu_mul(cos_vu(ang), r), fpu_mul(sin_vu(ang), r), 0.0f, 0.0f};

    const uint32_t buf = ee_stack_alloc(0x20);
    const uint32_t pos_addr = buf;
    const uint32_t base_addr = buf + 0x10u;
    vec3_add(V(pos_addr), joint_pos, dir);

    for (int k = 0; k < 6; ++k) {
        int32_t spin = rand_mod(2);
        if (rand_mod(2) != 0) spin = -spin;
        const float sz = rand_range_f(40000.0f, 150000.0f);
        const uint32_t c = static_cast<uint32_t>(rand_range(0xa, 0x7f));
        const uint32_t col = 0x7f000000u | (c << 16) | (c << 8) | c;
        const uint32_t p = spawn_particle_17(pos_addr, spin, OV(0x0015f580), col,
                                             u2f(0x3e4ccccdu), 1.0f, u2f(0x3f8147aeu), sz);
        if (p != 0) {
            sh(p + 0xa, static_cast<uint16_t>(frame_scale_i(120)));
            sw(p + 0x24, 2);
            sb(p + 0x2a, 0x7f);
            sb(p + 0x2b, lbu(p + 0xa));
        }
    }

    const Vec4 axis = dir;
    vec3_scale(dir, dir, 0.0625f);
    dir.z = rand_range_f(u2f(0x3ccccccdu), u2f(0x3d75c28fu));
    V(base_addr) = V(pos_addr);

    for (int i = 0; i < 16; ++i) {
        const float jitter = rand_range_signed_f(0.0f, 0.5f);
        const float rot = fpu_add(fpu_mul(static_cast<float>(i), u2f(0x3f20d845u)), jitter);
        Vec4 vel{};
        vec_rotate_axis(vel, dir, axis, rot);
        swc1(pos_addr + 0, fpu_add(lwc1(base_addr + 0), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))));
        swc1(pos_addr + 4, fpu_add(lwc1(base_addr + 4), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))));
        swc1(pos_addr + 8, fpu_add(lwc1(base_addr + 8), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))));
        spawn_particle_13_v(V(pos_addr), vel);
    }

    effect_spawn(L14_ADDR(0x001d88d0), base_addr, 0, 0);
    ee_stack_free(0x20);
}

// orig: L14:0x002b5c30 -- set path cursor to the end of the current entry path for retreating
void class31_prepare_retreat(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t pidx = (lw(pv + 0x10c) != 0) ? lw(pv + 0x150) : lw(pv + 0xd0);
    const uint32_t path = lvd::path(pidx);
    const int32_t last_seg = lw(path) - 2;
    sw(pv + 0xe8, static_cast<uint32_t>(last_seg));
    sw(pv + 0xe4, 0);
    swc1(pv + 0xec, lwc1(path + 0x1cu + (static_cast<uint32_t>(last_seg) << 4)));
    vec_clear(V(pv + 0xf0));
}

// orig: L14:0x002b5c98 -- hide bomber and stop its engine sound
void class31_hide(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(m + 0x94, 0);
    sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) | 0x41u));
    stop_bomber_sound(m, pv);
}

// orig: L14:0x002b5d18 -- place bomber at the start of its active approach path
void class31_enter_approach(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 0xe8, 0);
    swc1(pv + 0x160, fpu_mul(lwc1(L14_ADDR(0x00161540)), 1.5f));
    sw(pv + 0xec, 0);
    sw(pv + 0x164, 0);
    sw(pv + 0x168, 0);
    sw(pv + 0x16c, 0);
    sw(pv + 0xc0, 0);
    sw(pv + 0xc4, 0);

    const int32_t pidx = (lw(pv + 0x10c) != 0) ? lw(pv + 0x150) : lw(pv + 0xd0);
    const uint32_t path = lvd::path(pidx);
    V(m + 0x10) = V(path + 0x10u);
    Vec4 diff{};
    vec3_sub(diff, V(path + 0x20u), V(m + 0x10));
    swc1(m + 0x48, atan2_f(diff.x, diff.y));
    sw(pv + 0xe4, 0);
    vec_clear(V(pv + 0xf0));
}

// orig: L14:0x002b5dd8 -- spawn thruster flame and glow particles behind the bomber
void class31_exhaust_fx(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    if (st < 2 || st == 9) return;

    const uint32_t pos_addr = ee_stack_alloc(0x10);
    Vec4 off{};
    vec3_scale(off, V(m + 0xc0), -1.25f);
    vec3_add(V(pos_addr), V(m + 0x10), off);

    for (int k = 0; k < 2; ++k) {
        int32_t spin = rand_mod(16);
        if (rand_mod(2) != 0) spin = -spin;
        const uint32_t p = spawn_particle_17(pos_addr, spin, OV(0x0015f580), 0x7f3030ffu,
                                             u2f(0x3e4ccccdu), 1.0f, 0.75f, 160000.0f);
        if (p != 0) {
            sh(p + 0xa, static_cast<uint16_t>(frame_scale_i(6)));
            sw(p + 0x24, 2);
            sb(p + 0x2a, 0x7f);
            sb(p + 0x2b, lbu(p + 0xa));
        }
    }

    vec3_scale(off, V(m + 0xc0), -1.0f);
    vec3_add(V(pos_addr), V(m + 0x10), off);
    int32_t spin = 16;
    int32_t life = frame_scale_i(2);
    float sz = 100000.0f;
    for (int k = 0; k < 3; ++k) {
        const uint32_t p = spawn_particle_17(pos_addr, spin, OV(0x0015f580), 0x7fffffffu,
                                             u2f(0x3d4ccccdu), 1.0f, 1.0f, sz);
        if (p != 0) {
            sh(p + 0xa, static_cast<uint16_t>(life));
            sb(p + 0x8, static_cast<uint8_t>(rand_mod(0xff)));
            sw(p + 0x24, 2);
            sb(p + 0x2a, 0x7f);
            sb(p + 0x2b, lbu(p + 0xa));
        }
        spin = -spin;
        life <<= 1;
        sz = fpu_sub(sz, 20000.0f);
    }
    ee_stack_free(0x10);
}

// orig: L14:0x002bb310 -- spawn a class 0x51 detached wing debris moby
uint32_t class31_spawn_debris(uint32_t m, const Vec4& pos, float yaw) {
    const uint32_t deb = moby_spawn(0x51);
    if (deb == 0) return 0;

    const uint32_t dpv = lwu(deb + 0x78);
    V(deb + 0x10) = pos;
    swc1(deb + 0x48, yaw);
    sb(deb + 0x31, 1);
    sb(deb + 0x20, 0);
    sh(deb + 0x32, 0xff);
    sb(deb + 0x30, 0xff);

    Vec4 away{};
    vec3_sub(away, pos, V(m + 0x10));
    away.z = 0.0f;
    const float fscale = lwc1(0x0015ed60u);
    const float spd = rand_range_f(fpu_mul(fscale, u2f(0x3d99999au)), fpu_mul(fscale, u2f(0x3e4ccccdu)));
    vec3_set_length(away, away, spd);
    Vec4 cross_unused{};
    vec3_cross_ba(cross_unused, V(0x0013f5e0u), away);
    const float deflect = rand_range_signed_f(0.0f, u2f(0x3f490fdbu));
    vec_rotate_axis(V(dpv), away, V(0x0013f5e0u), deflect);
    const float fscale2 = lwc1(0x0015ed60u);
    swc1(dpv + 0x8, rand_range_f(fpu_mul(fscale2, u2f(0x3d4ccccdu)), fpu_mul(fscale2, u2f(0x3ea3d70au))));

    const float spin_max = u2f(0x3d0efa35u);  // 2 deg
    swc1(dpv + 0x14, rand_range_signed_f(0.0f, spin_max));
    swc1(dpv + 0x18, rand_range_signed_f(0.0f, spin_max));
    swc1(dpv + 0x1c, rand_range_signed_f(0.0f, spin_max));
    sw(dpv + 0x20, m);
    sh(dpv + 0x12, static_cast<uint16_t>(-1));
    sh(dpv + 0x10, static_cast<uint16_t>(frame_scale_i(200)));
    moby_update_transform(deb);
    return deb;
}

// orig: L14:0x003014e0 -- spawn a class 0x4a9 mortar bomb
uint32_t class31_spawn_mortar(uint32_t owner, const Vec4& pos, const Vec4& vel, int32_t fuse, float grav) {
    const uint32_t bomb = moby_spawn(0x4a9);
    if (bomb == 0) return 0;

    const uint32_t bpv = lwu(bomb + 0x78);
    sb(bomb + 0x30, 0xff);
    sh(bomb + 0x32, 0xff);
    sb(bomb + 0x31, 1);
    sb(bomb + 0x20, 0);
    sw(bpv + 0x24, owner);
    V(bomb + 0x10) = pos;
    V(bpv + 0x10) = vel;
    swc1(bpv + 0x1c, grav);
    sw(bpv + 0x20, static_cast<uint32_t>(fuse));
    sh(bpv + 0x28, lhu(L14_ADDR(0x0016208c)));
    sh(bpv + 0x2a, 0);
    sw(bomb + 0x40, 0);
    sw(bomb + 0x44, 0);
    swc1(bomb + 0x48, atan2_f(vel.x, vel.y));
    swc1(bomb + 0x2c, fpu_mul(lwc1(bomb + 0x2c), lwc1(L14_ADDR(0x00162098))));
    moby_update_transform(bomb);
    return bomb;
}

}  // namespace

// orig: L14:0x002b46e8
void olt_class31_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x52) == lbu(m + 0x53)) {
        const uint8_t anim = lbu(m + 0x52);
        if (anim == 2) {
            moby_anim_set(m, 0, 0, 10);
        } else if (anim != 0) {
            moby_set_anim(m, 0, 0);
        }
    }

    class31_hit_update(m);
    class31_target_update(m);
    class31_hold_hover(m, pv + 0x180, lwc1(pv + 0x17c),
                       lwc1(pv + 0x170), lwc1(pv + 0x174), lwc1(pv + 0x178),
                       lwc1(pv + 0x154), lwc1(pv + 0x158), lwc1(pv + 0x15c));
    class31_hover_bob(m);
    class31_exhaust_fx(m);

    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 40.0f) {
        const uint8_t st = lbu(m + 0x20);
        if (st != 0 && st != 1 && st != 9) {
            moby_floor_band(m);
        }
    }

    // jump table L14:0x001fbf80 (10 entries)
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002b4834) init
        auto bad_path = [&](int32_t idx) { return idx == -1 || lw(lvd::path(idx)) == 0; };
        if (bad_path(lw(pv + 0xd0)) || bad_path(lw(pv + 0xd4)) ||
            bad_path(lw(pv + 0xd8)) || bad_path(lw(pv + 0x150))) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        const uint32_t entry_path = lvd::path(lw(pv + 0xd0));
        V(m + 0x10) = V(entry_path + 0x10u);
        Vec4 diff{};
        vec3_sub(diff, V(entry_path + 0x20u), V(m + 0x10));
        swc1(m + 0x48, atan2_f(diff.x, diff.y));
        sw(pv + 0x164, 0);
        sw(pv + 0x168, 0);
        sw(pv + 0x16c, 0);
        swc1(pv + 0x160, lwc1(L14_ADDR(0x00161540)));
        class31_paths_measure(m);
        sw(pv + 0x108, static_cast<uint32_t>(-1));
        sb(m + 0x30, 0x80);
        sb(m + 0x20, 1);
        class31_hide(m);
        mem_fill32(pv + 0x110, 0, 0x40);
        sub_0024f590(m, 0, pv + 0x110);
        sb(m + 0x7f, 0x20);
        break;
    }
    case 1: {  // (0x002b49d8) hidden, waiting for target
        if (lw(pv + 0xb4) == 2) break;
        sb(m + 0x20, 2);
        sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xffbeu));
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        class31_enter_approach(m);
        break;
    }
    case 2: {  // (0x002b4a10) flying in along approach path
        const uint32_t path = lvd::path(lw(pv + 0x10c) != 0 ? lw(pv + 0x150) : lw(pv + 0xd0));
        if (lw(pv + 0xb4) == 2) {
            sb(m + 0x20, 3);
            break;
        }
        const float step = fpu_mul(lwc1(L14_ADDR(0x0016154c)), lwc1(0x0015ed60u));
        const int32_t done = class31_follow_path(
            m, path, pv + 0xe8, pv + 0xec, pv + 0xf0, pv + 0xe4, step,
            lwc1(L14_ADDR(0x00161574)), lwc1(L14_ADDR(0x00161578)), lwc1(L14_ADDR(0x0016157c)),
            lwc1(L14_ADDR(0x00161580)), lwc1(L14_ADDR(0x00161584)), lwc1(L14_ADDR(0x00161588)));
        if (done == 0) break;

        V(pv + 0x180) = V(path + (static_cast<uint32_t>(lw(path)) << 4));
        const uint32_t cross_path = lvd::path(lw(pv + 0xdc));
        const Vec4 p_first = V(cross_path + 0x10u);
        const Vec4 p_last = V(cross_path + (static_cast<uint32_t>(lw(cross_path)) << 4));
        Vec4 diff{};
        if (lw(pv + 0x10c) == 0) {
            vec3_sub(diff, p_last, p_first);
        } else {
            vec3_sub(diff, p_first, p_last);
        }
        swc1(pv + 0x17c, atan2_f(diff.x, diff.y));
        swc1(pv + 0x170, lwc1(L14_ADDR(0x00161574)));
        swc1(pv + 0x174, lwc1(L14_ADDR(0x00161578)));
        swc1(pv + 0x178, lwc1(L14_ADDR(0x0016157c)));
        swc1(pv + 0x154, lwc1(L14_ADDR(0x00161580)));
        swc1(pv + 0x158, lwc1(L14_ADDR(0x00161584)));
        swc1(pv + 0x15c, lwc1(L14_ADDR(0x00161588)));
        sb(m + 0x20, 4);
        sw(pv + 0x104, 0);
        sh(pv + 0x102, 0);
        break;
    }
    case 3: {  // (0x002b4b88) retreating along approach path
        const uint32_t path = lvd::path(lw(pv + 0x10c) != 0 ? lw(pv + 0x150) : lw(pv + 0xd0));
        if (lw(pv + 0xb4) != 2) {
            sb(m + 0x20, 2);
            break;
        }
        const float step = -fpu_mul(lwc1(L14_ADDR(0x00161554)), lwc1(0x0015ed60u));
        const int32_t done = class31_follow_path(
            m, path, pv + 0xe8, pv + 0xec, pv + 0xf0, pv + 0xe4, step,
            lwc1(L14_ADDR(0x0016158c)), lwc1(L14_ADDR(0x00161578)), lwc1(L14_ADDR(0x0016157c)),
            lwc1(L14_ADDR(0x00161590)), lwc1(L14_ADDR(0x00161584)), lwc1(L14_ADDR(0x00161588)));
        if (done != 0) {
            sb(m + 0x20, 1);
            class31_hide(m);
        }
        break;
    }
    case 4: {  // (0x002b4c20) hovering at station, deciding to cross, retreat, or aim
        const float denom = int_to_float(frame_scale_i(lw(L14_ADDR(0x00161560))));
        const float frac = fpu_div(static_cast<float>(lh(pv + 0x102)), denom);
        quat_from_axis_angle(V(pv + 0x120), 2, cos_ease(0.0f, lwc1(pv + 0x104), frac));

        if (vec2_distance(V(m + 0x10), V(lwu(pv + 0xb0) + 0x10u)) < lwc1(L14_ADDR(0x00161570))) {
            if (lw(pv + 0x10c) == 0) {
                swc1(pv + 0xec, 0.0f);
                sw(pv + 0xe8, 0);
            } else {
                const uint32_t cross_path = lvd::path(lw(pv + 0xdc));
                const int32_t last_seg = lw(cross_path) - 2;
                sw(pv + 0xe8, static_cast<uint32_t>(last_seg));
                swc1(pv + 0xec, lwc1(cross_path + 0x1cu + (static_cast<uint32_t>(last_seg) << 4)));
            }
            sb(m + 0x20, 7);
            sw(pv + 0xe4, 0);
            vec_clear(V(pv + 0xf0));
            break;
        }
        if (lw(pv + 0xb4) == 2) {
            sb(m + 0x20, 3);
            class31_prepare_retreat(m);
            break;
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x102))) != 0 && lbu(m + 0x53) != 2) {
            sb(m + 0x20, 5);
            sh(pv + 0x102, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x0016155c)))));
            swc1(pv + 0x104, 0.0f);
            sh(pv + 0x100, 0);
        }
        break;
    }
    case 5: {  // (0x002b4eac) aiming turret at next bomb drop point
        const uint32_t drop_path = lvd::path(lw(pv + 0xd8));
        const uint32_t pt = drop_path + (static_cast<uint32_t>(lh(pv + 0x100)) << 4);
        const float aim_yaw = angle_sub(atan2_f(fpu_sub(lwc1(pt + 0x10), lwc1(m + 0x10)),
                                                fpu_sub(lwc1(pt + 0x14), lwc1(m + 0x14))),
                                        lwc1(m + 0x48));
        const float denom = int_to_float(frame_scale_i(lw(L14_ADDR(0x0016155c))));
        const float frac = fpu_div(static_cast<float>(lh(pv + 0x102)), denom);
        const float cur_yaw = cos_ease(aim_yaw, lwc1(pv + 0x104), frac);
        quat_from_axis_angle(V(pv + 0x120), 2, cur_yaw);

        if (lbu(m + 0x53) == 2) {
            sb(m + 0x20, 4);
            sh(pv + 0x102, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x00161560)))));
            break;
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x102))) != 0) {
            swc1(pv + 0x104, cur_yaw);
            sb(m + 0x20, 6);
            moby_set_anim(m, 1, 0);
        }
        break;
    }
    case 6: {  // (0x002b4f80) firing mortar bomb
        if (anim_crossed(m, 3.0f) == 0) break;

        const float flight = static_cast<float>(frame_scale_i(lw(L14_ADDR(0x00161564))));
        quat_from_axis_angle(V(pv + 0x120), 2, lwc1(pv + 0x104));
        Vec4 muzzle{};
        moby_chain_point_v(m, 1, muzzle);

        const uint32_t drop_path = lvd::path(lw(pv + 0xd8));
        const int16_t idx = lh(pv + 0x100);
        const uint32_t pt = drop_path + 0x10u + (static_cast<uint32_t>(idx) << 4);
        Vec4 vel{};
        vec3_sub(vel, V(pt), muzzle);
        vel.z = 0.0f;
        vec3_scale(vel, vel, fpu_div(1.0f, flight));

        const float grav = fpu_mul(lwc1(L14_ADDR(0x00161568)), lwc1(0x0015ed70u));
        const float drop_z = lwc1(drop_path + (static_cast<uint32_t>(lh(pv + 0x100)) << 4) + 0x18u);
        const float vz = fpu_div(-fpu_add(fpu_sub(muzzle.z, drop_z),
                                          fpu_mul(fpu_mul(-grav, fpu_mul(flight, flight)), 0.5f)),
                                 flight);
        swc1(pv + 0x48, vz);  // original stores vz to pv+0x48 (vel.z stays 0)

        const int32_t fuse = frame_scale_i(lw(L14_ADDR(0x0016156c)));
        class31_spawn_mortar(m, muzzle, vel, fuse, grav);
        class31_mortar_muzzle_fx(m, muzzle, lwc1(pv + 0x104));

        const int16_t next_idx = static_cast<int16_t>(lh(pv + 0x100) + 1);
        sh(pv + 0x100, static_cast<uint16_t>(next_idx));
        if (next_idx < lw(lvd::path(lw(pv + 0xd8)))) {
            sh(pv + 0x102, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x0016155c)))));
            sb(m + 0x20, 5);
        } else {
            sb(m + 0x20, 4);
            sh(pv + 0x102, static_cast<uint16_t>(frame_scale_i(lw(L14_ADDR(0x00161560)))));
        }
        break;
    }
    case 7: {  // (0x002b4d48) crossing between the two stations
        float step = fpu_mul(lwc1(L14_ADDR(0x00161550)), lwc1(0x0015ed60u));
        if (lw(pv + 0x10c) == 1) step = -step;
        const uint32_t cross_path = lvd::path(lw(pv + 0xdc));
        const int32_t done = class31_follow_path(
            m, cross_path, pv + 0xe8, pv + 0xec, pv + 0xf0, pv + 0xe4, step,
            lwc1(L14_ADDR(0x0016158c)), lwc1(L14_ADDR(0x00161578)), lwc1(L14_ADDR(0x0016157c)),
            lwc1(L14_ADDR(0x00161590)), lwc1(L14_ADDR(0x00161584)), lwc1(L14_ADDR(0x00161588)));
        if (done == 0) break;

        sb(m + 0x20, 4);
        sw(pv + 0x10c, (lw(pv + 0x10c) != 0) ? 0u : 1u);
        const uint32_t cp = lvd::path(lw(pv + 0xdc));
        Vec4 p_from{}, p_to{};
        if (lw(pv + 0x10c) == 0) {
            p_from = V(cp + 0x10u);
            p_to = V(cp + (static_cast<uint32_t>(lw(cp)) << 4));
        } else {
            p_from = V(cp + (static_cast<uint32_t>(lw(cp)) << 4));
            p_to = V(cp + 0x10u);
        }
        swc1(pv + 0x170, lwc1(L14_ADDR(0x0016158c)));
        swc1(pv + 0x174, lwc1(L14_ADDR(0x00161578)));
        swc1(pv + 0x178, lwc1(L14_ADDR(0x0016157c)));
        swc1(pv + 0x154, lwc1(L14_ADDR(0x00161590)));
        swc1(pv + 0x158, lwc1(L14_ADDR(0x00161584)));
        swc1(pv + 0x15c, lwc1(L14_ADDR(0x00161588)));
        Vec4 diff{};
        vec3_sub(diff, p_to, p_from);
        swc1(pv + 0x17c, atan2_f(diff.x, diff.y));
        V(pv + 0x180) = p_from;
        break;
    }
    case 9: {  // (0x002b5110) destroyed -> spawn wing debris and explode
        Vec4 wing{};
        moby_chain_point_v(m, 2, wing);
        class31_spawn_debris(m, wing, lwc1(m + 0x48));
        moby_chain_point_v(m, 3, wing);
        class31_spawn_debris(m, wing, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
        explosion_effect(m, pv + 0x40, m + 0x10, 5, 2, 6, 1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        stop_bomber_sound(m, pv);
        moby_retire(m);
        return;
    }
    case 8:  // (0x002b521c)
    default:
        break;
    }

    const uint8_t st_after = lbu(m + 0x20);
    if (st_after >= 2 && st_after != 9 && sub_0028d8c0(m, lw(pv + 0x108)) == 0) {
        sw(pv + 0x108, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    }
}

}  // namespace rac1
