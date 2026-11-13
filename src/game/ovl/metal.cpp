// Ported overlay code: the Metal Detector (item 0x1b, hand class 585, update 0x2d2ee8).
#include <game/ovl/metal.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t PAD_BASE = 0x0013c940u;
constexpr uint32_t SND_CHANNELS = 0x0013e550u;

void stop_metal_sound(uint32_t m, uint32_t slot_addr) {
    const int32_t slot = lw(slot_addr);
    if (slot != -1) {
        const uint32_t ch = SND_CHANNELS + static_cast<uint32_t>(slot) * 0x70u;
        if (lwu(ch + 0x88) == m && lbu(ch + 0x74) != 0) {
            sub_0028d918(slot);
        }
    }
    sw(slot_addr, 0xffffffffu);
}
}  // namespace

// orig: 0x0028df38
void metal_28df38(uint32_t p0, uint32_t p1) {
    sw(SND_CHANNELS + p0 * 0x70u + 0x84u, p1);
}

// orig: 0x0025f8e0
void metal_25f8e0(uint32_t p0, float pf12) {
    const float lo = -pf12;
    const float rx = rand_range_f(lo, pf12);
    swc1(p0 + 0x0, fpu_add(lwc1(p0 + 0x0), rx));
    const float ry = rand_range_f(lo, pf12);
    swc1(p0 + 0x4, fpu_add(lwc1(p0 + 0x4), ry));
    const float rz = rand_range_f(lo, pf12);
    swc1(p0 + 0x8, fpu_add(lwc1(p0 + 0x8), rz));
}

// orig: 0x002d2e28
void metal_2d2e28(uint32_t p0) {
    const int32_t level = lw(0x0015ed84u);
    if (level >= 20) return;

    const int32_t idx = static_cast<int32_t>(lbu(p0 + 0xbc)) - 1;
    const int32_t byte_off = (idx + static_cast<int32_t>(static_cast<uint32_t>(idx) >> 31)) >> 1;
    const uint32_t addr = 0x0014bf10u + (static_cast<uint32_t>(level) << 4) + static_cast<uint32_t>(byte_off);
    const uint32_t b = lbu(addr);
    if ((idx & 1) != 0) {
        uint32_t nib = (b & 0xfu) + 1u;
        if (nib >= 16u) nib = 0xfu;
        sb(addr, (b & 0xf0u) | nib);
    } else {
        uint32_t nib = (b >> 4) + 1u;
        if (nib >= 16u) nib = 0xfu;
        sb(addr, (b & 0xfu) | (nib << 4));
    }
}

// orig: 0x002d2ee8
void metal_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;

    moby_chain_point_v(m, 0, V(pv + 0x10));

    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        swc1(P + 0x2044, u2f(0x47c35000u));
        sw(P + 0x2040, 0);
        sw(pv + 0xc, 0xffffffffu);
        sw(pv + 0x8, 0x7fffu);
        sw(pv + 0x0, 0xffffffffu);
        sw(pv + 0x4, 0xffffffffu);
        const uint32_t init_b = lbu(0x0013cac9u);
        sw(pv + 0x28, 0);
        sb(pv + 0x20, init_b);
        sw(pv + 0x24, 0);
        vec_clear(V(OV(0x001e1160u)));
        sub_0024f590(m, 3, OV(0x001e1160u));
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1 && st != 2) return;

    Vec4 dir{0.0f, 0.0f, 1.0f, 0.0f};
    Vec4 q{};
    const uint32_t target = lwu(P + 0x2040);
    if (target != 0 && lh(target + 0xa6) == 0x25d &&
        lbu(target + 0x20) != 0xfe && lbu(target + 0x20) != 0xfd) {
        const float d2 = vec2_distance(V(pv + 0x10), V(target + 0x10));
        const float neg_half_pi = u2f(0xbfc90fdbu);
        const uint32_t t1 = lwu(P + 0x2040);
        float pitch_diff = atan2_f(d2, fpu_sub(lwc1(t1 + 0x18), lwc1(pv + 0x18)));
        const uint32_t t2 = lwu(P + 0x2040);
        const float yaw_to = atan2_f(fpu_sub(lwc1(t2 + 0x10), lwc1(pv + 0x10)),
                                     fpu_sub(lwc1(t2 + 0x14), lwc1(pv + 0x14)));
        float yaw_diff = angle_sub(yaw_to, lwc1(P + 0x98));
        pitch_diff = angle_sub(pitch_diff, neg_half_pi);
        const float abs_yaw = abs_f(yaw_diff);
        const float abs_pitch = abs_f(pitch_diff);

        const float deg2rad = u2f(0x3c8efa35u);
        const float max_yaw = fpu_mul(lwc1(OV(0x00161950u)), deg2rad);
        if (max_yaw < abs_yaw) {
            yaw_diff = fpu_mul(fpu_div(yaw_diff, abs_yaw), max_yaw);
        }
        const float max_pitch = fpu_mul(lwc1(OV(0x00161954u)), deg2rad);
        if (max_pitch < abs_pitch) {
            pitch_diff = fpu_mul(fpu_div(pitch_diff, abs_pitch), max_pitch);
        }

        const float yaw = angle_add(yaw_diff, lwc1(P + 0x98));
        const float pitch = angle_add(pitch_diff, neg_half_pi);
        vec_from_spherical_v(dir, -3.0f, yaw, pitch);
        debug_stub_1f6c28();

        Mat4 frame{};
        {
            const uint32_t i0 = lwu(OV(0x00161920u));
            const float s0_len = static_cast<float>(lw(OV(0x0016192cu)));
            vec3_set_length(frame.r[i0], dir, s0_len);
        }
        {
            const uint32_t i1 = lwu(OV(0x00161924u));
            const uint32_t i0 = lwu(OV(0x00161920u));
            vec3_cross_ba(frame.r[i1], frame.r[i0], V(OV(0x00161940u)));
        }
        {
            const uint32_t i1 = lwu(OV(0x00161924u));
            const float s1_len = static_cast<float>(lw(OV(0x00161930u)));
            vec3_set_length(frame.r[i1], frame.r[i1], s1_len);
        }
        {
            const uint32_t i2 = lwu(OV(0x0016191cu));
            const uint32_t i1 = lwu(OV(0x00161924u));
            const uint32_t i0 = lwu(OV(0x00161920u));
            vec3_cross_ba(frame.r[i2], frame.r[i1], frame.r[i0]);
        }
        frame.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};

        Mat4 chain2{};
        moby_chain_world_one_v(m, 2, chain2);
        vec3_set_length(chain2.r[0], chain2.r[0], 1.0f);
        vec3_set_length(chain2.r[1], chain2.r[1], 1.0f);
        vec3_set_length(chain2.r[2], chain2.r[2], 1.0f);

        Mat4 inv_chain2{};
        mat3_transpose(inv_chain2, chain2);
        inv_chain2.r[3] = Vec4{0.0f, 0.0f, 0.0f, 1.0f};

        Mat4 prod{};
        mat4_mul(prod, inv_chain2, frame);
        mat_to_quat(q, prod);
    } else {
        const Vec4 zero_ang{0.0f, 0.0f, 0.0f, 0.0f};
        quat_from_euler_v(q, zero_ang);
    }

    V(pv + 0x30) = dir;
    quat_nlerp(V(OV(0x001e1170u)), V(OV(0x001e1170u)), q, u2f(0x3dcccccdu));

    Mat4 chain3{};
    moby_chain_world_one_v(m, 3, chain3);
    V(pv + 0x30) = chain3.r[0];

    if ((lwu(PAD_BASE + 0x1a4) & 0x20u) != 0) {
        sw(pv + 0x24, 0);
        sw(pv + 0x28, 0);
    }

    if ((lwu(PAD_BASE + 0x1a0) & 0x20u) == 0 || !(frame_scale_i(0xf) < lw(P + 0x10b0))) {
        sw(m + 0x90, 0x80808080u);
        stop_metal_sound(m, pv + 0x0);
        stop_metal_sound(m, pv + 0x4);
        stop_metal_sound(m, pv + 0xc);
        return;
    }

    if (!(lwc1(P + 0x2044) < 10.0f)) {
        sw(m + 0x90, 0x80808080u);
        stop_metal_sound(m, pv + 0x0);
        stop_metal_sound(m, pv + 0x4);
        stop_metal_sound(m, pv + 0xc);
        sub_001f93e0(OVF(0x002d2a90u), m);
        return;
    }

    if (sub_0028d8c0(m, lw(pv + 0xc)) == 0 && lw(pv + 0xc) == -1) {
        sw(pv + 0xc, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
    }
    if (lw(pv + 0xc) != -1) {
        const float d_twice = fpu_add(lwc1(P + 0x2044), lwc1(P + 0x2044));
        const float pitch_f = fpu_div(fpu_mul(fpu_sub(10.0f, d_twice), u2f(0x46fffe00u)), 60.0f);
        const uint32_t pitch_u = static_cast<uint32_t>(float_to_int(pitch_f)) - 0x1fffu;
        metal_28df38(lwu(pv + 0xc), pitch_u);
    }

    const uint32_t c = static_cast<uint32_t>(float_to_int(fpu_div(fpu_mul(lwc1(P + 0x2044), 255.0f), 10.0f)));
    sw(m + 0x90, 0xff0000ffu | (c << 8) | (c << 16));

    const uint32_t tgt = lwu(P + 0x2040);
    if (tgt != 0 && lh(tgt + 0xa6) == 0x25d &&
        lbu(tgt + 0x20) != 0xfe && lbu(tgt + 0x20) != 0xfd &&
        lwc1(P + 0x2044) < 1.0f) {
        const uint32_t tpv = lwu(tgt + 0x78);
        if (lw(tpv + 0x18) > 0) {
            Vec4 bvel = chain3.r[0];
            do {
                bvel.x = fpu_mul(rand_range_f(1.0f, 10.0f), lwc1(0x0015ed6cu));
                bvel.y = fpu_mul(rand_range_f(-10.0f, 10.0f), lwc1(0x0015ed6cu));
                bvel.z = fpu_mul(rand_range_f(1.0f, 12.0f), lwc1(0x0015ed6cu));
                mat3_transform(bvel, bvel, M(P));

                const uint32_t cur_tgt = lwu(P + 0x2040);
                V(cur_tgt + 0x10) = V(pv + 0x10);
                metal_25f8e0(cur_tgt + 0x10u, u2f(0x3e4ccccdu));
                {
                    const uint32_t bt = lwu(P + 0x2040);
                    (void)bolt_spawn_v(bt, V(bt + 0x10), bvel, 0x13, 5, 0);
                }

                uint32_t mt = lwu(P + 0x2040);
                V(mt + 0x10) = V(pv + 0x10);
                if (tpv != 0) {
                    const int32_t rem = lw(tpv + 0x18);
                    if (rem > 0) {
                        sw(tpv + 0x18, static_cast<uint32_t>(rem - 5));
                        mt = lwu(P + 0x2040);
                    }
                }
                metal_2d2e28(mt);
            } while (lw(tpv + 0x18) > 0);
        }

        moby_retire(lwu(P + 0x2040));
        sw(P + 0x2040, 0);
        swc1(P + 0x2044, u2f(0x47c35000u));
        sw(pv + 0x8, 0x7fffu);
        (void)moby_sound_start(3, 0, m);
        stop_metal_sound(m, pv + 0xc);
    }

    sub_001f93e0(OVF(0x002d2a90u), m);
}

// orig: (none)
void metal_fns_register() {
    ovl_register_moby_fn(0x002d2ee8, metal_update);
}

}  // namespace rac1
