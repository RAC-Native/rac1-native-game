// Classes 196 (Quartu) and 197 (Quartu, Drek's fleet): a lift that slides out along its facing when another moby reaches a
// state or the hero steps into a box, and back when he leaves another; one port of the two levels' copies of its update
// (shared_class196.cpp). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle
// batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS196_H
#define RAC1_GAME_OVL_SHARED_CLASS196_H

#include <cstdint>

namespace rac1 {

void class196_update(uint32_t m);  // L15:0x002bddb0 L17:0x002c0f00

}  // namespace rac1

#endif
