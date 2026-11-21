// The deadly zones of Blarg, Batalia, Gemlik Base and Veldin's finale (class 1039): shared_zones.cpp, one port for the four
// levels' copies. Phase 1 of stage 4: the levels' generated copies still run; proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_ZONES_H
#define RAC1_GAME_OVL_SHARED_ZONES_H

#include <cstdint>

namespace rac1 {

void deadly_zones_update(uint32_t m);  // class 1039 (L6:0x002f7930 L8:0x00302a78 L13:0x003036f0 L18:0x002ec858)

}  // namespace rac1

#endif
