// The shock waves of Quartu and Veldin's finale (class 1427): shared_shockwave.cpp, one port for both levels' copies.
// Phase 1 of stage 4: the levels' generated copies still run; proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_SHOCKWAVE_H
#define RAC1_GAME_OVL_SHARED_SHOCKWAVE_H

#include <cstdint>

namespace rac1 {

void shockwave_update(uint32_t m);  // class 1427 (L15:0x0029ead0, L18:0x002ab7c0)

}  // namespace rac1

#endif
