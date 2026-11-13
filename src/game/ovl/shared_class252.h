// Class 252's two draw callbacks (Batalia and Oltanis), shared: one port of the two levels' copies
// (shared_class252.cpp). Each function's `orig:` line lists both levels' addresses. Its hook +0xc is class252_hook_c
// (shared_helpers.h).
#ifndef RAC1_GAME_OVL_SHARED_CLASS252_H
#define RAC1_GAME_OVL_SHARED_CLASS252_H

#include <cstdint>

namespace rac1 {

// The beam from the moby's joint 2 to the moby its pvars +0x218 name: six textured quads.
void class252_draw_beam(uint32_t m);  // L8:0x002d3878 L14:0x002da678
// The glow at its pvars' point +0x220, facing the camera.
void class252_draw_glow(uint32_t m);  // L8:0x002d4108 L14:0x002daf08

}  // namespace rac1

#endif
