// The sky's VU0 routines written by hand (sky_hand.cpp) and their switch (sky_fast.cpp).
// orig: (host)
#ifndef RAC1_GAME_OVL_SKY_HAND_H
#define RAC1_GAME_OVL_SKY_HAND_H

#include <cstdint>

namespace rac1::sky_hand {

// orig: (host) the rewritten routines (true: done; false: not usable now, the caller runs the transliteration)
bool project(uint32_t v, uint32_t out, uint32_t n, uint32_t& ret);  // 0x28c14c
bool spheres(uint32_t s, uint32_t n, uint32_t out);                // 0x28c680
bool sprites(uint32_t& ret);                                       // 0x28bd58
uint32_t colour_triangles(uint32_t n, uint32_t tri, uint32_t colours, uint32_t verts);  // 0x28c298 (no VU0 op: always runs)
bool triangles(uint32_t n, uint32_t tri, uint32_t uvs, uint32_t verts, uint32_t& ret);  // 0x28c3c0

}  // namespace rac1::sky_hand

#endif
