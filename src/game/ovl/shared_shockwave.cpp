// The shock waves of Quartu (level 15) and Veldin's finale (18), class 1427: a ring that grows by its speed each frame
// (pvar +0), fades over its last 40 frames (+8 its life), and hurts Ratchet where it reaches him (+0xc the damage). The two
// levels' copies are the same code, relinked: no level word (the hero's moby 0x1413d0 is the base ELF's). Header
// shared_shockwave.h.
#include <game/ovl/shared_shockwave.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// orig: L15:0x0029ead0 L18:0x002ab7c0
void shockwave_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cls = lwu(m + 0x24);
    const float radius = fpu_add(fpu_mul(fpu_div(lwc1(m + 0x2c), lwc1(cls + 0x24)), 0.5f), lwc1(pv));  // (its radius now, grown)
    swc1(m + 0x2c, fpu_add(radius, radius));
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), lwc1(cls + 0x24)));
    if (lw(pv + 8) < frame_scale_i(0x28)) {  // (fading)
        const int32_t fade = frame_scale_i(0x28);
        sb(m + 0x23, static_cast<uint32_t>(ee_div(lw(pv + 8) * 127, fade)));
    }
    moby_sphere_contact(lwu(0x001413d0), m + 0x10, 0x30000, 0, 1, 0, radius, static_cast<float>(float_to_int(lwc1(pv + 0xc))), 1.0f);
    int32_t life = lw(pv + 8);
    const int32_t ticked = timer_tick_i32(life);
    sw(pv + 8, static_cast<uint32_t>(life));
    if (ticked != 0) moby_retire(m);
}

}  // namespace rac1
