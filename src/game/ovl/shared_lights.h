// The flickering lights of Novalis and Blarg (class 1504): shared_lights.cpp, one port for both levels' copies. Phase 1 of
// stage 4: the levels' generated copies still run; this is proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_LIGHTS_H
#define RAC1_GAME_OVL_SHARED_LIGHTS_H

#include <cstdint>

namespace rac1 {

void flicker_light_update(uint32_t m);  // class 1504 (L1:0x0030b618, L6:0x00308868)

}  // namespace rac1

#endif
