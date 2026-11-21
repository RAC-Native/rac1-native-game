// The linked movers of Umbris and Gemlik Base: a platform and a door that slide out and back when another moby reaches a
// state (shared_movers.cpp: one port for both levels' copies). Phase 1 of stage 4: the levels' generated copies still run;
// these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_MOVERS_H
#define RAC1_GAME_OVL_SHARED_MOVERS_H

#include <cstdint>

namespace rac1 {

void mover_platform_update(uint32_t m);  // class 1129 (L7:0x0031aee0), 104 / 106 (L13:0x002c32d0)
void mover_door_update(uint32_t m);      // class 1063 (L7:0x00310df0), 129 / 130 / 182 / 183 / 360 (L13:0x002c8390)

}  // namespace rac1

#endif
