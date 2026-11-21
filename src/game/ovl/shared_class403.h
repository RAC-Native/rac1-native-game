// Class 403, a rocket that rides a path and then flies free with a smoke trail until its fuse or a hit blows it up
// (Oltanis, Quartu), shared by the two levels that have it: one port of the levels' copies of its update and its trail
// (shared_class403.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by
// oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS403_H
#define RAC1_GAME_OVL_SHARED_CLASS403_H

#include <cstdint>

namespace rac1 {

void class403_update(uint32_t m);  // L14:0x002dfd10 L15:0x002ca9f8
// The trail: sparks and smoke behind the rocket.
void class403_trail(uint32_t m);   // L14:0x002e03b8 L15:0x002cb0a0

}  // namespace rac1

#endif
