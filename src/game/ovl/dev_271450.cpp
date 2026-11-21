// Ported overlay code: Devastator smoke / exhaust particle (type 0x2c, update 0x00271450)
// and VU0 in-place xyz vector scale helper 0x001ff318.
#include <game/ovl/dev_271450.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// In-place VU0 xyz scale: V(p1).xyz *= pf12, preserving V(p1).w (p0 is unused).
// orig: 0x001ff318
void dev_271450_1ff318(uint32_t p0, uint32_t p1, float pf12) {
    (void)p0;
    Vec4& v = V(p1 & ~15u);
    vec3_scale(v, v, pf12);
}

// Particle type 0x2c update: fades alpha linearly with remaining life, advances position by
// velocity plus carried hero velocity (m + 0x1c), applies gravity along hero up (0x0013f5e0)
// and damping, and steps rotation and size.
// orig: 0x00271450
void devastator_271450_update(uint32_t m) {
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(m + 0xa))) != 0) {
        free_particle(m);
        return;
    }

    const uint32_t ext = m + 0x20;
    const float alpha0 = int_to_float(static_cast<int32_t>(lh(ext + 0x1e)));
    const float life0 = int_to_float(static_cast<int32_t>(lh(ext + 0x1c)));
    const float rem = static_cast<float>(static_cast<int32_t>(lh(m + 0xa)));
    const uint32_t alpha = static_cast<uint32_t>(float_to_int(fpu_mul(alpha0, fpu_div(rem, life0))));
    const uint32_t col = (alpha << 24) | (lbu(ext + 0x13) << 16) | (lbu(ext + 0x12) << 8) | lbu(ext + 0x11);
    sw(m + 4, col);

    Vec4 vel{lwc1(m + 0x20), lwc1(ext + 4), lwc1(ext + 8), 0.0f};
    vec3_add(V(m + 0x10), V(m + 0x10), vel);

    Vec4 carry = V(0x0013f490u);
    vec3_scale(carry, carry, lwc1(m + 0x1c));
    vec3_add_inplace(V(m + 0x10), carry);

    Vec4 grav{};
    vec3_scale(grav, V(0x0013f5e0u), lwc1(ext + 0xc));
    vec3_add(vel, vel, grav);
    vec3_scale(vel, vel, lwc1(ext + 0x18));

    swc1(m + 0x20, vel.x);
    swc1(ext + 4, vel.y);
    swc1(ext + 8, vel.z);
    sb(m + 8, lbu(m + 8) + lbu(ext + 0x10));
    swc1(m + 0xc, fpu_add(lwc1(m + 0xc), lwc1(ext + 0x14)));
}

// orig: (none)
void dev_271450_fns_register() {
    ovl_register_moby_fn(0x00271450, devastator_271450_update);
}

}  // namespace rac1
