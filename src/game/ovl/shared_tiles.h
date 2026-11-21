// The tile grids of Umbris, Pokitaru, Hoven and Gemlik Base (records of 0x1190 bytes in a level buffer: their place, their
// height, their eight neighbours) and Pokitaru's / Hoven's Raritanium pickup (class 37): shared_tiles.cpp, one port for the
// levels' copies. Phase 1 of stage 4: the levels' generated copies still run; proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_TILES_H
#define RAC1_GAME_OVL_SHARED_TILES_H

#include <cstdint>

namespace rac1 {

// tile `i`'s neighbours: the tiles 16 away in the eight directions (L7:0x0030cd48 L11:0x0030e730 L12:0x002bf770 L13:0x0030a488)
void tile_links(int32_t i);
// the moby `m`'s group: each member and its tile (its index at pvar +4 on Pokitaru, +0xc on Hoven) at the moby's height
void tile_group_height(uint32_t m);  // L11:0x0030e690 L12:0x002bf6d0
void raritanium_pickup_update(uint32_t m);  // class 37 (L11:0x002cb790, L12:0x002bf9b8)

}  // namespace rac1

#endif
