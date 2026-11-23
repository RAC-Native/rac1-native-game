// Hand-written VU0 sky entry points (0x28c14c, 0x28c3c0, 0x28bd58, 0x28c298) forwarding directly to sky_hand.cpp.
// orig: (host)
#include <game/ovl/sky_ee.h>
#include <game/ovl/sky_hand.h>

namespace rac1 {

// orig: 0x0028c14c
uint32_t ovl_28c14c_tr(uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t v0 = 0;
    sky_hand::project(a0, a1, a2, v0);
    return v0;
}

// orig: 0x0028c3c0
uint32_t ovl_28c3c0_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t v0 = 0;
    sky_hand::triangles(a0, a1, a2, a3, v0);
    return v0;
}

// orig: 0x0028bd58
uint32_t ovl_28bd58_tr() {
    uint32_t v0 = 0;
    sky_hand::sprites(v0);
    return v0;
}

// orig: 0x0028c298
uint32_t ovl_28c298_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return sky_hand::colour_triangles(a0, a1, a2, a3);
}

}  // namespace rac1
