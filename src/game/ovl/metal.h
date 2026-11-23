#ifndef RAC1_GAME_OVL_METAL_H
#define RAC1_GAME_OVL_METAL_H
// metal, ported by hand (metal.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void metal_28df38(uint32_t p0, uint32_t p1);  // orig: 0x0028df38
void metal_25f8e0(uint32_t p0, float pf12);  // orig: 0x0025f8e0
void metal_2d2e28(uint32_t p0);  // orig: 0x002d2e28
void metal_update(uint32_t m);  // orig: 0x002d2ee8
void metal_fns_register();  // orig: (none)

}  // namespace rac1
#endif
