// Ported overlay code: particle type 0x08's update (0x0026a5b8).
#include <game/ovl/fx_26a5b8.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: 0x0026a5b8
void fx_26a5b8_update(uint32_t m) {
    vec3_add(V(m + 0x10), V(m + 0x10), V(m + 0x20));
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(m + 0xa))) != 0) {
        free_particle(m);
        return;
    }

    swc1(m + 0xc, fpu_add(lwc1(m + 0xc), lwc1(m + 0x3c)));

    const float total_f = int_to_float(lw(m + 0x38));
    const float t = fpu_div(static_cast<float>(lh(m + 0xa)), total_f);
    sw(m + 0x4, rgba_lerp(lwu(m + 0x34), lwu(m + 0x30), t));

    if (lh(m + 0xa) < frame_scale_i(6)) {
        const uint32_t alpha_max = lbu(m + 0x7);
        const int32_t fs6 = frame_scale_i(6);
        const int32_t num = static_cast<int32_t>(static_cast<uint32_t>(lh(m + 0xa)) * alpha_max);
        const uint32_t alpha = static_cast<uint32_t>(ee_div(num, fs6));
        sw(m + 0x4, (lwu(m + 0x4) & 0x00ffffffu) | (alpha << 24));
    }

    const int32_t total = lw(m + 0x38);
    const int32_t rem = lh(m + 0xa);
    const int32_t idx_num = static_cast<int32_t>((static_cast<uint32_t>(total) - static_cast<uint32_t>(rem)) * 10u);
    const uint32_t idx = static_cast<uint32_t>(ee_div(idx_num, total));
    sb(m + 0x2, lbu(lwu(OV(0x001b20a0u)) + idx));
}

// orig: (none) -- registers 0x0026a5b8 for indirect dispatch
void fx_26a5b8_fns_register() {
    ovl_register_moby_fn(0x0026a5b8, fx_26a5b8_update);
}

}  // namespace rac1
