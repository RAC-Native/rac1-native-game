// Class 1209 of Quartu and Drek's fleet: a floor switch of a group (shared_floor_switch.cpp: one port of both levels'
// copies, the same code relinked). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them
// by oracle batch 519.
#ifndef RAC1_GAME_OVL_SHARED_FLOOR_SWITCH_H
#define RAC1_GAME_OVL_SHARED_FLOOR_SWITCH_H

#include <cstdint>

namespace rac1 {

// Class 1209's update: the first switch of a group leads the others; each lights up while the hero stands on it, and the
// group (its moby group, +0x21) is set to state 4 once all are lit before the lead's timer runs out.
void floor_switch_update(uint32_t m);  // L15:0x002e5958 L17:0x002e5f50

}  // namespace rac1

#endif
