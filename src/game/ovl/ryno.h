#ifndef RAC1_GAME_OVL_RYNO_H
#define RAC1_GAME_OVL_RYNO_H
// ryno, ported by hand (ryno.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void ryno_2ccee0(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14);  // orig: 0x002ccee0
int32_t ryno_2cc360(uint32_t p0, uint32_t p1, uint32_t p2, float pf12, float pf13, float pf14);  // orig: 0x002cc360
void ryno_update(uint32_t m);  // orig: 0x002cc608
void ryno_fns_register();  // orig: (none)

}  // namespace rac1
#endif
