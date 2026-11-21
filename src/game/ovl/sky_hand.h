// The sky's VU0 routines written by hand (sky_hand.cpp) and their switch (sky_fast.cpp).
// orig: (host)
#ifndef RAC1_GAME_OVL_SKY_HAND_H
#define RAC1_GAME_OVL_SKY_HAND_H

#include <cstdint>

namespace rac1::sky_hand {

// orig: (host) the hand-written sky VU0 routines (drain any running VU0 microprogram first)
bool project(uint32_t v, uint32_t out, uint32_t n, uint32_t& ret);  // 0x28c14c
bool spheres(uint32_t s, uint32_t n, uint32_t out);                // 0x28c680 (EE RAM output)
void spheres_host(uint32_t s, uint32_t n, uint8_t* out);           // 0x28c680 (host buffer output)
bool sprites(uint32_t& ret);                                       // 0x28bd58
uint32_t colour_triangles(uint32_t n, uint32_t tri, uint32_t colours, uint32_t verts);  // 0x28c298
bool triangles(uint32_t n, uint32_t tri, uint32_t uvs, uint32_t verts, uint32_t& ret);  // 0x28c3c0

}  // namespace rac1::sky_hand

#endif
