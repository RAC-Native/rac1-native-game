// The sea's draw callback of Kerwan, Umbris, Batalia and Oltanis (the callback the seas' controllers queue: classes 994,
// 460, 327 and 1260; Umbris's class 1018 draws a second sea), shared_sea_draw.cpp. Phase 1 of stage 4: the levels'
// generated copies still run; these are proved against them by oracle batch 520.
#ifndef RAC1_GAME_OVL_SHARED_SEA_DRAW_H
#define RAC1_GAME_OVL_SHARED_SEA_DRAW_H

#include <cstdint>

namespace rac1 {

// The level's sea (its wave record) drawn (ocean_draw_one); the moby unused.
void sea_draw(uint32_t m);  // L3:0x002dc990 L7:0x002f8038 L8:0x002da0d0 L14:0x00303350
// Umbris's second sea.
void sea_draw_2(uint32_t m);  // L7:0x0030d420

}  // namespace rac1

#endif
