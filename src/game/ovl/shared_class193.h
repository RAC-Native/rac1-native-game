// Class 193, a walking creature that wanders about its home, charges the hero (or another target) in range, strikes and
// returns, knocked about by hits: one port of the copies Gaspar and Quartu carry (shared_class193.cpp). Every level with a
// copy runs this port; it is proved against each level's own copy by oracle batch 514.
#ifndef RAC1_GAME_OVL_SHARED_CLASS193_H
#define RAC1_GAME_OVL_SHARED_CLASS193_H

#include <cstdint>

namespace rac1 {

void class193_update(uint32_t m);  // L9:0x002e27d8 L15:0x002bc608

}  // namespace rac1

#endif
