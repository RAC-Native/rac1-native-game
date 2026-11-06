// The moby drop shadows' VU0 routines written by hand (moby_shadows_hand.cpp) and their switch (moby_shadows_fast.cpp).
// orig: (host)
#ifndef RAC1_GAME_OVL_MOBY_SHADOWS_HAND_H
#define RAC1_GAME_OVL_MOBY_SHADOWS_HAND_H

#include <cstdint>

namespace rac1::shadow_hand {

// orig: (host) the rewritten routines (true: done; false: not usable now, the caller runs the transliteration)
bool points();
bool points_clip(uint32_t a0, uint32_t a1);
bool point_draw(uint32_t e);
bool segment_draw(uint32_t e);
bool packets(uint32_t layer, uint32_t rgba);
bool quads(uint32_t pts, uint32_t count, uint32_t clipped);

}  // namespace rac1::shadow_hand

#endif
