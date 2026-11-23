// Galactic map galaxy view (0x2033c0) and planet panel model (0x27cbc0; implemented in galactic_map.cpp).
#ifndef RAC1_GAME_OVL_GALACTIC_MAP_TR_H
#define RAC1_GAME_OVL_GALACTIC_MAP_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

uint32_t ovl_23bef8_tr(uint32_t tex_idx, uint32_t x16, uint32_t y16, uint32_t w16, uint32_t h16, uint32_t u16, uint32_t v16, uint32_t alpha);  // 0x0023bef8
uint32_t ovl_23c920_tr(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t colour, uint32_t subpixel);  // 0x0023c920
void galaxy_view_draw(uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1);  // 0x002033c0
uint32_t galmap_planet_draw_base(uint32_t w);  // 0x0027cbc0 (galactic_map.h's galmap_planet_draw: with a level mod's row)

}  // namespace rac1

#endif
