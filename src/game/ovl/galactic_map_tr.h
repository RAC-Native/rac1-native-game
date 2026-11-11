// Galactic map galaxy view (0x2033c0) and planet panel model (0x27cbc0; implemented in galactic_map.cpp).
#ifndef RAC1_GAME_OVL_GALACTIC_MAP_TR_H
#define RAC1_GAME_OVL_GALACTIC_MAP_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

TrRet ovl_23bef8_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x0023bef8
TrRet ovl_23c920_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x0023c920
TrRet galaxy_view_draw_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x002033c0
TrRet galmap_planet_draw_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x0027cbc0

}  // namespace rac1

#endif
