// Ported overlay code: Aridia's ship shadow/glow, engine glow, and exhaust trail draw callbacks
// (L2:0x28dde0 / 0x28e738 / 0x28e9a8, Aridia's copies of Veldin's 0x28e670 / 0x28efc8 / 0x28f238) and
// their screen-space visibility / depth-alpha helpers (L2:0x211348 / 0x211440).
#include <game/ovl/aridia_ship.h>

#include <algorithm>
#include <cmath>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/vu0_macro.h>

namespace rac1 {

namespace {
// Shared VU0 frustum + near-depth helper for L2:0x211348 / L2:0x211440.
int64_t ari_point_on_screen_v_l2(const Vec4& pt, float r, float* vf3y_out) {
    const uint32_t base = L2_ADDR(0x0016d2c0u);
    const Vec4 r0 = V(base + 0x00u), r1 = V(base + 0x10u), r2 = V(base + 0x20u);
    const Vec4 cam = V(base + 0x30u);
    const Vec4 a = V(base + 0x40u), b = V(base + 0x50u), c = V(base + 0x60u), d = V(base + 0x70u);

    const Vec4 p{vu0m::mul(pt.x, cam.w), vu0m::mul(pt.y, cam.w), vu0m::mul(pt.z, cam.w), vu0m::mul(pt.w, cam.w)};
    const float dx = vu0m::sub(p.x, cam.x), dy = vu0m::sub(p.y, cam.y), dz = vu0m::sub(p.z, cam.z);
    const float near_x = vu0m::add(0.0f, p.w), near_y = vu0m::sub(0.0f, p.w);
    const float vx = vu0m::madd(vu0m::madd(vu0m::mul(r0.x, dx), r1.x, dy), r2.x, dz);
    const float vy = vu0m::madd(vu0m::madd(vu0m::mul(r0.y, dx), r1.y, dy), r2.y, dz);
    const float vz = vu0m::madd(vu0m::madd(vu0m::mul(r0.z, dx), r1.z, dy), r2.z, dz);
    const float rr = vu0m::mul(r, cam.w);
    const float e8x = vu0m::mul(c.x, p.w), e8y = vu0m::mul(c.y, p.w);
    const float s1x = vu0m::add(near_x, vz), s1y = vu0m::add(near_y, vz);
    const float e5x = vu0m::mul(a.x, vz), e5y = vu0m::mul(a.y, vz);
    const float ax = u2f(f2u(vx) & 0x7fffffffu), ay = u2f(f2u(vy) & 0x7fffffffu);
    const float e7x = vu0m::mul(e8x, c.z), e7y = vu0m::mul(e8y, c.z);
    const float t3x = vu0m::sub(0.0f, s1x), t3y = vu0m::sub(rr, s1y);
    const float e9x = vu0m::mul(e5x, b.x), e9y = vu0m::mul(e5y, b.y);
    const float e6x = vu0m::sub(ax, e8x), e6y = vu0m::sub(ay, e8y);
    const float f8x = vu0m::add(ax, e7x), f8y = vu0m::add(ay, e7y);
    const float f4x = vu0m::sub(s1x, d.x), f4y = vu0m::sub(s1y, d.y);
    const float f7x = vu0m::sub(e5x, e6x), f7y = vu0m::sub(e5y, e6y);
    const float g8x = vu0m::sub(e9x, f8x), g8y = vu0m::sub(e9y, f8y);

    if (vf3y_out != nullptr) *vf3y_out = t3y;
    if (std::signbit(t3y) || !std::signbit(t3x)) return -1;
    if (std::signbit(f7y) || std::signbit(f7x)) return -1;
    if (std::signbit(g8y) || std::signbit(f4y)) return 0;
    if (std::signbit(g8x) || !std::signbit(f4x)) return 0;
    return 1;
}

int32_t ari_screen_alpha_v_l2(const Vec4& pt, uint32_t& out_alpha, float r) {
    float vf3y = 0.0f;
    const int64_t v = ari_point_on_screen_v_l2(pt, r, &vf3y);
    if (v < 0) {
        out_alpha = 0;
        return static_cast<int32_t>(v);
    }
    const uint32_t y = vu0m::ftoi(vf3y, 1.0f);
    const int32_t w = static_cast<int32_t>(y >> 7);
    out_alpha = static_cast<uint32_t>(std::min(std::max(w, 0), 0x80));
    return static_cast<int32_t>(v);
}
}  // namespace

// Veldin's 0x2002e0 (point_on_screen) on Aridia.
// orig: L2:0x00211348
int64_t ari_point_on_screen_l2(uint32_t pt, float r) {
    return ari_point_on_screen_v_l2(V(pt), r, nullptr);
}

// Veldin's 0x2003d8 (point_on_screen_depth) on Aridia.
// orig: L2:0x00211440
int32_t ari_screen_alpha_l2(uint32_t pt, uint32_t out, float r) {
    uint32_t alpha = 0;
    const int32_t res = ari_screen_alpha_v_l2(V(pt), alpha, r);
    sw(out, alpha);
    return res;
}

// The ship's shadow / under-glow draw callback (Veldin 0x28e670).
// orig: L2:0x0028dde0
void ari_ship_glow_draw_l2(uint32_t m) {
    Vec4 pt_scaled{};
    vec4_scale(pt_scaled, V(m), u2f(0x3a800000u));
    uint32_t alpha_u = 0;
    if (ari_screen_alpha_v_l2(pt_scaled, alpha_u, 32.0f) < 0) return;

    int32_t alpha = static_cast<int32_t>(alpha_u);
    if (lw(L2_ADDR(0x0015f5c4u)) == 6 && lw(0x0013e050u) == 3) {
        const int32_t frame = lh(0x0013e054u);
        if (frame_scale_i(0x96) < frame) {
            const int32_t thr = frame_scale_i(0x96);
            const int32_t cur_frame = lh(0x0013e054u);
            alpha = static_cast<int32_t>(static_cast<uint32_t>(alpha) - ((static_cast<uint32_t>(cur_frame) - static_cast<uint32_t>(thr)) << 2));
            if (alpha <= 0) return;
        }
    }

    const uint32_t d = ee_stack_alloc(0x90);
    const float z_bias = u2f(0x3dcccccdu);
    const uint64_t tex0 = effect_tex0(0);
    sd(d + 0x88, 0x8000000044ull);
    sd(d + 0x78, tex0);
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x70, 0);
    mem_copy128(d + 0x50, L2_ADDR(0x001bdeb0u), 0x20);

    float z = fpu_add(lwc1(m + 0x18), z_bias);
    if (lw(L2_ADDR(0x0015f5c4u)) == 6) {
        z = fpu_add(floor_probe_v(0.5f, V(m + 0x10), 0), z_bias);
    }

    const uint32_t col = (static_cast<uint32_t>(alpha >> 1) << 24) | 0x00808080u;
    for (uint32_t i = 0; i < 4; ++i) {
        const int32_t ship_kind = lh(0x0013e056u);
        const uint32_t corner = L2_ADDR(0x001bded0u) + (static_cast<uint32_t>(ship_kind) << 6) + i * 0x10u;
        const uint32_t vtx = d + i * 0x10u;
        sw(d + 0x40u + i * 4u, col);
        mat3_transform(V(vtx), V(corner), M(m + 0xc0));
        vec3_add(V(vtx), V(vtx), V(m + 0x10));
        swc1(vtx + 0x8, z);
    }
    quad_draw(d, 0);
    ee_stack_free(0x90);
}

// The ship's engine glow draw callback (Veldin 0x28efc8).
// orig: L2:0x0028e738
void ari_ship_engine_glow_draw_l2(uint32_t m) {
    const int32_t kind_init = lh(0x0013e056u);
    uint32_t offsets = L2_ADDR(0x001bdfb0u);
    if (kind_init == 1) {
        offsets = L2_ADDR(0x001be030u);
    } else if (kind_init == 2) {
        offsets = L2_ADDR(0x001be050u);
    }

    const uint32_t d = ee_stack_alloc(0x90);
    const uint64_t tex0 = effect_tex0(5);
    sd(d + 0x78, tex0);
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x88, 0x8000000048ull);
    sd(d + 0x70, 0);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t uv_src = L2_ADDR(0x001bdf90u) + k * 8u;
        swc1(d + 0x50u + k * 8u, lwc1(uv_src + 0x0));
        swc1(d + 0x54u + k * 8u, lwc1(uv_src + 0x4));
    }

    Vec4 ship_pos{};
    vec3_scale(ship_pos, V(m), u2f(0x3a800000u));

    const int32_t count0 = lw(L2_ADDR(0x00160580u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2));
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
            const uint32_t rgb = (lh(m + 0xa6) == 0x215) ? 0x00308000u : 0x002058b0u;
            const uint32_t col = (intensity << 24) | rgb;

            for (uint32_t k = 0; k < 4; ++k) {
                const uint32_t vtx = d + k * 0x10u;
                sw(d + 0x40u + k * 4u, col);
                vec3_scale(V(vtx), V(L2_ADDR(0x001be070u) + k * 0x10u), radius);
                vec3_add(V(vtx), V(vtx), V(off_entry));
                const uint32_t ship_moby = lwu(0x0013e030u);
                mat3_transform(V(vtx), V(vtx), M(ship_moby + 0xc0));
                vec3_add(V(vtx), V(vtx), ship_pos);
            }

            ++idx;
            quad_draw(d, 0);
        } while (idx < lw(L2_ADDR(0x00160580u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2)));
    }

    ee_stack_free(0x90);
}

// The ship's take-off / landing exhaust trails draw callback (Veldin 0x28f238).
// orig: L2:0x0028e9a8
void ari_ship_takeoff_draw_l2(uint32_t /*m*/) {
    const int32_t tex_id = (lw(L2_ADDR(0x0015f5c4u)) == 6 && lw(0x0013e050u) == 4) ? 0 : 0x13;
    const uint32_t d = ee_stack_alloc(0x90);
    sd(d + 0x78, effect_tex0(tex_id));
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x88, 0x8000000048ull);
    sd(d + 0x70, 0);
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t uv_src = L2_ADDR(0x001be110u) + k * 8u;
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
                    const uint32_t col0 = lwu(L2_ADDR(0x001be130u) + kind_col_off);
                    const uint32_t col1 = lwu(L2_ADDR(0x001be134u) + kind_col_off);
                    sw(d + 0x40u + static_cast<uint32_t>(k) * 4u, rgba_lerp(col0, col1, t));

                    const float falloff = fpu_sub(1.0f, fpu_mul(t, t));
                    const float base_w = lwc1(L2_ADDR(0x00160590u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2));
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
                    const float base_w = lwc1(L2_ADDR(0x00160590u) + (static_cast<uint32_t>(lh(0x0013e056u)) << 2));
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
