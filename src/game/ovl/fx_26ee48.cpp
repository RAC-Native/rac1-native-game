// Ported overlay code: particle type 0x14's update (0x0026ee48).
#include <game/ovl/fx_26ee48.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: 0x0026ee48
void fx_26ee48_update(uint32_t m) {
    if (m == 0) return;

    const uint32_t owner = lwu(m + 0x20);
    const uint32_t q = m + 0x20u;
    if (owner == 0 || (lbu(owner + 0x20) & 0xf0u) == 0xf0u) {
        free_particle(m);
        return;
    }

    lq_sq(m + 0x10, owner + 0x10);
    const float step = lwc1(q + 0x8);
    float alpha = fpu_add(lwc1(q + 0x4), step);
    swc1(q + 0x4, alpha);
    if (alpha < 0.0f) {
        free_particle(m);
        return;
    }

    if (u2f(0x3f19999au) < alpha) {
        const float neg_step = -step;
        alpha = fpu_add(alpha, neg_step);
        swc1(q + 0x8, neg_step);
        swc1(q + 0x4, alpha);
    }

    const float r = fpu_add(lwc1(q + 0x10), fpu_mul(lwc1(0x0015ed60u), u2f(0xbc8e8a72u)));
    swc1(q + 0x10, r < 0.0f ? 0.0f : r);

    const float g = fpu_add(lwc1(q + 0x14), fpu_mul(lwc1(0x0015ed60u), u2f(0xbc6ce9a3u)));
    swc1(q + 0x14, g < 0.0f ? 0.0f : g);

    const float b = fpu_add(lwc1(q + 0x18), fpu_mul(lwc1(0x0015ed60u), u2f(0xbac49ba6u)));
    swc1(q + 0x18, b < 0.0f ? 0.0f : b);

    sw(m + 0x4, rgba_pack(lwc1(q + 0x10), lwc1(q + 0x14), lwc1(q + 0x18), lwc1(q + 0x4)));

    float rot = fpu_add(lwc1(q + 0xc), fpu_mul(lwc1(0x0015ed60u), u2f(0x3a902de0u)));
    if (1.0f < rot) {
        rot = fpu_sub(rot, 1.0f);
    }
    swc1(q + 0xc, rot);
    sb(m + 0x8, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(q + 0xc), 255.0f))));

    const uint32_t cur_owner = lwu(m + 0x20);
    const float sz = fpu_add(fpu_mul(fpu_mul(lwc1(cur_owner + 0x2c), u2f(0x47c35000u)), u2f(0x41f00000u)), u2f(0x47435000u));
    swc1(m + 0xc, sz);
}

// orig: (none) -- registers 0x0026ee48 for indirect dispatch
void fx_26ee48_fns_register() {
    ovl_register_moby_fn(0x0026ee48, fx_26ee48_update);
}

}  // namespace rac1
