// Class 1331, the manager of the three effect slots (Veldin's class 1471's, a build of their own that also lets go of a
// slot whose moby died) on Oltanis and Quartu: one port of the two levels' copies of its update (shared_class1331.cpp).
// Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1331_H
#define RAC1_GAME_OVL_SHARED_CLASS1331_H

#include <cstdint>

namespace rac1 {

void class1331_update(uint32_t m);  // L14:0x003039e0 L15:0x002e8910

}  // namespace rac1

#endif
