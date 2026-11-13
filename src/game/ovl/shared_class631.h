// Class 631 (Kerwan, Umbris), a flying craft that follows its paths, carries a rider (class 574) and drops class 848's
// charges toward its target, shared by the two levels: one port of their copies (shared_class631.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS631_H
#define RAC1_GAME_OVL_SHARED_CLASS631_H

#include <cstdint>

namespace rac1 {

// The update: states 0 (set up) .. 5 (destroyed).
void class631_update(uint32_t m);  // L3:0x002cca00 L7:0x002fd908
// Its hits: the damage taken, destroyed (state 5) out of health, else thrown off its course (state 4) and tilted away.
void class631_hits(uint32_t m);  // L3:0x002ce238 L7:0x002ff140

}  // namespace rac1

#endif
