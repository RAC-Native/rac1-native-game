#ifndef RAC1_GAME_OVL_ROCKET_H
#define RAC1_GAME_OVL_ROCKET_H
// rocket, ported by hand (rocket.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void rocket_2affa8(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002affa8
void rocket_1ffe20(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x001ffe20
void rocket_1ff2c0(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x001ff2c0
void rocket_1ffd48(uint32_t p0, uint32_t p1, float pf12);  // orig: 0x001ffd48
void devastator_rocket_update(uint32_t m);  // orig: 0x002b03a0
void rocket_fns_register();  // orig: (none)

}  // namespace rac1
#endif
