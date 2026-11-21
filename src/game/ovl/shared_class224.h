// Classes 224 / 228, the pickups of the space levels (Gemlik Base, Drek's fleet): an ammo pickup (224) and a shield pickup
// (228) the ship flies through, which can wait for another moby's state, appear and spin, drift to the ship and are taken
// (shared_class224.cpp). One port of the two levels' copies of their update. Phase 1 of stage 4: the levels' generated
// copies still run; this is proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_CLASS224_H
#define RAC1_GAME_OVL_SHARED_CLASS224_H

#include <cstdint>

namespace rac1 {

void class224_update(uint32_t m);  // L13:0x002e1cc8 L17:0x002c53d8

}  // namespace rac1

#endif
