#ifndef RAC1_GAME_OVL_VISIBOMB_H
#define RAC1_GAME_OVL_VISIBOMB_H
// visibomb, ported by hand (visibomb.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void visibomb_203880();  // orig: 0x00203880
void visibomb_203b18();  // orig: 0x00203b18
void visibomb_2b56d0(uint32_t p0);  // orig: 0x002b56d0
void visibomb_2eb320(uint32_t p0, uint32_t p1, uint32_t p2, float pf12);  // orig: 0x002eb320
void visibomb_2eb648(uint32_t p0, uint32_t p1);  // orig: 0x002eb648
int32_t visibomb_2eb930(uint32_t p0);  // orig: 0x002eb930
int32_t visibomb_2b58d8(uint32_t p0, uint32_t p1, float pf12, float pf13);  // orig: 0x002b58d8
void visibomb_update(uint32_t m);  // orig: 0x002b3058
void visibomb_fns_register();  // orig: (none)

}  // namespace rac1
#endif
