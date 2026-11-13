// The exhaust of Pokitaru's class 1242 and Drek's fleet's class 1379 (the ships the hero rides; shared_ship_exhaust.cpp: one
// port of both levels' copies, the same code relinked). Phase 1 of stage 4: the levels' generated copies still run; this is
// proved against them by oracle batch 519.
#ifndef RAC1_GAME_OVL_SHARED_SHIP_EXHAUST_H
#define RAC1_GAME_OVL_SHARED_SHIP_EXHAUST_H

#include <cstdint>

namespace rac1 {

// The ship `m`'s exhaust at its chain points 1 and 2: three flames each (particles 0x4a riding on the ship) blown back by its
// thrust (`pv` +0xc4) and its speed (+0x64), five when the thrust is past half.
void ship_exhaust_spawn(uint32_t m, uint32_t pv);  // L11:0x003112b8 L17:0x002ea688

}  // namespace rac1

#endif
