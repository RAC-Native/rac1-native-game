// The homing missiles of Batalia and Hoven (class 0x1ca / 458): their launch and their flight (shared_homing_missile.cpp:
// one port for both levels' copies). Phase 1 of stage 4: the levels' generated copies still run; these are proved against
// them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_HOMING_MISSILE_H
#define RAC1_GAME_OVL_SHARED_HOMING_MISSILE_H

#include <cstdint>

namespace rac1 {

// a missile from `src` at `pos` with the velocity `vel` (EE vectors) homing on `target`; the missile or 0 (L8:0x002e7eb0, L12:0x002ef2e8)
uint32_t homing_missile_launch(uint32_t src, uint32_t pos, uint32_t vel, uint32_t target);
void homing_missile_update(uint32_t m);  // class 458 (L8:0x002e8210, L12:0x002ef648)

}  // namespace rac1

#endif
