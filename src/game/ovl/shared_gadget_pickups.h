// The gadgets the hero picks up before a panel of four pulsing lights: Aridia's class 1005 (the Trespasser) and Blarg's class
// 1016 (one update, the same code relinked; the class number picks the item and the scene), and Quartu's and Drek's fleet's
// class 1428 (shared_gadget_pickups.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved
// against them by oracle batch 520.
#ifndef RAC1_GAME_OVL_SHARED_GADGET_PICKUPS_H
#define RAC1_GAME_OVL_SHARED_GADGET_PICKUPS_H

#include <cstdint>

namespace rac1 {

// Class 1005 / 1016: 0 set up on the floor (gone once owned), 1 spinning until the hero is by it (its scene), 3 given
// (class 1005: a camera move to the panel; else the level changes), 4 the camera's move, 5 the screen faded, the level changed.
void gadget_pickup_update(uint32_t m);  // L2:0x002ea210 (class 1005) L6:0x002f4638 (class 1016)
// Class 1428: 0 set up (gone once owned), 1 spinning until the hero is by it (its scene), 2 the item's message and the
// level changed, 3 gone.
void class1428_update(uint32_t m);  // L15:0x002ebd78 L17:0x002f1790

}  // namespace rac1

#endif
