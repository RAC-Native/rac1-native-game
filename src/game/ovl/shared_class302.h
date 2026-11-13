// Classes 302 and 303 of Blarg and Orxon (an environment-mapped mesh and a glowing light), shared by the two levels that
// carry them: one port of the levels' copies (shared_class302.cpp). Each definition's `orig:` line lists every level's
// address.
#ifndef RAC1_GAME_OVL_SHARED_CLASS302_H
#define RAC1_GAME_OVL_SHARED_CLASS302_H

#include <cstdint>

namespace rac1 {

// Class 302's draw callback: the level's mesh (57 points, 42 quads) at the moby, sphere-mapped from the camera when near.
void class302_draw(uint32_t m);  // L6:0x002d8738 L10:0x002bd280
// Class 303's update: its colour, pulsing while the moby it watches (+0xb8) is in state 0 or 1, else fading to grey.
void class303_update(uint32_t m);  // L6:0x002d8c20 L10:0x002bd768

}  // namespace rac1

#endif
