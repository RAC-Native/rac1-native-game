// Class 1400, the weather about the camera (Batalia, Hoven, Oltanis): drops (or, on Hoven, motes) spawned about the camera
// each frame, drifting with a wind dealt anew now and then, and splashes on the ground. One port of the three levels'
// copies of its update, its wind's set-up and its two particles (shared_class1400.cpp). Phase 1 of stage 4: the levels'
// generated copies still run; these are proved against them by oracle batch 517.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1400_H
#define RAC1_GAME_OVL_SHARED_CLASS1400_H

#include <cstdint>

namespace rac1 {

void class1400_update(uint32_t m);  // L8:0x00307cf0 L12:0x00308be0 L14:0x00306390
// The wind dealt anew (Hoven's when `motes`): its two speeds, its timer and the drops' count, in the level's record.
void class1400_wind(uint32_t motes);  // L8:0x00307b90 L12:0x00308a80 L14:0x00306230
// A drop (particle 0) at `pos` falling at `vel` (EE vectors) to the height `floor`: `kind` 0 a drop, 1 a heavier one,
// 2 a streak.
void class1400_drop(uint32_t pos, int32_t kind, uint32_t vel, float floor);  // L8:0x00272cb8 L12:0x00281420 L14:0x002706b0
// A mote (particle 0x49) at `pos` drifting at `vel` to the height `floor`; `kind` 0 or 1.
void class1400_mote(uint32_t pos, int32_t kind, uint32_t vel, float floor);  // L8:0x0027f5f0 L12:0x0028db38 L14:0x0027cfb0

}  // namespace rac1

#endif
