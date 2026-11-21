// Ported overlay code: Decoy Glove landing ring draw callback (0x002c31c0).
#include <game/ovl/decoy_fx.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

// orig: 0x002c31c0
void decoy_2c31c0_update(uint32_t m) {
    const int32_t state = lw(OV(0x0015f5c4u));
    const uint32_t pv = lwu(m + 0x78);
    if (state == 2) return;

    const uint32_t frame = ee_stack_alloc(0xd0);
    const uint32_t desc = frame;
    const uint32_t mat_addr = frame + 0x90;

    constexpr uint32_t COLOR = 0x80808080u;
    for (int32_t i = 0; i < 2; ++i) {
        const int32_t odd = i & 1;
        const float nx = lwc1(pv + 0x30);
        const float nz = lwc1(pv + 0x38);
        const float h = sqrt_f(fpu_add(fpu_mul(nx, nx), fpu_mul(nz, nz)));
        const float pitch = angle_add(-atan2_f(h, lwc1(pv + 0x34)), u2f(0x3fc90fdbu));
        const float yaw = atan2_f(lwc1(pv + 0x38), lwc1(pv + 0x30));
        const Vec4 rot1{pitch, yaw, 0.0f, 0.0f};

        const int32_t period = frame_scale_i(0xb4);
        const int32_t rem = ee_rem(lw(OV(0x0015f5ccu)), period);
        const float rem_f = int_to_float(rem);
        const float denom = frame_scale_f(180.0f);
        float spin = angle_wrap(fpu_div(fpu_mul(rem_f, u2f(0x40c90fdbu)), denom));
        if (odd != 0) spin = -spin;
        const Vec4 rot2{0.0f, spin, 0.0f, 0.0f};

        Mat4 spin_mat{};
        euler_to_mat4_vu(spin_mat, rot2);
        euler_to_mat4_vu(M(mat_addr), rot1);
        mat4_mul(M(mat_addr), M(mat_addr), spin_mat);
        V(mat_addr + 0x30) = V(pv + 0x20);

        const int32_t tex_id = (odd == 0) ? 0x11 : 0x12;
        sw(desc + 0x40, COLOR);
        sw(desc + 0x44, COLOR);
        sw(desc + 0x48, COLOR);
        sw(desc + 0x4c, COLOR);

        const uint64_t tex0 = effect_tex0(tex_id);
        sd(desc + 0x88, (uint64_t{0x8000} << 24) | 0x48u);
        sd(desc + 0x78, tex0);
        sd(desc + 0x80, (uint64_t{0xff90} << 32) | 0x260u);
        sd(desc + 0x70, 0);

        V(desc + 0x00) = Vec4{-1.0f, 0.0f, 1.0f, 1.0f};
        V(desc + 0x10) = Vec4{1.0f, 0.0f, 1.0f, 1.0f};
        V(desc + 0x20) = Vec4{-1.0f, 0.0f, -1.0f, 1.0f};
        V(desc + 0x30) = Vec4{1.0f, 0.0f, -1.0f, 1.0f};

        swc1(desc + 0x50, 0.0f);
        swc1(desc + 0x54, 1.0f);
        swc1(desc + 0x58, 1.0f);
        swc1(desc + 0x5c, 1.0f);
        swc1(desc + 0x60, 0.0f);
        swc1(desc + 0x64, 0.0f);
        swc1(desc + 0x68, 1.0f);
        swc1(desc + 0x6c, 0.0f);

        quad_draw(desc, mat_addr);
    }

    ee_stack_free(0xd0);
}

// orig: (none)
void decoy_fx_fns_register() {
    ovl_register_moby_fn(0x002c31c0, decoy_2c31c0_update);
}

}  // namespace rac1
