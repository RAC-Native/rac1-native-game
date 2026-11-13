// The ship's draw callbacks on Aridia, and the one port of every level's copy of them (aridia_ship.cpp): the shadow / glow,
// the engine glow and the exhaust trails (Veldin's 0x28e670 / 0x28efc8 / 0x28f238, still transliterated on Veldin:
// ship_draw_cbs.h). Proved against the original on every level (batch 503).
#ifndef RAC1_GAME_OVL_ARIDIA_SHIP_H
#define RAC1_GAME_OVL_ARIDIA_SHIP_H

#include <cstdint>

namespace rac1 {

// (each is shared: the level's copy of Veldin's function, at the address the level's overlay map pairs with it)
void ari_ship_glow_draw_l2(uint32_t m);         // orig: L2:0x0028dde0 (Veldin 0x28e670)
void ari_ship_engine_glow_draw_l2(uint32_t m);  // orig: L2:0x0028e738 (Veldin 0x28efc8)
void ari_ship_takeoff_draw_l2(uint32_t m);      // orig: L2:0x0028e9a8 (Veldin 0x28f238)

}  // namespace rac1

#endif
