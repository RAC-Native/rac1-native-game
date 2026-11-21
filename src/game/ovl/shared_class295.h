// Class 295, a rocket of the space levels (Gemlik Base, Drek's fleet), and class 1034, another build of it (Pokitaru too): it flies on, homing on its target while it keeps
// near the point it last saw it at, trails smoke and sparks, and blows up where it meets something (shared_class295.cpp).
// One port of the levels' copies of the two updates. Phase 1 of stage 4: the levels' generated copies still run; this is
// proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_CLASS295_H
#define RAC1_GAME_OVL_SHARED_CLASS295_H

#include <cstdint>

namespace rac1 {

void class295_update(uint32_t m);   // L13:0x002e6c08 L17:0x002c9ce8
// Class 1034, Pokitaru's, Gemlik Base's and Drek's fleet's other build of the rocket.
void class1034_update(uint32_t m);  // L11:0x003094f8 L13:0x00303128 L17:0x002e30c8
// A rocket (class 295) of `owner` launched from `pos` turned as `rot` (an EE vector: +4 the pitch, +8 the yaw) at `speed`,
// homing on `target` (0: none; its class noted, and its pvar word 0's flag 0x80 set), flying `frames`, with its puff; its
// scale the owner's part of its class's. The rocket, or 0.
uint32_t class295_launch(uint32_t owner, uint32_t pos, uint32_t target, uint32_t rot, int32_t frames, float speed);  // L13:0x002e6a58 L17:0x002c9b38

}  // namespace rac1

#endif
