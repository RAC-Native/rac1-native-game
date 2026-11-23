// Aridia's own HUD element: the callbacks Skid's update hands to the HUD's element request (slots 5 / 7, keys 0x7d0 /
// 0x7d1), Aridia-only code ported by hand (aridia_hud.cpp). Proved against the original.
#ifndef RAC1_GAME_OVL_ARIDIA_HUD_H
#define RAC1_GAME_OVL_ARIDIA_HUD_H

#include <cstdint>

namespace rac1 {

// ported by hand (aridia_hud.cpp)
void ari_skid_hud_start_l2(uint32_t e);   // orig: L2:0x0023c990
void ari_skid_hud_update_l2(uint32_t e);  // orig: L2:0x0023ccc8
void ari_skid_hud_draw_l2(uint32_t e);    // orig: L2:0x0023cda8

}  // namespace rac1

#endif
