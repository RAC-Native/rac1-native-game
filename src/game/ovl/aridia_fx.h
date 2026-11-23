// Aridia's own draw callbacks of its classes: ported by hand (aridia_fx.cpp).
// Proved against the original.
#ifndef RAC1_GAME_OVL_ARIDIA_FX_H
#define RAC1_GAME_OVL_ARIDIA_FX_H

#include <cstdint>

namespace rac1 {

// ported by hand (aridia_fx.cpp)
void ari_elevator_draw_l2(uint32_t m);  // orig: L2:0x002dc2c8
void ari_vendor_orb_update_l2(uint32_t m);  // orig: L2:0x002ebcb8

}  // namespace rac1

#endif
