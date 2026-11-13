// Small class code task F11 ported once for several levels: Umbris's and Gaspar's sea (classes 460 / 317, the same code),
// class 1139's glints (Rilgar, Kalebo III), and the puff of particle 0x34 about a moby several classes of Orxon, Hoven and
// Gemlik Base make (shared_seas.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved against
// them by oracle batch 517.
#ifndef RAC1_GAME_OVL_SHARED_SEAS_H
#define RAC1_GAME_OVL_SHARED_SEAS_H

#include <cstdint>

namespace rac1 {

// The sea (Umbris's class 460, Gaspar's 317): state 0 sets the level's wave table up (k 1) and shows it; state 1 hooks its
// draw each frame.
void sea_update(uint32_t m);  // L7:0x002f8058 L9:0x002ef7f8
// Class 1139: state 0 shows it (+0x30 0x80, flags 0x41); state 1, when on screen and the hero in the state 0x16, three
// glints (particle 0x43) at random angles.
void class1139_update(uint32_t m);  // L5:0x0031abe0 L16:0x002e0f78
// A puff of particle 0x34 about `m` from the record `rec` (+0 / +4 the particle's words, +8 / +0xc the sizes' bounds,
// +0x10 / +0x14 its life's, +0x18 the reach); the particle or 0.
uint32_t particle34_puff(uint32_t m, uint32_t rec);
// The copies: each level's record (L10:0x002e3de8 L12:0x002e3420 L13:0x003072c0), and Gemlik Base's second (L13:0x0030c2b0).
uint32_t particle34_puff_1(uint32_t m);
uint32_t particle34_puff_2(uint32_t m);

}  // namespace rac1

#endif
