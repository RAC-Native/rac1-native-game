#include <game/ovl/hero_move_mode.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {

constexpr uint32_t P = g::PLAYER;

void update_mode1_clank() {
    swc1(OV(0x0017b85cu), lwc1(0x0015ee18u));
    clank_glow_blink();

    if (lwu(P + 0x1624) == 0) {
        const uint32_t glow = moby_spawn(0x4b4);
        sw(P + 0x1624, glow);
        if (glow != 0) {
            sw(glow + 0x90, 0x801432d7u);
            sw(P + 0x2300, 0x301432d7u);
            uint32_t g = lwu(P + 0x1624);
            V(g + 0x10) = V(P + 0x80);
            V(g + 0x40) = V(P + 0x90);
            sh(g + 0x32, 0x40);
            sb(lwu(P + 0x1624) + 0x31, 1);
            sb(lwu(P + 0x1624) + 0x30, 0);
            sh(lwu(P + 0x1624) + 0x34, 0);
            g = lwu(P + 0x1624);
            swc1(g + 0x2c, fpu_mul(lwc1(g + 0x2c), u2f(0x3fd9999au)));
            g = lwu(P + 0x1624);
            sh(g + 0x34, lhu(g + 0x34) | 0x10u);
            moby_update_transform(lwu(P + 0x1624));
        }
    }

    const uint32_t hm = lwu(P + 0x2080);
    if ((lhu(hm + 0x34) & 1u) == 0) {
        Vec4 pt{};
        moby_chain_point_v(hm, 7, pt);
        V(P + 0x1d80) = pt;
        sub_00207d00();
        if (lwu(P + 0x1624) != 0) {
            Mat4 world{};
            moby_chain_world_one_v(lwu(P + 0x2080), 7, world);
            const uint32_t glow = lwu(P + 0x1624);
            V(glow + 0x10) = world.r[3];
            mat3_copy(M(glow + 0xc0), world);
            Vec4 offset{0.0f, 0.0f, u2f(0x3cd288ceu), 0.0f};
            mat3_transform(offset, offset, M(lwu(P + 0x1624) + 0xc0));
            const uint32_t g2 = lwu(P + 0x1624);
            swc1(g2 + 0x18, fpu_sub(lwc1(g2 + 0x18), offset.z));
            moby_update_transform(lwu(P + 0x1624));
            float scale_mul = (lbu(0x0015edb3u) != 0) ? u2f(0x40466666u) : u2f(0x3fd9999au);
            if (lh(P + 0x22fc) != 0) {
                scale_mul = fpu_mul(scale_mul, u2f(0x3fb33333u));
            }
            const uint32_t g3 = lwu(P + 0x1624);
            const uint32_t cls = lwu(g3 + 0x24);
            approach_f(g3 + 0x2c, fpu_mul(lwc1(cls + 0x24), scale_mul), u2f(0x3ca3d70au));
        }
    }

    uint32_t col_a = 0x281414c8u;
    uint32_t col_b = 0x1e00004bu;
    swc1(P + 0x1628, angle_add(lwc1(P + 0x1628), fpu_mul(lwc1(0x0015ed6cu), u2f(0x403de44eu))));
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(P + 0x22fc))) == 0) {
        swc1(P + 0x1628, angle_add(lwc1(P + 0x1628), fpu_mul(lwc1(0x0015ed6cu), u2f(0x41437a14u))));
        switch (lh(P + 0x22fe)) {
            case 1:
                col_a = 0x2814c814u;
                col_b = 0x1e004b00u;
                break;
            case 2:
                col_a = 0x2814c8c8u;
                col_b = 0x1e004b4bu;
                break;
            case 3:
                col_a = 0x323232fau;
                col_b = 0x1e000078u;
                break;
            case 4:
                col_a = 0x28f01414u;
                col_b = 0x1e4b0000u;
                break;
            default:
                break;
        }
    }

    const float wave = fpu_add(fpu_mul(sin_vu(lwc1(P + 0x1628)), 0.5f), 0.5f);
    const uint32_t target_col = rgba_lerp(col_a, col_b, wave);
    if (lwu(P + 0x1624) != 0) {
        const uint32_t g = lwu(P + 0x1624);
        const uint32_t c = rgba_lerp(lwu(g + 0x90), target_col, fpu_mul(lwc1(0x0015ed60u), u2f(0x3dcccccdu)));
        sw(lwu(P + 0x1624) + 0x90, c);
    }
    sw(P + 0x2300, rgba_lerp(lwu(P + 0x2300), target_col, fpu_mul(lwc1(0x0015ed60u), u2f(0x3dcccccdu))));

    bool update_antenna = true;
    if (lwu(P + 0x1620) == 0) {
        const uint32_t ant = moby_spawn(0x47a);
        sw(P + 0x1620, ant);
        if (ant == 0) {
            update_antenna = false;
        } else {
            V(ant + 0x10) = V(P + 0x80);
            V(ant + 0x40) = V(P + 0x90);
            sh(ant + 0x32, 0x40);
            sb(lwu(P + 0x1620) + 0x31, 1);
            sb(lwu(P + 0x1620) + 0x30, 0);
            sh(lwu(P + 0x1620) + 0x34, 0);
            moby_update_transform(lwu(P + 0x1620));
            if (lwu(P + 0x1620) == 0) {
                update_antenna = false;
            }
        }
    }

    if (update_antenna) {
        Mat4 chain{};
        moby_chain_world_one_v(lwu(P + 0x2080), 5, chain);
        const uint32_t ant = lwu(P + 0x1620);
        V(ant + 0x10) = chain.r[3];
        moby_update_transform(ant);

        Vec4 rot{};
        vec_clear(rot);
        rot.z = -lwc1(P + 0x98);
        Mat4 rot_m{};
        euler_to_mat4_vu(rot_m, rot);
        mat4_mul(chain, chain, rot_m);
        mat3_copy(M(lwu(P + 0x1620) + 0xc0), chain);

        sd(lwu(P + 0x1620) + 0x38, ld(lwu(P + 0x2080) + 0x38));
        const uint32_t state = lwu(P + 0x2084);
        const uint32_t ant2 = lwu(P + 0x1620);
        if (state == 0x4f) {
            if (lbu(ant2 + 0x53) != 1) {
                const int32_t blend = frame_scale_i(0xa);
                moby_anim_set(lwu(P + 0x1620), 1, 0, blend);
            }
        } else {
            if (lbu(ant2 + 0x53) != 0) {
                const int32_t blend = frame_scale_i(0x11);
                moby_anim_set(lwu(P + 0x1620), 0, 0, blend);
            }
        }
    }

    const uint32_t cmd = lwu(P + 0x22c0);
    if (cmd != 0) {
        moby_sound_start(static_cast<int32_t>(cmd + 0x10u), 0, lwu(P + 0x2080));
        sh(P + 0x22fe, lhu(P + 0x22c0));
        sh(P + 0x22fc, static_cast<uint32_t>(frame_scale_i(0x39)));
    }
    sw(P + 0x22c0, 0);
}

void update_mode2_giant_clank() {
    if (lbu(0x0015edb3u) != 0) {
        swc1(OV(0x0017ba6cu), fpu_mul(lwc1(0x0015ee18u), u2f(0x3fb33333u)));
    }
    clank_glow_blink();
    hero_rider_attach();
    const uint32_t hm = lwu(P + 0x2080);
    if ((lhu(hm + 0x34) & 1u) == 0) {
        Vec4 pt{};
        moby_chain_point_v(hm, 6, pt);
        V(P + 0x1d80) = pt;
        sub_00207d00();
    }
}

void update_mode3_hologuise() {
    if (lbu(0x0015edb1u) != 0) {
        swc1(OV(0x0017bbccu), u2f(0x3ff33333u));
    }

    int32_t period = frame_scale_i(0xaa);
    const int32_t alert = lh(P + 0x22e0);
    if (alert == 2) {
        period = frame_scale_i(0x32);
    } else if (alert == 1) {
        period = frame_scale_i(0x5a);
    }

    const float period_f = int_to_float(period);
    const int32_t rem = ee_rem(lw(OV(0x0015f5ccu)), period);
    float ang = fpu_div(static_cast<float>(rem), period_f);
    ang = fpu_add(ang, ang);
    ang = fpu_mul(ang, u2f(0x40490fdbu));
    ang = fpu_add(ang, u2f(0xc0490fdbu));

    const float s = sin_vu(ang);
    uint32_t r = static_cast<uint32_t>(float_to_int(fpu_mul(s, 20.0f))) + 0x32u;
    uint32_t g = static_cast<uint32_t>(float_to_int(fpu_mul(s, 70.0f))) + 0xb4u;
    const uint32_t b = static_cast<uint32_t>(float_to_int(fpu_mul(s, 10.0f))) + 0x14u;

    const uint32_t hm = lwu(P + 0x2080);
    if (static_cast<int32_t>(r) >= 256) r = 0xffu;
    sh(hm + 0x34, lhu(hm + 0x34) | 0x10u);
    if (static_cast<int32_t>(g) >= 256) g = 0xffu;

    uint32_t target_rgba = 0;
    if (lh(P + 0x22e0) == 2) {
        const uint32_t half_r = static_cast<uint32_t>(static_cast<int32_t>(r + (r >> 31)) >> 1);
        target_rgba = 0x80000000u | (b << 16) | (half_r << 8) | g;
    } else {
        target_rgba = 0x80000000u | (b << 16) | (g << 8) | r;
    }

    const uint32_t lerped = rgba_lerp(lwu(lwu(P + 0x2080) + 0x90), target_rgba, u2f(0x3dcccccdu));
    sw(lwu(P + 0x2080) + 0x90, lerped);

    for (uint32_t i = 0; i < 3; ++i) {
        moby_chain_point_v(lwu(P + 0x2080), i, V(P + 0x1d50 + i * 0x10u));
    }

    const uint32_t hm2 = lwu(P + 0x2080);
    const int32_t fade_t = lh(P + 0x22de);
    const uint32_t c0 = lbu(hm2 + 0x90);
    const uint32_t c1 = lbu(hm2 + 0x91);
    const uint32_t c2 = lbu(hm2 + 0x92);
    uint32_t alpha = 0x30u;
    if (fade_t != 0) {
        const float frac = fpu_div(int_to_float(fade_t), int_to_float(0x12));
        alpha = static_cast<uint32_t>(float_to_int(fpu_mul(frac, static_cast<float>(static_cast<int32_t>(alpha)))));
    }
    sw(P + 0x22e4, (alpha << 24) | (c2 << 16) | (c1 << 8) | c0);

    sub_00207d00();
    const uint32_t mode_now = lbu(P + 0x20a4);
    sh(P + 0x22e0, 0);
    if (mode_now == 3) {
        const uint32_t trig = lwu(P + 0x20b8);
        if (trig != 0 && trig != 0x1fu) {
            if (lh(P + 0x22de) == 0) {
                hero_sound_start(0x19, 0);
                sh(P + 0x22dc, 1);
                sh(P + 0x22de, 0x12);
            }
        } else {
            sw(P + 0x20b8, 0);
            if (lbu(P + 0x12e7) != 0 && lwu(P + 0x300) != 0 && lbu(0x0013d4dcu) != 0 && lh(P + 0x22de) == 0) {
                hero_sound_start(0x19, 0);
                sh(P + 0x22dc, 1);
                sh(P + 0x22de, 0x12);
            }
        }
    }
}

}  // namespace

void hero_move_mode_ex(bool has_mode_1, bool has_mode_2) {
    hero_anim_advance();
    hero_tick();
    hero_surface_update();
    if (has_mode_1 && lbu(P + 0x20a4) == 1) {
        hero_ledge_grab_check();
        hero_body_lean();
    }
    hero_input_update();
    if (lbu(P + 0x20a4) == 0) {
        return;
    }
    hero_delayed_sounds();
    bone_nodes_update();
    if (lbu(P + 0x20a4) == 0) {
        return;
    }
    hero_moby_sync();
    sub_00207c48();
    sub_00207a58();

    if (has_mode_1 && lbu(P + 0x20a4) == 1) {
        update_mode1_clank();
    }

    map_reveal(P + 0x80);

    if (has_mode_2 && lbu(P + 0x20a4) == 2) {
        update_mode2_giant_clank();
    }

    if (lbu(P + 0x20a4) == 3) {
        update_mode3_hologuise();
    }

    hero_shadow_update();
    moby_update_light(lwu(P + 0x2080));
}

// orig: 0x002062b0
void hero_move_mode() {
    hero_move_mode_ex(true, true);
}

// orig: L1:0x00228000 L2:0x00217158 L3:0x001ffa88 L5:0x00234dd8
void hero_move_mode_l1() {
    hero_move_mode_ex(false, false);
}

}  // namespace rac1
