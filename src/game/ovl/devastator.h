#ifndef RAC1_GAME_OVL_DEVASTATOR_H
#define RAC1_GAME_OVL_DEVASTATOR_H
// The Devastator (item 0x0b, hand class 0x9d): the hand's update and its helpers, ported by hand
// (devastator.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

int32_t dev_25f878(uint32_t p0);                                              // orig: 0x0025f878
float dev_2aff50(uint32_t p0);                                              // orig: 0x002aff50
int32_t dev_2712b8(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, float pf12, float pf13, float pf14, float pf15, float pf16);  // orig: 0x002712b8
void dev_2afc70(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14, float pf15, float pf16);  // orig: 0x002afc70
void dev_2b1af0(uint32_t p0);                                                // orig: 0x002b1af0
void dev_2b1de0(uint32_t p0);                                                // orig: 0x002b1de0
void devastator_update(uint32_t m);                                          // orig: 0x002b2100
void devastator_fns_register();                                              // orig: (none)

}  // namespace rac1
#endif
