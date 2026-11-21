// Hand-written VU0 sky entry points (0x28c14c, 0x28c3c0, 0x28bd58, 0x28c298, 0x28c680) forwarding directly to sky_hand.cpp.
// orig: (host)
#include <game/ovl/sky_ee.h>
#include <game/ovl/sky_hand.h>

namespace rac1 {

// orig: 0x0028c14c
TrRet ovl_28c14c_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    uint32_t v0 = 0;
    sky_hand::project(a0, a1, a2, v0);
    return TrRet{v0, 0.0f};
}

// orig: 0x0028c3c0
TrRet ovl_28c3c0_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    uint32_t v0 = 0;
    sky_hand::triangles(a0, a1, a2, a3, v0);
    return TrRet{v0, 0.0f};
}

// orig: 0x0028bd58
TrRet ovl_28bd58_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    uint32_t v0 = 0;
    sky_hand::sprites(v0);
    return TrRet{v0, 0.0f};
}

// orig: 0x0028c298
TrRet ovl_28c298_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    return TrRet{sky_hand::colour_triangles(a0, a1, a2, a3), 0.0f};
}

// orig: 0x0028c680
TrRet ovl_28c680_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sky_hand::spheres(a0, a1, a2);
    return TrRet{0, 0.0f};
}

}  // namespace rac1
