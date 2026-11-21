// Class 52, a floating creature that drifts after its target and bursts (Gaspar, Kalebo III): one port of the two levels'
// copies of its update and its hit handler (shared_class52.cpp). Phase 1 of stage 4: the levels' generated copies still
// run; these are proved against them by oracle batch 517.
#ifndef RAC1_GAME_OVL_SHARED_CLASS52_H
#define RAC1_GAME_OVL_SHARED_CLASS52_H

#include <cstdint>

namespace rac1 {

void class52_update(uint32_t m);  // L9:0x002c5990 L16:0x002a1088
// Its hits (knocked back, killed), its colour pulses and glow, and its target (pvars +0x120 / +0x160 / +0x164).
void class52_hit(uint32_t m);  // L9:0x002c6230 L16:0x002a1928

}  // namespace rac1

#endif
