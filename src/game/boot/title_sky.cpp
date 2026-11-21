// The title screen's sky shells: the boot module's own sky variant (front:0x0022ae70), called by title_sky
// (front:0x001e9ab8) between the sky pass's start (0x28b680) and end (0x28b710). Draws background shells 0 and 1,
// seeds and updates the 256 star sprites (246 twinkling stars in a band + 10 orbiting shooting stars), draws the
// sprite batch (0x28bd58), sets GS ALPHA_1, and draws foreground shells 2 and 3.
#include <game/boot/title_sky.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sky_ee.h>
#include <game/port_stubs.h>

namespace rac1 {

// orig: front:0x0022ae70
TrRet front_sky_shells_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float,
                          float, float) {
    sh(lwu(FR(0x0016045cu)) + 4, 0);
    mat4_identity(M(FR(0x001d96e0u)));
    ovl_28b848_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    ovl_28b848_tr(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    uint32_t sky = lwu(FR(0x0016045cu));
    if (lh(sky + 8) == 0) {
        sh(sky + 8, 0x100);
        libc_srand(0x3039);
        sky = lwu(FR(0x0016045cu));
        if (lh(sky + 8) > 0) {
            const float kRadius = u2f(0x42480000u);  // 50.0f
            uint32_t list = lwu(sky + 0x1c);
            int32_t i = 0;
            for (;;) {
                const uint32_t s = list + static_cast<uint32_t>(i) * 0x20u;
                if (i < 246) {
                    sb(s + 0x0, 1);
                    const int32_t phase = rand_mod(0x100);
                    sb(s + 0x2, 1);
                    sb(s + 0x3, 0x48);
                    sh(s + 0xc, static_cast<uint16_t>(phase));
                    swc1(s + 0x8, rand_angle());
                    const float size = fpu_mul(int_to_float(rand_mod(0x18) + 0x20), u2f(0x3b800000u));
                    swc1(s + 0x1c, size);

                    const float az = angle_add(u2f(0xc0400000u), fpu_mul(rand_angle(), u2f(0x3e4ccccdu)));
                    const float el = fpu_add(fpu_mul(rand_angle(), u2f(0x3db851ecu)), u2f(0x3f99999au));
                    swc1(s + 0x10, fpu_mul(fpu_mul(cos_vu(az), sin_vu(el)), kRadius));
                    swc1(s + 0x14, fpu_mul(fpu_mul(sin_vu(az), sin_vu(el)), kRadius));
                    swc1(s + 0x18, fpu_mul(cos_vu(el), kRadius));

                    const uint32_t tint = static_cast<uint32_t>(rand_mod(0x18));
                    const uint32_t alpha_hi = static_cast<uint32_t>(rand_mod(0x20)) << 24;
                    const uint32_t coin = static_cast<uint32_t>(libc_rand() >> 16) & 1u;
                    uint32_t col;
                    if (coin != 0) {
                        col = alpha_hi + ((tint << 16) + 0x30505050u);
                    } else {
                        col = (alpha_hi + ((tint << 8) + 0x30505050u)) | tint;
                    }
                    sw(s + 0xc, col);
                } else {
                    sb(s + 0x0, 0);
                    sh(s + 0xc, static_cast<uint16_t>(libc_rand() >> 16));
                    sh(s + 0xe, static_cast<uint16_t>(libc_rand() >> 16));
                    swc1(s + 0x1c, u2f(0x3e23d70au));  // 0.16f
                    sb(s + 0x2, 1);
                    sb(s + 0x3, 0x48);
                }
                sky = lwu(FR(0x0016045cu));
                ++i;
                if (i >= lh(sky + 8)) break;
                list = lwu(sky + 0x1c);
            }
        }
        sky = lwu(FR(0x0016045cu));
    }

    if (lh(sky + 8) > 0) {
        const float kStep = u2f(0x3ac90fdbu);    // pi / 2048
        const float kRadius = u2f(0x42480000u);  // 50.0f
        uint32_t list = lwu(sky + 0x1c);
        int32_t i = 0;
        for (;;) {
            const uint32_t s = list + static_cast<uint32_t>(i) * 0x20u;
            if (lbu(s + 0x0) == 0) {
                sh(s + 0xc, static_cast<uint16_t>(lhu(s + 0xc) + 1u));
                sh(s + 0xe, static_cast<uint16_t>(lhu(s + 0xe) + 1u));
                const float az = fpu_mul(int_to_float(static_cast<int32_t>(lhu(s + 0xc) & 0xfffu) - 0x800), kStep);
                const float el = fpu_mul(int_to_float(static_cast<int32_t>(lhu(s + 0xe) & 0xfffu) - 0x800), kStep);
                swc1(s + 0x10, fpu_mul(fpu_mul(cos_vu(az), sin_vu(el)), kRadius));
                swc1(s + 0x14, fpu_mul(fpu_mul(sin_vu(az), sin_vu(el)), kRadius));
                const float z = fpu_mul(abs_f(cos_vu(el)), kRadius);
                const bool flash = (lhu(s + 0xc) & 0x3fu) < 8u;
                swc1(s + 0x18, z);
                sw(s + 0x4, (flash ? 0x70200000u : 0x20200000u) | 0x20f0u);
            } else {
                const uint32_t r = static_cast<uint32_t>(libc_rand() >> 16);
                const uint32_t delta = (((r & 0x1f00u) << 10) + 0xffdfdfe0u) + ((r & 0x1f0u) << 6) + ((r & 0x1fu) << 2);
                sw(s + 0x4, lwu(s + 0xc) + delta);
            }
            sky = lwu(FR(0x0016045cu));
            ++i;
            if (i >= lh(sky + 8)) break;
            list = lwu(sky + 0x1c);
        }
    }

    ovl_28bd58_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(0x42, (uint64_t{0x8000} << 24) | 0x44u);
    ovl_28b848_tr(2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return ovl_28b848_tr(3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

}  // namespace rac1
