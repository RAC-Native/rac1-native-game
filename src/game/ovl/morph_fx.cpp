// Ported overlay code: Morph-o-Ray particle type 0x4e update (0x00275aa0).
#include <game/ovl/morph_fx.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: 0x00275aa0
void morph_275aa0_update(uint32_t m) {
    const uint32_t q = m + 0x20u;
    sb(m + 0x8u, lbu(m + 0x8u) + lbu(q + 0xau));

    const int32_t rem0 = lh(m + 0xau);
    const int32_t total = lh(q + 0x8u);
    const int32_t elapsed = static_cast<int32_t>(static_cast<uint32_t>(total) - static_cast<uint32_t>(rem0));

    float alpha_f;
    if (elapsed < 6) {
        const float s1 = lwc1(q + 0x4u);
        const float t = fpu_div(static_cast<float>(elapsed), 5.0f);
        const float s0 = lwc1(m + 0x20u);
        const float mid = fpu_mul(fpu_add(s0, s1), 0.5f);
        swc1(m + 0xcu, fpu_add(s1, fpu_mul(fpu_sub(mid, s1), t)));

        const uint32_t b0 = lbu(q + 0xbu);
        const uint32_t half_b = b0 >> 1;
        const int32_t diff_b = static_cast<int32_t>(half_b - b0);
        alpha_f = fpu_add(static_cast<float>(static_cast<int32_t>(b0)), fpu_mul(static_cast<float>(diff_b), t));
    } else {
        const float denom = int_to_float(static_cast<int32_t>(static_cast<uint32_t>(total) - 6u));
        const float s0 = lwc1(m + 0x20u);
        const float s1 = lwc1(q + 0x4u);
        const int32_t rem = lh(m + 0xau);
        const float mid = fpu_mul(fpu_add(s0, s1), 0.5f);
        const float t = fpu_div(static_cast<float>(rem), denom);
        swc1(m + 0xcu, fpu_add(s0, fpu_mul(fpu_sub(mid, s0), t)));

        const uint32_t half_b = lbu(q + 0xbu) >> 1;
        alpha_f = fpu_add(fpu_mul(static_cast<float>(static_cast<int32_t>(half_b)), t), 0.0f);
    }

    const uint32_t alpha = static_cast<uint32_t>(float_to_int(alpha_f));
    sw(m + 0x4u, (alpha << 24) | (lwu(m + 0x4u) & 0x00ffffffu));

    Vec4 vel{lwc1(q + 0xcu), lwc1(q + 0x10u), lwc1(q + 0x14u), 0.0f};
    vec3_add(V(m + 0x10u), V(m + 0x10u), vel);

    const uint32_t target = lwu(q + 0x18u);
    if (target != 0u) {
        Vec4 to_tgt{};
        vec3_sub(to_tgt, V(target + 0x10u), V(m + 0x10u));
        if (to_tgt.x <= 2.0f && to_tgt.y <= 2.0f) {
            Vec4 swirl{};
            vec3_cross_ba(swirl, to_tgt, V(0x0013f5e0u));
            vec3_set_length(swirl, swirl, lwc1(OV(0x0016024cu)));
            vec3_add(V(m + 0x10u), V(m + 0x10u), swirl);

            if (to_tgt.x <= 0.5f && to_tgt.y <= 0.5f) {
                vec3_set_length(vel, to_tgt, u2f(0x3cf5c28fu));
            } else {
                const float spd = vec3_length(vel);
                vec3_set_length(vel, to_tgt, spd);
            }
            const float vz = fpu_add(vel.z, u2f(0x3ca3d70au));
            swc1(q + 0xcu, vel.x);
            swc1(q + 0x10u, vel.y);
            swc1(q + 0x14u, vz);
        }
    }

    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(m + 0xau))) != 0) {
        free_particle(m);
    }
}

// orig: (none)
void morph_fx_fns_register() {
    ovl_register_moby_fn(0x00275aa0, morph_275aa0_update);
}

}  // namespace rac1
