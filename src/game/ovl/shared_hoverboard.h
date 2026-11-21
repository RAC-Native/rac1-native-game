// The hoverboard races' shared code (Rilgar and Kalebo III): one port of the two levels' copies (shared_hoverboard.cpp).
#ifndef RAC1_GAME_OVL_SHARED_HOVERBOARD_H
#define RAC1_GAME_OVL_SHARED_HOVERBOARD_H

#include <cstdint>

namespace rac1 {

// Class 439, the hoverboard the hero steps on.
void class439_update(uint32_t m);  // L5:0x002f87a8 L16:0x002c7788
// The race's boost pads (classes 0x473 and 0x474 in the board's groups): over one, the race controller `m` (class 717 on
// Rilgar, 556 on Kalebo III) adds to its boost (pvars `boost`, a half) and waits a second (pvars `wait`); `at` is the
// pvars' offset of the point it tests. Rilgar's controller: at 0x200, boost 0x25a, wait 0x264; Kalebo III's: 0x260,
// 0x2f6, 0x2dc.
void hoverboard_boost_pads(uint32_t m, uint32_t at, uint32_t boost, uint32_t wait);  // L5:0x00307010 L16:0x002cfc00
// The race's HUD element: its start, update, reset and draw callbacks.
void race_hud_start(uint32_t e);   // L5:0x00262ae8 L16:0x002351f8
void race_hud_update(uint32_t e);  // L5:0x002661e8 L16:0x002388f8
void race_hud_reset(uint32_t e);   // L5:0x002661a0 L16:0x002388b0
void race_hud_draw(uint32_t e);    // L5:0x00266710 L16:0x00238e20

}  // namespace rac1

#endif
