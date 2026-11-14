// Aridia's own moby classes, part 2: the update functions (and the camera type functions) transliterated from Aridia's
// own overlay, `// orig: L2:0x00xxxxxx`. Registered in aridia_mobys.cpp (the camera type through camera_type_call; the
// sound emitter types 3 / 6 are sound_emitter_types.cpp's, dispatched by sound_emitters_update).
#ifndef RAC1_GAME_OVL_ARIDIA_CLASSES_TR_H
#define RAC1_GAME_OVL_ARIDIA_CLASSES_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>  // (TrRet)

namespace rac1 {

void sand_shark_update_l2(uint32_t a0);  // L2:0x002d3e50
void constructorbot_update_l2(uint32_t a0);  // L2:0x002d7748
void nest_update_l2(uint32_t a0);  // L2:0x002dcb38
void transport_pod_update_l2(uint32_t a0);  // L2:0x002ddc88
void rocket_launcher_update_l2(uint32_t a0);  // L2:0x002df3d8
void skid_update_l2(uint32_t a0);  // L2:0x002e1950
void skid_agent_update_l2(uint32_t a0);  // L2:0x002e0dc0
void liftcar_platform_update_l2(uint32_t a0);  // L2:0x002e2228
void elevator_update_l2(uint32_t a0);  // L2:0x002dbd38
void class1324_update_l2(uint32_t a0);  // L2:0x002ee890
void class1479_update_l2(uint32_t a0);  // L2:0x002ef020
void rocks_update_l2(uint32_t a0);  // L2:0x002e0720
uint32_t shark_hook_14_l2(uint32_t a0);  // L2:0x002d6340
uint32_t shark_hook_10_l2(uint32_t a0);  // L2:0x002d6310
// class 1208's spawner (the Rocket Launcher's rocket: the launcher, a position, a direction): the oracle's generator uses it
TrRet lv2_2ec228_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x002ec228
void shark_egg_update_l2(uint32_t a0);    // L2:0x002e0280 (class 755, nothing on Aridia spawns it)

}  // namespace rac1

#endif
