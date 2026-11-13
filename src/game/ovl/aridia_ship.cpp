// Ported overlay code: the ship's shadow / glow, engine glow, and exhaust trail draw callbacks (Veldin's 0x28e670 /
// 0x28efc8 / 0x28f238, Aridia's copies L2:0x28dde0 / 0x28e738 / 0x28e9a8). Every level carries its copy of the three at its
// own address (the overlay map pairs them with Veldin's): one port of them, every address the engine's (OV). Veldin's own
// are still the transliterated ship_draw_cbs_tr.cpp (unit veldin_ship).
#include <game/ovl/aridia_ship.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/ship_draw_cbs.h>

namespace rac1 {

// The ship's shadow / under-glow draw callback (Veldin 0x28e670).
// (Shared: every level's copy; the screen test is Veldin's point_on_screen_depth 0x2003d8, Aridia's 0x211440.)
// orig: L2:0x0028dde0 L0:0x0028e670
void ari_ship_glow_draw_l2(uint32_t m) {
    // the original's frame: the point (the moby's position x 1/1024) +0, the quad +0x10, the screen test's depth +0xb0
    const uint32_t f = ee_stack_alloc(0xc0);
    vec4_scale(V(f), V(m), u2f(0x3a800000u));
    sw(f + 0xb0, 0);
    if (point_on_screen_depth(f, f + 0xb0, 32.0f) < 0) {  // (Aridia's 0x211440)
        ee_stack_free(0xc0);
        return;
    }

    int32_t alpha = lw(f + 0xb0);
    if (lw(OV(0x0015f5c4u)) == 6 && lw(0x0013e050u) == 3) {
        const int32_t frame = lh(0x0013e054u);
        if (frame_scale_i(0x96) < frame) {
            const int32_t thr = frame_scale_i(0x96);
            const int32_t cur_frame = lh(0x0013e054u);
            alpha = static_cast<int32_t>(static_cast<uint32_t>(alpha) - ((static_cast<uint32_t>(cur_frame) - static_cast<uint32_t>(thr)) << 2));
            if (alpha <= 0) {
                ee_stack_free(0xc0);
                return;
            }
        }
    }

    const uint32_t d = f + 0x10;
    const float z_bias = u2f(0x3dcccccdu);
    const uint64_t tex0 = effect_tex0(0);
    sd(d + 0x88, 0x8000000044ull);
    sd(d + 0x78, tex0);
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x70, 0);
    mem_copy128(d + 0x50, OV(0x001bd8b0u), 0x20);

    float z = fpu_add(lwc1(m + 0x18), z_bias);
    if (lw(OV(0x0015f5c4u)) == 6) {
        z = fpu_add(floor_probe_v(0.5f, V(m + 0x10), 0), z_bias);
    }

    const uint32_t col = (static_cast<uint32_t>(alpha >> 1) << 24) | 0x00808080u;
    for (uint32_t i = 0; i < 4; ++i) {
        const int32_t ship_kind = lh(0x0013e056u);
        const uint32_t corner = OV(0x001bd8d0u) + (static_cast<uint32_t>(ship_kind) << 6) + i * 0x10u;
        const uint32_t vtx = d + i * 0x10u;
        sw(d + 0x40u + i * 4u, col);
        mat3_transform(V(vtx), V(corner), M(m + 0xc0));
        vec3_add(V(vtx), V(vtx), V(m + 0x10));
        swc1(vtx + 0x8, z);
    }
    quad_draw(d, 0);
    ee_stack_free(0xc0);
}

// The ship's engine glow draw callback (Veldin 0x28efc8).
// (Shared: every level's copy.)
// orig: L2:0x0028e738 L0:0x0028efc8
void ari_ship_engine_glow_draw_l2(uint32_t m) {
    const int32_t kind_init = lh(0x0013e056u);
    uint32_t offsets = OV(0x001bd9b0u);
    if (kind_init == 1) {
        offsets = OV(0x001bda30u);
    } else if (kind_init == 2) {
        offsets = OV(0x001bda50u);
    }

    const uint32_t d = ee_stack_alloc(0x90);
    const uint64_t tex0 = effect_tex0(5);
    sd(d + 0x78, tex0);
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x88, 0x8000000048ull);
    sd(d + 0x70, 0);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t uv_src = OV(0x001bd990u) + k * 8u;
        swc1(d + 0x50u + k * 8u, lwc1(uv_src + 0x0));
        swc1(d + 0x54u + k * 8u, lwc1(uv_src + 0x4));
    }

    Vec4 ship_pos{};
    vec3_scale(ship_pos, V(m), u2f(0x3a800000u));

    const int32_t count0 = lw(OV(0x00160580u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2));
    if (count0 > 0) {
        int32_t idx = 0;
        do {
            const int32_t jitter = lh(m + 0xb2);
            uint32_t intensity = lbu(m + 0xbc);
            if (jitter != 0) {
                intensity += static_cast<uint32_t>(rand_mod(jitter));
            }
            const float int_f = int_to_float(static_cast<int32_t>(intensity));
            const uint32_t off_entry = offsets + (static_cast<uint32_t>(idx) << 4);
            const float size_scale = fpu_div(lwc1(off_entry + 0xc), 40.0f);
            const float radius = fpu_mul(int_f, size_scale);
            const uint32_t rgb = (lh(m + 0xa6) == 0x215) ? NOT_ADDR(0x00308000u) : NOT_ADDR(0x002058b0u);  // (colours)
            const uint32_t col = (intensity << 24) | rgb;

            for (uint32_t k = 0; k < 4; ++k) {
                const uint32_t vtx = d + k * 0x10u;
                sw(d + 0x40u + k * 4u, col);
                vec3_scale(V(vtx), V(OV(0x001bda70u) + k * 0x10u), radius);
                vec3_add(V(vtx), V(vtx), V(off_entry));
                const uint32_t ship_moby = lwu(0x0013e030u);
                mat3_transform(V(vtx), V(vtx), M(ship_moby + 0xc0));
                vec3_add(V(vtx), V(vtx), ship_pos);
            }

            ++idx;
            quad_draw(d, 0);
        } while (idx < lw(OV(0x00160580u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2)));
    }

    ee_stack_free(0x90);
}

// The ship's take-off / landing exhaust trails draw callback (Veldin 0x28f238).
// (Shared: every level's copy.)
// orig: L2:0x0028e9a8 L0:0x0028f238
void ari_ship_takeoff_draw_l2(uint32_t /*m*/) {
    const int32_t tex_id = (lw(OV(0x0015f5c4u)) == 6 && lw(0x0013e050u) == 4) ? 0 : 0x13;
    const uint32_t d = ee_stack_alloc(0x90);
    sd(d + 0x78, effect_tex0(tex_id));
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x88, 0x8000000048ull);
    sd(d + 0x70, 0);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t uv_src = OV(0x001bdb10u) + k * 8u;
        swc1(d + 0x50u + k * 8u, lwc1(uv_src + 0x0));
        swc1(d + 0x54u + k * 8u, lwc1(uv_src + 0x4));
    }

    if (lw(0x0013e084u) - 1 > 0) {
        int32_t seg = 0;
        do {
            const uint32_t head = lwu(0x0013e080u);
            const uint32_t i0 = (head - static_cast<uint32_t>(seg) + 0x1fu) & 0x1fu;
            const uint32_t i1 = (i0 + 1u) & 0x1fu;
            const uint32_t i2 = (i0 + 2u) & 0x1fu;

            constexpr uint32_t ring_a = 0x0013e0f0u;
            constexpr uint32_t ring_b = 0x0013e2f0u;
            const uint32_t p_b0 = ring_b + (i0 << 4);
            const uint32_t p_a0 = ring_a + (i0 << 4);
            const uint32_t p_b1 = ring_b + (i1 << 4);
            const uint32_t p_a1 = ring_a + (i1 << 4);
            const uint32_t p_a2 = ring_a + (i2 << 4);

            for (int32_t side = 0; side < 2; ++side) {
                const uint32_t side_off = static_cast<uint32_t>(side) << 9;
                Vec4 edges[4]{};
                Vec4 norms[4]{};

                vec3_sub(edges[0], V(p_b0), V(p_a0));
                vec3_sub(edges[1], V(p_a0), V(p_b0));
                vec3_sub(edges[2], V(p_b1), V(p_a1));
                vec3_sub(edges[3], V(p_a1), V(p_b1));

                const uint32_t cur1 = side_off + p_a1;
                vec3_sub(norms[1], V(cur1), V(side_off + p_a0));
                vec3_sub(norms[3], V(side_off + p_a2), V(cur1));
                if (seg == 0) {
                    norms[3] = norms[1];
                }

                vec3_cross_ba(norms[0], edges[0], norms[1]);
                vec3_cross_ba(norms[1], edges[1], norms[1]);
                vec3_cross_ba(norms[2], edges[2], norms[3]);
                vec3_cross_ba(norms[3], edges[3], norms[3]);

                for (int32_t k = 0; k < 4; ++k) {
                    const int32_t step = k >> 1;
                    const float t = fpu_mul(int_to_float(seg - (step - 1)), 0.03125f);
                    const uint32_t kind_col_off = static_cast<uint32_t>(lh(0x0013e056u)) << 3;
                    const uint32_t col0 = lwu(OV(0x001bdb30u) + kind_col_off);
                    const uint32_t col1 = lwu(OV(0x001bdb34u) + kind_col_off);
                    sw(d + 0x40u + static_cast<uint32_t>(k) * 4u, rgba_lerp(col0, col1, t));

                    const float falloff = fpu_sub(1.0f, fpu_mul(t, t));
                    const float base_w = lwc1(OV(0x00160590u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2));
                    const uint32_t vtx = d + static_cast<uint32_t>(k) * 0x10u;
                    vec3_set_length(V(vtx), norms[k], fpu_mul(falloff, base_w));

                    const uint32_t pt_idx = (i0 + static_cast<uint32_t>(step)) & 0x1fu;
                    vec3_add(V(vtx), V(vtx), V(side_off + ring_a + (pt_idx << 4)));
                }
                quad_draw(d, 0);

                for (int32_t k = 0; k < 4; ++k) {
                    const int32_t step = k >> 1;
                    const float t = fpu_mul(int_to_float(seg - (step - 1)), 0.03125f);
                    const float falloff = fpu_sub(1.0f, fpu_mul(t, t));
                    const float base_w = lwc1(OV(0x00160590u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2));
                    const uint32_t vtx = d + static_cast<uint32_t>(k) * 0x10u;
                    vec3_set_length(V(vtx), edges[k], fpu_mul(falloff, base_w));

                    const uint32_t pt_idx = (i0 + static_cast<uint32_t>(step)) & 0x1fu;
                    vec3_add(V(vtx), V(vtx), V(side_off + ring_a + (pt_idx << 4)));
                }
                quad_draw(d, 0);
            }

            ++seg;
        } while (seg < lw(0x0013e084u) - 1);
    }

    ee_stack_free(0x90);
}

}  // namespace rac1
