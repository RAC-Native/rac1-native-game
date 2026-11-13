// The level's height map (the ground's height under a point, as a byte grid) and the dust puff the ground's points raise:
// Veldin's functions 0x263468 / 0x2634f8 / 0x268f10, which Batalia, Hoven and Oltanis carry copies of (shared_height_map.cpp).
// The levels' addresses are given without the `orig:` prefix the code generator reads: their copies stay transliterated
// in their units until each level is re-glued (then the declarations take Veldin's `// orig: 0x...` form).
#ifndef RAC1_GAME_OVL_SHARED_HEIGHT_MAP_H
#define RAC1_GAME_OVL_SHARED_HEIGHT_MAP_H

#include <cstdint>

#include <game/math/vecmath.h>

namespace rac1 {

// The ground's height under (x, y) of `p` from the level's height map.
float ground_height_at(const Vec4& p);  // L0:0x00263468 L8:0x0026d3e0 L12:0x0027bb20 L14:0x0026b008
// A puff of dust (particle kind 1) at `pos` (an EE address; its z raised by 0.1 first), of alpha `alpha`.
void ground_puff_spawn(uint32_t pos, uint32_t alpha);  // L0:0x00268f10 L8:0x00273080 L12:0x002817e8 L14:0x00270a78
// A puff where the ground under `pos` (an EE address) meets the world, when the ground there is below the camera's
// height + 5: fainter the further it is from the camera.
void ground_puff_at(uint32_t pos);  // L0:0x002634f8 L8:0x0026d470 L12:0x0027bbb0 L14:0x0026b098

}  // namespace rac1

#endif
