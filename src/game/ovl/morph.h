#ifndef RAC1_GAME_OVL_MORPH_H
#define RAC1_GAME_OVL_MORPH_H
// morph, ported by hand (morph.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void morph_2bcf98(uint32_t p0);  // orig: 0x002bcf98
void morph_275910(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, float pf12, float pf13);  // orig: 0x00275910
void morph_2bd0a0(uint32_t p0);  // orig: 0x002bd0a0
void morph_2c92d8(uint32_t p0);  // orig: 0x002c92d8
void morph_2c9348(uint32_t p0);  // orig: 0x002c9348
int32_t morph_2bc3b0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);  // orig: 0x002bc3b0
void morph_update(uint32_t m);  // orig: 0x002bc7e8
void morph_fns_register();  // orig: (none)

}  // namespace rac1
#endif
