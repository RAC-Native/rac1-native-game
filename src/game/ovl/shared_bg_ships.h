// The background ships of Aridia, Gaspar and Orxon (class 1213 on Aridia and Gaspar, 1973 on Orxon: the same code), and
// the helpers their two kinds share (the glow's draw request, the exhaust puffs): one port for the copies of the three
// levels (shared_bg_ships.cpp, where every level's address is). Phase 1 of stage 4: the levels' generated copies still
// run; these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_BG_SHIPS_H
#define RAC1_GAME_OVL_SHARED_BG_SHIPS_H

#include <cstdint>

namespace rac1 {

// The glow's draw: its chain points `first` .. into the pvars +0x80 (`points` of them), its colour (+0x90..+0x92, alpha
// 0x30) after them, and once a frame the draw callback `draw` asked for (L2:0x002ec3f0 / 0x002ecd50 ...).
void bg_ship_glow_prep(uint32_t m, uint32_t first, uint32_t points, uint32_t draw);
// The exhaust: one puff (particle 2) per chain point 0 .. count-1 along the way the ship moved from `from` (an EE vector),
// tuned by the words at `scales` (two speeds, a scatter), `timing` (three frame counts, a spread, two ranges) and
// `colours` (two colour pairs); `slowed` divides the ranges by 1.5 (L2:0x002eca30 / 0x002ed420 / 0x002ed658 ...).
void bg_ship_exhaust(uint32_t m, uint32_t from, int32_t count, uint32_t scales, uint32_t timing, uint32_t colours, bool slowed);
// the ship of class 1213 / 1973 (L2:0x002ece18, L9:0x00306720, L10:0x002ebb08)
void bg_ship2_update(uint32_t m);

}  // namespace rac1

#endif
