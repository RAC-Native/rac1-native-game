// The camera of Blarg's class 1051 (the hydro pumps' switch) while its pump runs: its start and its follow, each a function
// the level carries several copies of (shared_pump_camera.cpp: one port of the copies, the same code). Phase 1 of stage 4:
// the level's generated copies still run; these are proved against them by oracle batch 519.
#ifndef RAC1_GAME_OVL_SHARED_PUMP_CAMERA_H
#define RAC1_GAME_OVL_SHARED_PUMP_CAMERA_H

#include <cstdint>

namespace rac1 {

// The scene's start: the hero put at the switch's spot (pvars +0xf8), a scripted camera behind the switch looking at it, the
// blend +0x118 at 0, the pump's path (+0xec) at its start.
void pump_camera_start(uint32_t m);  // L6:0x002fb148 L6:0x002fb508 L6:0x002fb8c8
// The scene's camera: from behind the switch to behind the hero as the blend (+0x118) runs to 1 (the camera itself while the
// switch's animation is 6); 1 once the blend is full and the animation is 1, else 0.
uint32_t pump_camera_follow(uint32_t m);  // L6:0x002fb668 L6:0x002fba28

}  // namespace rac1

#endif
