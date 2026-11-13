// Class 686, a burning shell that flies (smoking) to its target point and blows up there in a shower of sparks, smoke and
// shards, waking the mobys of class 700 it names (Novalis, Oltanis): one port of the two levels' copies of its update
// (shared_class686.cpp). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle
// batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS686_H
#define RAC1_GAME_OVL_SHARED_CLASS686_H

#include <cstdint>

namespace rac1 {

void class686_update(uint32_t m);  // L1:0x002f6a30 L14:0x002ef840

}  // namespace rac1

#endif
