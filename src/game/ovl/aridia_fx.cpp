// Ported overlay code: Aridia's Elevator shaft glow draw callback (class 651, L2:0x2dc2c8) and Vendor Orb
// update (class 1143, L2:0x2ebcb8).
#include <game/ovl/aridia_fx.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants_l2.h>

namespace rac1 {

// The Elevator's draw callback: draws its shaft's glow slices from its box (pvar +0xa0, boxes *(0x1600fc)).
// orig: L2:0x002dc2c8
void ari_elevator_draw_l2(uint32_t m) {
    const uint32_t d = ee_stack_alloc(0xd0);
    const uint32_t mat = d + 0x90u;

    const uint32_t boxes = lwu(L2_ADDR(0x001600fcu));
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t box = boxes + (lwu(pv + 0xa0) << 7);
    const float z_off = lwc1(pv + 0xb8);
    const float height_ang = lwc1(pv + 0xb0);
    const float half_h = lwc1(box + 0x28);
    const float center_z = lwc1(box + 0x38);
    const float z_top = fpu_add(fpu_add(center_z, half_h), z_off);
    const float z_bot = fpu_add(fpu_sub(center_z, half_h), z_off);

    const float col_t = fpu_div(abs_f(height_ang), fpu_mul(lwc1(L2_ADDR(0x00161bdcu)), lwc1(0x0015ed6cu)));
    const uint32_t col = rgba_lerp(lwu(L2_ADDR(0x00161c04u)), lwu(L2_ADDR(0x00161c08u)), col_t);

    mat3_copy_affine(M(mat), M(m + 0xc0));
    lq_sq(mat + 0x30, m + 0x10);
    swc1(mat + 0x38, z_bot);
    swc1(mat + 0x3c, 1.0f);

    const uint64_t tex0 = effect_tex0(0xe);
    const uint64_t alpha_reg = static_cast<uint64_t>(static_cast<int64_t>(lw(L2_ADDR(0x00161bf4u))))
        | (static_cast<uint64_t>(static_cast<int64_t>(lw(L2_ADDR(0x00161bf8u)))) << 2)
        | (static_cast<uint64_t>(static_cast<int64_t>(lw(L2_ADDR(0x00161bfcu)))) << 4)
        | (static_cast<uint64_t>(static_cast<int64_t>(lw(L2_ADDR(0x00161c00u)))) << 6)
        | 0x8000000000ull;
    sd(d + 0x70, 0);
    sd(d + 0x78, tex0);
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x88, alpha_reg);

    const float deg2rad = u2f(0x3c8efa35u);
    const float rad_inset = u2f(0x3d4ccccdu);
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t uv_src = L2_ADDR(0x001d3f50u) + static_cast<uint32_t>(i) * 8u;
        const uint32_t uv_dst = d + 0x50u + static_cast<uint32_t>(i) * 8u;
        swc1(uv_dst + 0x0, lwc1(uv_src + 0x0));
        swc1(uv_dst + 0x4, lwc1(uv_src + 0x4));

        float vz;
        float rad;
        if ((i & 1) != 0) {
            vz = lwc1(L2_ADDR(0x00161bf0u));
            rad = lwc1(L2_ADDR(0x00161becu));
        } else {
            vz = -lwc1(L2_ADDR(0x00161bf0u));
            rad = fpu_sub(lwc1(L2_ADDR(0x00161becu)), rad_inset);
        }

        const uint32_t vtx = d + static_cast<uint32_t>(i) * 0x10u;
        swc1(vtx + 0x8, vz);
        const float k = static_cast<float>(i >> 1);
        const float ang_c = fpu_mul(k, fpu_mul(lwc1(L2_ADDR(0x00161c10u)), deg2rad));
        swc1(vtx + 0x0, fpu_mul(cos_vu(ang_c), rad));
        const float ang_s = fpu_mul(k, fpu_mul(lwc1(L2_ADDR(0x00161c10u)), deg2rad));
        swc1(vtx + 0xc, 1.0f);
        swc1(vtx + 0x4, fpu_mul(sin_vu(ang_s), rad));
    }

    if (lwc1(mat + 0x38) < z_top) {
        int32_t sign = 1;
        do {
            const int32_t frame_mod = ee_rem(lw(L2_ADDR(0x0015f5ccu)), 0x78);
            const float step_deg = lwc1(L2_ADDR(0x00161c10u));
            const float count = fpu_div(360.0f, step_deg);
            sign = static_cast<int32_t>(0u - static_cast<uint32_t>(sign));
            const float spin = fpu_mul(static_cast<float>(frame_mod), u2f(0x3cd67744u));

            if (0.0f < count) {
                const float phase = fpu_mul(spin, static_cast<float>(sign));
                const float half_pi = u2f(0x3fc90fdbu);
                int32_t idx = 0;
                float idx_f = 0.0f;
                float step_rad = fpu_mul(step_deg, deg2rad);
                do {
                    ++idx;
                    const float a0 = angle_wrap(fpu_add(fpu_mul(step_rad, idx_f), phase));
                    swc1(mat + 0x00, cos_vu(a0));

                    const float a1 = angle_wrap(fpu_add(fpu_mul(fpu_mul(lwc1(L2_ADDR(0x00161c10u)), deg2rad), idx_f), phase));
                    swc1(mat + 0x04, sin_vu(a1));
                    sw(mat + 0x08, 0);

                    const float a2 = angle_wrap(fpu_add(fpu_add(fpu_mul(fpu_mul(lwc1(L2_ADDR(0x00161c10u)), deg2rad), idx_f), half_pi), phase));
                    swc1(mat + 0x10, cos_vu(a2));

                    const float prev_idx_f = idx_f;
                    idx_f = static_cast<float>(idx);
                    const float a3 = angle_wrap(fpu_add(fpu_add(fpu_mul(fpu_mul(lwc1(L2_ADDR(0x00161c10u)), deg2rad), prev_idx_f), half_pi), phase));
                    swc1(mat + 0x14, sin_vu(a3));
                    sw(mat + 0x18, 0);
                    swc1(mat + 0x28, 1.0f);

                    sw(d + 0x4c, col);
                    sw(d + 0x48, col);
                    sw(d + 0x44, col);
                    sw(d + 0x40, col);
                    quad_draw(d, mat);

                    const float cur_step_deg = lwc1(L2_ADDR(0x00161c10u));
                    if (!(idx_f < fpu_div(360.0f, cur_step_deg))) break;
                    step_rad = fpu_mul(cur_step_deg, deg2rad);
                } while (true);
            }

            const float next_z = fpu_add(lwc1(mat + 0x38), lwc1(L2_ADDR(0x00161be8u)));
            swc1(mat + 0x38, next_z);
            if (!(next_z < z_top)) break;
        } while (true);
    }

    ee_stack_free(0xd0);
}

// Class 1143 (the Vendor Orb)'s update on Aridia.
// orig: L2:0x002ebcb8
void ari_vendor_orb_update_l2(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x73, 0x20);
        swc1(pv + 0x40, rand_angle());
        sub_0024f590(m, 1, pv);
        return;
    }
    if (st != 1) return;

    const float delta = fpu_mul(fpu_mul(lwc1(L2_ADDR(0x00161f48u)), u2f(0x3c8efa35u)), lwc1(0x0015ed6cu));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), delta));

    const float orb_ang = angle_add(lwc1(pv + 0x40), fpu_mul(delta, -2.0f));
    swc1(pv + 0x40, orb_ang);
    quat_from_axis_angle(V(pv + 0x10), 2, orb_ang);
}

}  // namespace rac1
