#ifndef RAC1_GAME_OVL_DECOY_H
#define RAC1_GAME_OVL_DECOY_H
// decoy, ported by hand (decoy.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void decoy_2c3888(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002c3888
int32_t decoy_2c35c0(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002c35c0
void decoy_2d1f30(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14);  // orig: 0x002d1f30
void decoy_2d2158(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002d2158
void decoy_update(uint32_t m);  // orig: 0x002d2670
void decoy_fns_register();  // orig: (none)

}  // namespace rac1
#endif
