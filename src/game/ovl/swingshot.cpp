// Ported overlay code: the Swingshot (item 0x0c, hand class 0xd0, update 0x2c6158), grapple spawn (0x2c6f88),
// and rope draw callback (0x2c5dc8).
#include <game/ovl/swingshot.h>
#include <game/ovl/swingshot_rope_tr.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t HERO_POS = 0x0013f3d0u;
}  // namespace

// orig: 0x002c6f88
uint32_t swingshot_spawn_grapple(uint32_t m, float pf12) {
    const uint32_t g = moby_spawn(0xd1);
    if (g == 0) return 0;

    const uint32_t gpv = lwu(g + 0x78);
    swc1(gpv + 0x4, pf12);
    sw(gpv + 0x0, m);
    sh(g + 0x32, 0xff);
    sb(g + 0x30, 0xff);
    sb(g + 0x31, 1);
    moby_set_color_hi(g, 0x80, 0x80, 0x80);
    moby_chain_point_v(m, 0, V(g + 0x10));

    const uint32_t g_anim = lbu(g + 0x53);
    V(g + 0xc0) = V(m + 0xc0);
    V(g + 0xd0) = V(m + 0xd0);
    V(g + 0xe0) = V(m + 0xe0);
    if (g_anim != 0) {
        moby_anim_set(g, 0, 0, frame_scale_i(0xa));
    }
    moby_update_transform(g);
    const uint32_t flags = lhu(g + 0x34);
    sb(g + 0x20, 0);
    sh(g + 0x34, flags | 0x4u);
    return g;
}

// orig: 0x002c6158
void swingshot_update(uint32_t m) {
    if (m == 0) return;
    if (lw(P + 0x2084) == 1) {
        (void)hero_set_state(0x1e, 1);
    }

    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    uint32_t g = lwu(pv + 0x14);
    uint32_t gpv = 0;
    if (g == 0) {
        g = swingshot_spawn_grapple(m, fpu_mul(lwc1(0x0015ed6cu), 24.0f));
        sw(pv + 0x14, g);
        if (g != 0) {
            gpv = lwu(lwu(pv + 0x14) + 0x78);
        }
    } else {
        gpv = lwu(g + 0x78);
    }

    const int32_t hst0 = lw(P + 0x2084);
    if ((hst0 == 1 || hst0 == 0x1e) && lw(P + 0x198) >= 2) {
        cam_effect_add(m, 0xff0000ffu, nullptr, 0x24, -1, 1.0f, 0.0f, 90.0f);
        if (lw(P + 0x968) != 0 && lwu(P + 0x964) != 0) {
            cam_effect_add(m, 0xff00ff00u, &V(lwu(P + 0x964) + 0x10u), 0x24, -1, 0.5f, 0.0f, 90.0f);
        } else if (lwu(P + 0x990) != 0) {
            cam_effect_add(m, 0xff00ff00u, &V(lwu(P + 0x990) + 0x10u), 0x24, -1, 0.5f, 0.0f, 90.0f);
        }
    }

    switch (lbu(m + 0x20)) {
    case 0:
    case 1: {
        if ((lbu(m + 0x70) & 0x2u) != 0) {
            if (lbu(m + 0x53) != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 1);
        }
        const uint32_t g_cur = lwu(pv + 0x14);
        if (g_cur != 0) {
            moby_chain_point_v(m, 0, V(g_cur + 0x10));
            const uint32_t g2 = lwu(pv + 0x14);
            V(g2 + 0xc0) = V(m + 0xc0);
            V(g2 + 0xd0) = V(m + 0xd0);
            V(g2 + 0xe0) = V(m + 0xe0);
        }
        if (lw(P + 0x2084) == 0x24 || (lw(P + 0x208c) == 0xf && lw(P + 0x5b4) != 0)) {
            if (gpv != 0) {
                const float spd = fpu_mul(lwc1(0x0015ed6cu), 24.0f);
                swc1(pv + 0x20, u2f(0x3e8f5c29u));
                swc1(pv + 0x1c, 1.0f);
                sw(pv + 0x18, 0);
                swc1(gpv + 0x4, spd);
                if (lbu(m + 0x53) != 3) {
                    moby_anim_set(m, 3, 0, frame_scale_i(0xa));
                }
                (void)moby_sound_start(0, 0, m);
                sb(m + 0x20, 2);
            }
        }
        if (lw(P + 0x2084) != 0x2c || gpv == 0) return;

        const float dist = vec3_distance(V(lwu(P + 0x994) + 0x10u), V(lwu(pv + 0x14) + 0x10u));
        int32_t frames = frame_scale_i(7);
        if (lw(P + 0x10b0) < frame_scale_i(0x1e)) {
            frames = frame_scale_i(5);
        }
        swc1(gpv + 0x4, fpu_div(dist, static_cast<float>(frames)));
        swc1(pv + 0x20, u2f(0x3e8f5c29u));
        swc1(pv + 0x1c, 1.0f);
        sw(pv + 0x18, 0);
        if (lbu(m + 0x53) != 3) {
            moby_anim_set(m, 3, 0, frame_scale_i(0xa));
        }
        (void)moby_sound_start(0, 0, m);
        sb(m + 0x20, 9);
        break;
    }
    case 2: {
        sub_001f93e0(OVF(0x002c5dc8u), m);
        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }

        Vec4 delta{};
        vec3_sub(delta, V(lwu(P + 0x964) + 0x10u), V(lwu(pv + 0x14) + 0x10u));
        float step = lwc1(gpv + 0x4);
        if (lw(P + 0x208c) == 0xf) {
            step = fpu_add(step, lwc1(P + 0x574));
        } else if ((lw(P + 0x2094) == 4 || lw(P + 0x2094) == 2) && 2.0f < lwc1(P + 0x2dc)) {
            step = fpu_mul(lwc1(0x0015ed6cu), 48.0f);
        }

        Vec4 probe_start = V(HERO_POS);
        probe_start.z = fpu_add(probe_start.z, 0.5f);
        if (segment_collide_v(probe_start, V(lwu(P + 0x964) + 0x10u), 2, 0, nullptr) != 0) {
            if (0.5f < lwc1(P + 0x2dc)) {
                (void)hero_set_state(6, 1);
            } else {
                (void)hero_set_state(0, 0);
                const uint32_t anim = sub_00205000(0);
                hero_anim_set(anim, 0, static_cast<float>(frame_scale_i(0xe)));
            }
            sb(m + 0x20, 6);
            break;
        }

        if (step < vec3_distance(V(lwu(pv + 0x14) + 0x10u), V(lwu(P + 0x964) + 0x10u))) {
            vec3_set_length(delta, delta, step);
            const uint32_t gp = lwu(pv + 0x14) + 0x10u;
            vec3_add(V(gp), V(gp), delta);
        } else {
            V(lwu(pv + 0x14) + 0x10u) = V(lwu(P + 0x964) + 0x10u);
            sb(m + 0x20, 3);
        }
        break;
    }
    case 3: {
        (void)moby_sound_start(1, 0, m);
        sub_001f93e0(OVF(0x002c5dc8u), m);
        V(lwu(pv + 0x14) + 0x10u) = V(lwu(P + 0x964) + 0x10u);
        sb(m + 0x20, 4);
        break;
    }
    case 4: {
        Vec4 probe_start{};
        moby_chain_point_v(m, 0, probe_start);
        probe_start = V(HERO_POS);
        probe_start.z = fpu_add(probe_start.z, 0.5f);
        if (segment_collide_v(probe_start, V(lwu(P + 0x964) + 0x10u), 2, 0, nullptr) != 0) {
            (void)hero_set_state(lh(P + 0x30e) != 0 ? 6u : 0u, 1);
            sb(m + 0x20, 6);
            break;
        }

        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }
        sub_001f93e0(OVF(0x002c5dc8u), m);

        const float damp = fpu_add(fpu_mul(lwc1(0x0015ed60u), u2f(0xbdccccd0u)), 1.0f);
        const float wave_amp = fpu_mul(lwc1(pv + 0x20), damp);
        swc1(pv + 0x18, fpu_add(lwc1(pv + 0x18), u2f(0x3f32b8c2u)));
        swc1(pv + 0x20, wave_amp);
        if (!(wave_amp < u2f(0x3e2c0832u))) break;

        sw(P + 0x5b4, 0);
        const uint32_t g_cur = lwu(pv + 0x14);
        if (g_cur != 0 && 3.5f < vec3_distance(V(g_cur + 0x10u), V(P + 0x80))) {
            if ((lwu(0x0013cae0u) & lwu(P + 0x10a0)) != 0) {
                (void)moby_sound_start(2, 0, m);
                (void)hero_set_state(lw(P + 0x978) != 0 ? 0x26u : 0x25u, 1);
                sb(m + 0x20, 5);
            } else {
                sw(P + 0x988, 1);
                (void)hero_set_state(0, 0);
                const uint32_t anim = sub_00205000(0);
                hero_anim_set(anim, 0, static_cast<float>(frame_scale_i(0xe)));
                sb(m + 0x20, 6);
            }
        } else {
            (void)hero_set_state(0, 1);
            sb(m + 0x20, 6);
        }
        break;
    }
    case 5: {
        V(lwu(pv + 0x14) + 0x10u) = V(lwu(P + 0x964) + 0x10u);
        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }
        const float damp = fpu_add(fpu_mul(lwc1(0x0015ed60u), u2f(0xbdccccd0u)), 1.0f);
        swc1(pv + 0x18, fpu_add(lwc1(pv + 0x18), u2f(0x3f32b8c2u)));
        swc1(pv + 0x20, fpu_mul(lwc1(pv + 0x20), damp));
        sub_001f93e0(OVF(0x002c5dc8u), m);
        if (static_cast<uint32_t>(lw(P + 0x2084) - 0x25) >= 2u) {
            sb(m + 0x20, 6);
        }
        break;
    }
    case 6: {
        sub_001f93e0(OVF(0x002c5dc8u), m);
        swc1(gpv + 0x4, fpu_mul(lwc1(0x0015ed6cu), 80.0f));
        sb(m + 0x20, 7);
        break;
    }
    case 7: {
        Vec4 tip{}, delta{};
        moby_chain_point_v(m, 0, tip);
        vec3_sub(delta, tip, V(lwu(pv + 0x14) + 0x10u));
        const float dist = vec3_distance(V(lwu(pv + 0x14) + 0x10u), tip);
        const float step = lwc1(gpv + 0x4);
        if (step < dist) {
            vec3_set_length(delta, delta, step);
            const uint32_t gp = lwu(pv + 0x14) + 0x10u;
            vec3_add(V(gp), V(gp), delta);
            sub_001f93e0(OVF(0x002c5dc8u), m);
        } else {
            V(lwu(pv + 0x14) + 0x10u) = tip;
            if (lbu(m + 0x53) != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 1);
        }
        break;
    }
    case 8:
        break;
    case 9: {
        sub_001f93e0(OVF(0x002c5dc8u), m);
        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }
        Vec4 delta{};
        vec3_sub(delta, V(lwu(P + 0x994) + 0x10u), V(lwu(pv + 0x14) + 0x10u));
        const float dist = vec3_length(delta);
        const float step = lwc1(gpv + 0x4);
        if (step < dist) {
            vec3_set_length(delta, delta, step);
            const uint32_t gp = lwu(pv + 0x14) + 0x10u;
            vec3_add(V(gp), V(gp), delta);
        } else {
            V(lwu(pv + 0x14) + 0x10u) = V(lwu(P + 0x994) + 0x10u);
            sb(m + 0x20, 0xb);
            (void)moby_sound_start(1, 0, m);
        }
        break;
    }
    case 10: {
        sub_001f93e0(OVF(0x002c5dc8u), m);
        sb(m + 0x20, 0xb);
        break;
    }
    case 11: {
        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }
        sub_001f93e0(OVF(0x002c5dc8u), m);
        const float damp = fpu_add(fpu_mul(lwc1(0x0015ed60u), u2f(0xbf19999au)), 1.0f);
        const float wave_amp = fpu_mul(lwc1(pv + 0x20), damp);
        swc1(pv + 0x18, fpu_add(lwc1(pv + 0x18), u2f(0x3f32b8c2u)));
        swc1(pv + 0x20, wave_amp);
        if (wave_amp < u2f(0x3e2c0832u)) {
            sh(P + 0x99c, 1);
            sb(m + 0x20, 0xc);
        }
        break;
    }
    case 12: {
        const uint32_t target = lwu(P + 0x994);
        if (target != 0) {
            V(lwu(pv + 0x14) + 0x10u) = V(target + 0x10u);
        }
        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }
        const float damp = fpu_add(fpu_mul(lwc1(0x0015ed60u), u2f(0xbf19999au)), 1.0f);
        swc1(pv + 0x18, fpu_add(lwc1(pv + 0x18), u2f(0x3f32b8c2u)));
        swc1(pv + 0x20, fpu_mul(lwc1(pv + 0x20), damp));
        sub_001f93e0(OVF(0x002c5dc8u), m);
        if (lw(P + 0x2084) != 0x2c) {
            sb(m + 0x20, 0xd);
        }
        break;
    }
    case 13: {
        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }
        sub_001f93e0(OVF(0x002c5dc8u), m);
        swc1(gpv + 0x4, fpu_mul(lwc1(0x0015ed6cu), 80.0f));
        sb(m + 0x20, 0xe);
        break;
    }
    case 14: {
        if ((lbu(m + 0x70) & 0x2u) != 0 && lbu(m + 0x53) != 4) {
            moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        }
        Vec4 tip{}, delta{};
        moby_chain_point_v(m, 0, tip);
        vec3_sub(delta, tip, V(lwu(pv + 0x14) + 0x10u));
        const float dist = vec3_distance(V(lwu(pv + 0x14) + 0x10u), tip);
        const float step = lwc1(gpv + 0x4);
        if (step < dist) {
            vec3_set_length(delta, delta, step);
            const uint32_t gp = lwu(pv + 0x14) + 0x10u;
            vec3_add(V(gp), V(gp), delta);
            sub_001f93e0(OVF(0x002c5dc8u), m);
        } else {
            V(lwu(pv + 0x14) + 0x10u) = tip;
            if (lbu(m + 0x53) != 1) {
                moby_anim_set(m, 1, 0, frame_scale_i(0xa));
            }
            sb(m + 0x20, 1);
        }

        if (lw(P + 0x2084) != 0x2c || gpv == 0) return;
        const float d_new = vec3_distance(V(lwu(P + 0x994) + 0x10u), V(lwu(pv + 0x14) + 0x10u));
        swc1(gpv + 0x4, fpu_div(d_new, 10.0f));
        swc1(pv + 0x1c, 1.0f);
        sw(pv + 0x18, 0);
        sw(pv + 0x20, 0);
        if (lbu(m + 0x53) != 3) {
            moby_anim_set(m, 3, 0, frame_scale_i(0xa));
        }
        sb(m + 0x20, 9);
        break;
    }
    case 15:
    default:
        break;
    }
}

// orig: 0x002c5dc8
void swingshot_rope_draw_tr(uint32_t m) {
    const float half_w = u2f(0x3d4ccccdu);
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    const uint32_t desc = ee_stack_alloc(0x90);
    const uint64_t tex0 = effect_tex0(0xf);
    sd(desc + 0x70u, 5ull);
    sd(desc + 0x78u, tex0);
    sd(desc + 0x80u, 0x0000ff9000000260ull);
    sd(desc + 0x88u, (0xff00ull << 24) | 0x44ull);
    sw(desc + 0x40u, 0x80808080u);
    sw(desc + 0x44u, 0x80808080u);
    sw(desc + 0x48u, 0x80808080u);
    sw(desc + 0x4cu, 0x80808080u);
    sw(desc + 0x50u, 0);
    sw(desc + 0x54u, 0);
    swc1(desc + 0x58u, 1.0f);
    sw(desc + 0x5cu, 0);
    sw(desc + 0x60u, 0);
    swc1(desc + 0x64u, 1.0f);
    swc1(desc + 0x68u, 1.0f);
    swc1(desc + 0x6cu, 1.0f);

    Vec4 hand_pos{};
    moby_chain_point_v(m, 0, hand_pos);

    const Vec4 hook_pos = V(lwu(pv + 0x14) + 0x10u);
    Vec4 cur = hook_pos;
    Vec4 dir{};
    vec3_sub(dir, hand_pos, hook_pos);

    float traveled = 0.0f;
    const float total_len = vec3_length(dir);
    Vec4 wave_vec = dir;
    wave_vec.z = fpu_sub(wave_vec.z, u2f(0x3dcccccdu));

    Vec4 perp{}, binorm{};
    vec3_cross_ba(perp, dir, wave_vec);
    vec3_cross_ba(binorm, dir, perp);

    V(desc + 0x00u) = hook_pos;
    V(desc + 0x20u) = hook_pos;
    swc1(desc + 0x08u, fpu_sub(lwc1(desc + 0x08u), half_w));
    swc1(desc + 0x28u, fpu_add(lwc1(desc + 0x28u), half_w));

    const float phase_cos = cos_vu(angle_wrap(lwc1(pv + 0x18)));
    if (traveled < total_len) {
        float rem = fpu_sub(total_len, traveled);
        do {
            float seg = u2f(0x3e4ccccdu);
            if (rem < seg) seg = rem;
            traveled = fpu_add(traveled, seg);

            const float pi_t = fpu_mul(traveled, u2f(0x40490fdbu));
            const float env = sin_vu(fpu_div(pi_t, total_len));
            const float osc = sin_vu(angle_wrap(fpu_div(pi_t, lwc1(pv + 0x1c))));
            const float disp = fpu_mul(fpu_mul(fpu_mul(osc, env), phase_cos), lwc1(pv + 0x20));

            vec3_set_length(dir, dir, seg);
            vec3_add(cur, cur, dir);

            V(desc + 0x10u) = cur;
            V(desc + 0x30u) = cur;

            Vec4 to_cam{}, side{};
            vec3_sub(to_cam, V(OV(0x00166dc0u)), cur);
            vec3_cross_ba(side, to_cam, dir);
            vec3_set_length(side, side, u2f(0x3d4ccccdu));
            vec3_add(V(desc + 0x10u), V(desc + 0x10u), side);
            vec3_sub(V(desc + 0x30u), V(desc + 0x30u), side);

            vec3_set_length(wave_vec, perp, disp);
            vec3_add(V(desc + 0x10u), V(desc + 0x10u), wave_vec);
            vec3_add(V(desc + 0x30u), V(desc + 0x30u), wave_vec);

            quad_draw(desc, 0);

            V(desc + 0x00u) = V(desc + 0x10u);
            V(desc + 0x20u) = V(desc + 0x30u);

            if (!(traveled < total_len)) break;
            rem = fpu_sub(total_len, traveled);
        } while (true);
    }

    ee_stack_free(0x90);
}

// orig: (none)
void swingshot_fns_register() {
    ovl_register_moby_fn(0x002c6158, swingshot_update);
}

}  // namespace rac1
