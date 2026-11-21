// Class 156, a shot that flies straight, faced to the camera, trailing sparks, and bursts on what it meets: one port of the
// copies Novalis, Blarg, Batalia, Gaspar, Quartu and Veldin's orbit carry (shared_class156.cpp), with its burst and its trail's
// particle. Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by oracle batch 514.
#ifndef RAC1_GAME_OVL_SHARED_CLASS156_H
#define RAC1_GAME_OVL_SHARED_CLASS156_H

#include <cstdint>

namespace rac1 {

void class156_update(uint32_t m);  // L1:0x002c7530 L6:0x002bead0 L8:0x002ba8d0 L9:0x002d0d88 L15:0x002aabb8 L17:0x002af608
// The burst at the collision's hit point: sparks (particle 0x13) and smoke (particle 0x15) thrown about.
void class156_burst(uint32_t m);  // L1:0x002c72c8 L6:0x002be868 L8:0x002ba668 L9:0x002d0b20 L15:0x002aa950 L17:0x002af3a0
// The trail's particle (0xe) at the moby `m`, turning by `spin` either way (0: none). The particle (0: none).
uint32_t class156_trail(uint32_t spin, uint32_t m);  // L1:0x00280970 L6:0x0027a850 L8:0x00275bd8 L9:0x0028a428 L15:0x0025b050 L17:0x002653f8

}  // namespace rac1

#endif
