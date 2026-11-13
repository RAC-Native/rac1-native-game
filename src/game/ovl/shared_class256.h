// Class 256, a homing rocket (Quartu, Veldin): an earlier build of the Devastator's rocket (rocket.cpp), without the gold
// weapon's bounces or the particle load's thinning; one port of the two levels' copies of its update (shared_class256.cpp).
// Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle batch 505.
#ifndef RAC1_GAME_OVL_SHARED_CLASS256_H
#define RAC1_GAME_OVL_SHARED_CLASS256_H

#include <cstdint>

namespace rac1 {

void class256_update(uint32_t m);  // L15:0x0029d6c0 L18:0x002aa3b0

}  // namespace rac1

#endif
