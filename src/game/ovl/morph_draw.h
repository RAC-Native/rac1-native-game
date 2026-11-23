#ifndef RAC1_GAME_OVL_MORPH_DRAW_H
#define RAC1_GAME_OVL_MORPH_DRAW_H
// morph_draw, ported by hand (morph_draw.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void morph_2bdc50_update(uint32_t m);  // orig: 0x002bdc50
void morph_draw_2be078(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, float pf12, float pf13, float pf14);  // orig: 0x002be078
void morph_draw_2be650();  // orig: 0x002be650
void morph_draw_2be890(uint32_t p0);  // orig: 0x002be890
void morph_draw_fns_register();  // orig: (none)

}  // namespace rac1
#endif
