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

// ---- class 252's update and its two helpers (task F11; added after the draws)
// Class 252, a creature that walks to a goal, strikes what it carries its beam to, and can be carried: its update.
void class252_update(uint32_t m);  // L8:0x002d2af0 L14:0x002d98f0
// Its move: the speed pvars +0x40 (eased down, capped by the frame), the bob about its floor height, and the vertical speed.
void class252_move(uint32_t m);  // L8:0x002d3d78 L14:0x002dab78
// Its hits (the health pvars +0x20; dead: the bolts and state 0x63) and its target (pvars +0x170 / +0x1b0 / +0x1b4).
void class252_hit(uint32_t m);  // L8:0x002d36e0 L14:0x002da4e0

}  // namespace rac1

#endif
