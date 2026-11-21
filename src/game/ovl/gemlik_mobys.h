// Gemlik Base (level 13) transliterated moby classes, camera type, class hooks, draw callbacks, HUD element, the camera mode hook and level-only engine functions: the declarations of
// gemlik_classes_tr_N.cpp (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI).
#ifndef RAC1_GAME_OVL_GEMLIK_MOBYS_H
#define RAC1_GAME_OVL_GEMLIK_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// ---- hand-written (gemlik_trigger_box.cpp, gemlik_lift.cpp, gemlik_turret.cpp, gemlik_ship.cpp, gemlik_boss_shots.cpp,
// gemlik_drone.cpp, gemlik_swing.cpp, gemlik_tower.cpp, gemlik_asteroid.cpp, gemlik_target_group.cpp)
void gml_class6_update_l13(uint32_t m);  // L13:0x002b0e80 (class 6, the trigger box)
void gml_class21_update_l13(uint32_t m);  // L13:0x002b39e0 (class 21, the lift)
void gml_2b4060_l13(uint32_t m, uint32_t pos, uint32_t rot);  // L13:0x002b4060 (the lift's ride camera)
void gml_class29_update_l13(uint32_t m);  // L13:0x002b41b8 (class 29, the wall turret's base)
void gml_class36_update_l13(uint32_t m);  // L13:0x002b4c80 (class 36, its top)
uint32_t gml_3061c8_l13(uint32_t pos, uint32_t vel, uint32_t owner, float range);  // L13:0x003061c8 (class 1238's shot spawned)
void gml_2b47a0_l13(uint32_t m);  // L13:0x002b47a0 (the turret fires)
void gml_2b48d8_l13(uint32_t m);  // L13:0x002b48d8 (the turret's target)
uint32_t gml_2b4958_l13(uint32_t m);  // L13:0x002b4958 (the turret's top spawned; fix 38)
void gml_2b4a58_l13(uint32_t m, uint32_t pv);  // L13:0x002b4a58 (the top's hits)
void gml_class69_update_l13(uint32_t m);  // L13:0x002bb068 (class 69, the ship; gemlik_ship.cpp)
void gml_2ba178_l13(uint32_t m, uint32_t pv);  // L13:0x002ba178 (its flight)
void gml_2b9df8_l13(uint32_t m, uint32_t pv, float yaw, float pitch);  // L13:0x002b9df8 (its guns)
void gml_2b9f58_l13(uint32_t m, uint32_t pv);  // L13:0x002b9f58 (its missiles)
void gml_2b8590_l13(uint32_t m, uint32_t pv);  // L13:0x002b8590 (its engines' trails)
void gml_265170_l13(int32_t cls, uint32_t state);  // L13:0x00265170 (a class's mobys given a state)
void gml_2651c8_l13(uint32_t list, uint32_t hidden, uint32_t solid, uint32_t glow);  // L13:0x002651c8 (listed classes' flags)
void gml_class82_update_l13(uint32_t m);  // L13:0x002c1528 (class 82, Qwark's homing missile; gemlik_boss_shots.cpp)
void gml_class83_update_l13(uint32_t m);  // L13:0x002c2048 (class 83, Qwark's drifting mine)
void gml_class101_update_l13(uint32_t m);  // L13:0x002c25b8 (class 101, the gun drone; gemlik_drone.cpp)
void gml_2c2d30_l13(uint32_t m);  // L13:0x002c2d30 (its every frame: hum, bobbing, sparks, hits, target)
uint32_t gml_3058a8_l13(uint32_t m, int32_t joint);  // L13:0x003058a8 (a shot, class 1233, held at a chain point)
void gml_305988_l13(uint32_t s, uint32_t vel, uint32_t target, float range);  // L13:0x00305988 (the held shot fired)
void gml_class127_update_l13(uint32_t m);  // L13:0x002c7f38 (class 127, a part swung by a switch; gemlik_swing.cpp)
void gml_class170_update_l13(uint32_t m);  // L13:0x002cefc0 (class 170, a tower that breaks; gemlik_tower.cpp; fix 43)
void gml_2982e8_l13(int32_t slot, uint32_t pos);  // L13:0x002982e8 (a sound slot given a position to follow)
void gml_2ce688_l13(uint32_t m, uint32_t pos, uint32_t flash);  // L13:0x002ce688 (the tower's debris)
void gml_2cecb8_l13(uint32_t m, uint32_t pv, uint32_t hs);  // L13:0x002cecb8 (the tower's hits)
uint32_t gml_30bfb0_l13(uint32_t parent, uint32_t pos, float scale);  // L13:0x0030bfb0 (its hit proxy, class 1632; fix 43)
void gml_class212_update_l13(uint32_t m);  // L13:0x002e1638 (class 212, an asteroid; gemlik_asteroid.cpp)
uint32_t gml_2e1140_l13(uint32_t parent, uint32_t pos, uint32_t vel, int32_t gen, float scale);  // L13:0x002e1140 (a piece)
void gml_2e1348_l13(uint32_t m, uint32_t pv, float k);  // L13:0x002e1348 (the asteroid breaks)
void gml_class231_update_l13(uint32_t m);  // L13:0x002e4448 (class 231, a target of a group; gemlik_target_group.cpp)
bool gml_2e4300_l13(uint32_t m, uint32_t pv, uint32_t hs);  // L13:0x002e4300 (its hits)

// ---- generated (gemlik_classes_tr_N.cpp)
TrRet gml_280400_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x00280400
TrRet gml_2b4f28_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b4f28
TrRet gml_2b4fb8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b4fb8
TrRet gml_2b4ff0_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b4ff0
void gml_class63_update_l13(uint32_t a0);  // L13:0x002b50d8
TrRet gml_2c37e0_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c37e0
TrRet gml_2c3ac8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c3ac8
TrRet gml_2c3988_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c3988
TrRet gml_2c3c88_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c3c88
TrRet gml_2c3f08_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c3f08
TrRet gml_2c3fc0_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c3fc0
void gml_class111_update_l13(uint32_t a0);  // L13:0x002c4428
TrRet gml_250748_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x00250748
TrRet gml_250840_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x00250840
TrRet gml_2a5390_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002a5390
void gml_class304_update_l13(uint32_t a0);  // L13:0x002e7238
void gml_class352_update_l13(uint32_t a0);  // L13:0x002e84d8
TrRet gml_2e8438_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e8438
TrRet gml_2e86f8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e86f8
TrRet gml_2e8920_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e8920
TrRet gml_2e8b58_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e8b58
TrRet gml_2ecd10_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002ecd10
TrRet gml_2e8d28_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e8d28
TrRet gml_2e9018_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e9018
TrRet gml_2e9160_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e9160
TrRet gml_2e9408_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e9408
TrRet gml_2e9680_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e9680
TrRet gml_2e97e0_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e97e0
TrRet gml_2e9a48_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002e9a48
TrRet gml_2c13b0_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c13b0
TrRet gml_2ea330_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002ea330
TrRet gml_2ea400_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002ea400
TrRet gml_2ea540_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002ea540
TrRet gml_2c1f28_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002c1f28
TrRet gml_2ea6b8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002ea6b8
TrRet gml_2eac00_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002eac00
void gml_class388_update_l13(uint32_t a0);  // L13:0x002eb098
void gml_class389_update_l13(uint32_t a0);  // L13:0x002ece00
TrRet gml_2ed158_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002ed158
void gml_class404_update_l13(uint32_t a0);  // L13:0x002ed4a8
void gml_class533_update_l13(uint32_t a0);  // L13:0x002f3750
void gml_class558_update_l13(uint32_t a0);  // L13:0x002f3778
void gml_class667_update_l13(uint32_t a0);  // L13:0x002f8880
void gml_class674_update_l13(uint32_t a0);  // L13:0x002f8a78
TrRet gml_3060b8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x003060b8
TrRet gml_306148_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x00306148
void gml_class1233_update_l13(uint32_t a0);  // L13:0x00305a58
void gml_class1238_update_l13(uint32_t a0);  // L13:0x00306300
TrRet gml_30c138_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x0030c138
TrRet gml_3073c8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x003073c8
void gml_class1261_update_l13(uint32_t a0);  // L13:0x00307b10
TrRet gml_3094f8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x003094f8
TrRet gml_308d10_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x00308d10
TrRet gml_309370_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x00309370
void gml_class1262_update_l13(uint32_t a0);  // L13:0x00307e98
TrRet gml_2aeba8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002aeba8
void gml_class1263_update_l13(uint32_t a0);  // L13:0x00309e88
TrRet gml_30ad38_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x0030ad38
void gml_class1270_update_l13(uint32_t a0);  // L13:0x0030a6d8
TrRet gml_30ae68_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x0030ae68
void gml_class1271_update_l13(uint32_t a0);  // L13:0x0030af50
void gml_class1353_update_l13(uint32_t a0);  // L13:0x0030b628
void gml_class1403_update_l13(uint32_t a0);  // L13:0x0030bb70
void gml_class1558_update_l13(uint32_t a0);  // L13:0x0030bdf0
void gml_class1577_update_l13(uint32_t a0);  // L13:0x0030be60
void gml_class1632_update_l13(uint32_t a0);  // L13:0x0030c060
void gml_class1634_update_l13(uint32_t a0);  // L13:0x0030c3b8
void gml_class1805_update_l13(uint32_t a0);  // L13:0x0030cdb8
TrRet gml_30d3b8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x0030d3b8
void gml_class1885_update_l13(uint32_t a0);  // L13:0x0030d550
TrRet gml_2b89f8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b89f8
TrRet gml_2b8be0_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b8be0
TrRet gml_2b95b8_l13(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b95b8
void gml_cb_2b97f8_l13(uint32_t a0);  // L13:0x002b97f8
void gml_cb_2ea8c8_l13(uint32_t a0);  // L13:0x002ea8c8
TrRet gml_hook_2b7980_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b7980
TrRet gml_hook_2b79d0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b79d0
TrRet gml_hook_2b7a20_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b7a20
TrRet gml_hook_2b7ad0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b7ad0
TrRet gml_hook_2b7b00_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L13:0x002b7b00

}  // namespace rac1

#endif
