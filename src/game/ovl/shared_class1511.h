// Class 1511's draw callback and update (Rilgar and Oltanis), shared: one port of the two levels' copies
// (shared_class1511.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1511_H
#define RAC1_GAME_OVL_SHARED_CLASS1511_H

#include <cstdint>

namespace rac1 {

// A flickering glow facing the camera, along the moby's y axis from its place.
void class1511_draw(uint32_t m);  // L5:0x0031cf58 L14:0x00308150
// Its update (task F11): the glow's alpha by the camera's distance and its draw; hit (a blast, or the wrench at its glow), it
// breaks: its remains, two pieces, the bolts, sparks and smoke, gone.
void class1511_update(uint32_t m);  // L5:0x0031c8e0 L14:0x00307ad8
// Its remains: a moby of class 1514 in its place, turned and coloured as it is; the moby or 0.
uint32_t class1511_remains(uint32_t m);  // L5:0x0031d160 L14:0x00308358

}  // namespace rac1

#endif
