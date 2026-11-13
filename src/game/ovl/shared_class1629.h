// Class 1629 of Batalia and Oltanis: a shower of class 1400's drops about a place while the camera is near it
// (shared_class1629.cpp: one port of both levels' copies, the same code relinked). Phase 1 of stage 4: the levels' generated
// copies still run; this is proved against them by oracle batch 520.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1629_H
#define RAC1_GAME_OVL_SHARED_CLASS1629_H

#include <cstdint>

namespace rac1 {

// The update: with the camera within its reach (+4 of its pvars, flat), its count (+0x14) of drops (kind +0) at random in a
// ring about it (radii +0xc .. +8, angles +0x28 .. +0x2c degrees from the back), each one beyond +0x10 from the camera,
// blown by the level's wind and a random part of it (+0x24 a second), falling at +0x18 .. +0x1c a second to +0x20 below it.
void class1629_update(uint32_t m);  // L8:0x003085f0 L14:0x003088b0

}  // namespace rac1

#endif
