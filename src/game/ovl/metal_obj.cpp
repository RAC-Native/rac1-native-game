// Ported overlay code: the Metal Detector's pulse-quad draw callback (0x2d2a90).
#include <game/ovl/metal_obj.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// orig: 0x002d2a90
void metal_2d2a90_update(uint32_t m) {
    const uint32_t desc = ee_stack_alloc(0x90);
    const uint32_t pv = lwu(m + 0x78);

    const uint64_t tex0 = effect_tex0(lw(OV(0x00161918u)));
    const uint64_t a0_fld = static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00161900u))));
    const uint64_t a1_fld = static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00161904u)))) << 2;
    const uint64_t a2_fld = static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00161908u)))) << 4;
    const uint64_t a3_fld = static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x0016190cu)))) << 6;
    const uint64_t fix_fld = static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00161910u)))) << 32;
    const uint64_t alpha_reg = a0_fld | a1_fld | a2_fld | a3_fld | fix_fld;
    const uint32_t init_col = lwu(OV(0x00161914u));

    sw(desc + 0x40u, init_col);
    sw(desc + 0x44u, init_col);
    sw(desc + 0x48u, init_col);
    sw(desc + 0x4cu, init_col);
    sw(desc + 0x50u, 0);
    sw(desc + 0x54u, 0);
    swc1(desc + 0x58u, 1.0f);
    sw(desc + 0x5cu, 0);
    sw(desc + 0x60u, 0);
    swc1(desc + 0x64u, 1.0f);
    swc1(desc + 0x68u, 1.0f);
    swc1(desc + 0x6cu, 1.0f);
    sd(desc + 0x70u, 4ull);
    sd(desc + 0x78u, tex0);
    sd(desc + 0x80u, 0x0000ff9000000260ull);
    sd(desc + 0x88u, alpha_reg);

    float phase = fpu_add(lwc1(pv + 0x24), lwc1(0x0015ed6cu));
    swc1(pv + 0x24, phase);
    if (1.0f < phase) {
        phase = fpu_sub(phase, 1.0f);
        swc1(pv + 0x24, phase);
    }
    const float cur_phase = lwc1(pv + 0x24);
    if (lwc1(pv + 0x28) < cur_phase) {
        swc1(pv + 0x28, cur_phase);
    }

    float f21 = lwc1(pv + 0x24);
    Vec4 fwd{};
    Vec4 right{1.0f, 0.0f, 0.0f, 0.0f};
    Vec4 up{};
    vec3_set_length(fwd, V(pv + 0x30), 1.0f);
    vec3_cross_ba(up, fwd, right);
    vec3_set_length(up, up, 1.0f);
    vec3_cross_ba(right, fwd, up);
    vec3_set_length(right, right, 1.0f);

    float f22 = lwc1(pv + 0x24);
    if (fpu_sub(f22, 1.0f) < f22) {
        float max_t = lwc1(pv + 0x28);
        do {
            if (f21 <= max_t) {
                const float half_size = fpu_add(fpu_mul(f21, 0.5f), u2f(0x3e6b851fu));
                Vec4 step_fwd{}, step_right{}, step_up{};
                vec3_set_length(step_fwd, fwd, f21);
                vec3_set_length(step_right, right, half_size);
                vec3_set_length(step_up, up, half_size);

                vec3_sub(V(desc + 0x00u), V(pv + 0x10), step_fwd);
                V(desc + 0x10u) = V(desc + 0x00u);
                V(desc + 0x20u) = V(desc + 0x00u);
                V(desc + 0x30u) = V(desc + 0x00u);

                vec3_sub(V(desc + 0x00u), V(desc + 0x00u), step_right);
                vec3_add(V(desc + 0x10u), V(desc + 0x10u), step_right);
                vec3_sub(V(desc + 0x20u), V(desc + 0x20u), step_right);
                vec3_add(V(desc + 0x30u), V(desc + 0x30u), step_right);

                vec3_sub(V(desc + 0x00u), V(desc + 0x00u), step_up);
                vec3_add(V(desc + 0x20u), V(desc + 0x20u), step_up);
                vec3_sub(V(desc + 0x10u), V(desc + 0x10u), step_up);
                vec3_add(V(desc + 0x30u), V(desc + 0x30u), step_up);

                const uint32_t alpha = static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(1.0f, f21), 255.0f)));
                const uint32_t col = (lwu(m + 0x90) & 0x00ffffffu) | (alpha << 24);
                sw(desc + 0x40u, col);
                sw(desc + 0x4cu, col);
                sw(desc + 0x48u, col);
                sw(desc + 0x44u, col);
                quad_draw(desc, 0);
            }

            const float step = u2f(0x3dcccccdu);
            f21 = fpu_sub(f21, step);
            if (f21 < 0.0f) {
                f21 = fpu_add(f21, 1.0f);
            }
            const float cur_p = lwc1(pv + 0x24);
            f22 = fpu_sub(f22, step);
            if (!(fpu_sub(cur_p, 1.0f) < f22)) break;
            max_t = lwc1(pv + 0x28);
        } while (true);
    }

    ee_stack_free(0x90);
}

// orig: (none)
void metal_obj_fns_register() {
    ovl_register_moby_fn(0x002d2a90, metal_2d2a90_update);
}

}  // namespace rac1
