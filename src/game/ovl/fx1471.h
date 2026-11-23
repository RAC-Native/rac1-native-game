#ifndef RAC1_GAME_OVL_FX1471_H
#define RAC1_GAME_OVL_FX1471_H
// The class-1471 effect manager's beams (the shooting robot 1440's beam), ported by hand (fx1471.cpp). Addresses are
// Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void fx1471_effect_start(uint32_t p0, uint32_t p1);  // orig: 0x002e2250
void fx1471_beam_prepare(uint32_t p0, uint32_t p1);  // orig: 0x002e3388
void fx1471_beam_strip(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5);  // orig: 0x002e2c88
void fx1471_draw(uint32_t m);  // orig: 0x002e2af0
void fx1471_fns_register();  // orig: (none)

}  // namespace rac1
#endif
