// Aridia's own moby classes, part 2: the update functions (and the camera type functions) transliterated from Aridia's
// own overlay, `// orig: L2:0x00xxxxxx`. Registered in aridia_mobys.cpp (the camera type through camera_type_call; the
// sound emitter types 3 / 6 are sound_emitter_types.cpp's, dispatched by sound_emitters_update).
#ifndef RAC1_GAME_OVL_ARIDIA_CLASSES_TR_H
#define RAC1_GAME_OVL_ARIDIA_CLASSES_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>  // (TrRet)

namespace rac1 {

TrRet lv2_2e7660_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x002e7660
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
void swingshot_target_update_l2(uint32_t a0);  // L2:0x002e0450
void trespasser_pad_update_l2(uint32_t a0);  // L2:0x002d8ad0
void trespasser_puzzle_draw_l2(uint32_t a0);  // L2:0x002d93e8 (the pad's draw-list callback, the puzzle)
void class832_draw_l2(uint32_t a0);  // L2:0x002e4838 (class 832's draw-list callback: the Visibomb's out-of-range warning)
void sandmouse_glow_l2(uint32_t a0);  // L2:0x002f0eb0 (the Sandmouse's (1818) draw-list callback, its two glows)
void rocks_update_l2(uint32_t a0);  // L2:0x002e0720
void bg_ship1_update_l2(uint32_t a0);  // L2:0x002ec4b8
void bg_ship2_update_l2(uint32_t a0);  // L2:0x002ece18
void gold_bolt_update_l2(uint32_t a0);  // L2:0x002ead38
void sandmouse_update_l2(uint32_t a0);  // L2:0x002f0f88
void class604_update_l2(uint32_t a0);  // L2:0x002d7190
void class832_update_l2(uint32_t a0);  // L2:0x002e4a48
uint32_t cam_type_12_accept_l2(uint32_t a0, uint32_t a1);  // L2:0x002fc298
uint32_t shark_hook_0_l2(uint32_t a0);  // L2:0x002d61c0
uint32_t shark_hook_4_l2(uint32_t a0);  // L2:0x002d6210
uint32_t shark_hook_8_l2(uint32_t a0);  // L2:0x002d6290
uint32_t shark_hook_c_l2(uint32_t a0);  // L2:0x002d62e0
uint32_t shark_hook_14_l2(uint32_t a0);  // L2:0x002d6340
uint32_t shark_hook_10_l2(uint32_t a0);  // L2:0x002d6310
void class737_update_l2(uint32_t a0);  // L2:0x002dfcd0
void cam_mode_hook_2fa710_l2(uint32_t a0);  // L2:0x002fa710
void cam_callback_2fa770_l2(uint32_t a0);  // L2:0x002fa770 (the end-of-update callback the camera hook 0x2fa710 queues)
// the sparks (Veldin's unported 0x26ced0; the Rocks spawn them): proved against the original
// class 1633's spawner (a parent, a position, f12..f15): the oracle's generator spawns one with it
TrRet lv2_2efdd0_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x002efdd0
// class 1208's spawner (the Rocket Launcher's rocket: the launcher, a position, a direction): the oracle's generator uses it
TrRet lv2_2ec228_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x002ec228
// the Rocks' piece spawner (a position, a velocity, the class 696..698, a life, f12..f15): the oracle's generator uses it
TrRet lv2_2dd4d8_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x002dd4d8
void sand_mouse_update_l2(uint32_t a0);   // L2:0x002ccc08 (class 437, nothing on Aridia spawns it)
void rock_update_l2(uint32_t a0);         // L2:0x002dd6c0 (classes 696..698, the Rocks' pieces, L2:0x2dd4d8)
void shark_egg_update_l2(uint32_t a0);    // L2:0x002e0280 (class 755, nothing on Aridia spawns it)
void trespasser_gadget_update_l2(uint32_t a0);  // L2:0x002ea210 (class 1005)
void class1633_update_l2(uint32_t a0);  // L2:0x002efee0 (class 1633, spawned by L2:0x2efdd0)

}  // namespace rac1

#endif
