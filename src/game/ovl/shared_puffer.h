// The puffer of Batalia and Orxon (class 648): a vent that puffs clouds (shared_puffer.cpp: one port for both levels'
// copies). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_PUFFER_H
#define RAC1_GAME_OVL_SHARED_PUFFER_H

#include <cstdint>

namespace rac1 {

void puffer_update(uint32_t m);  // class 648 (L8:0x002f5830, L10:0x002c74b8)

}  // namespace rac1

#endif
