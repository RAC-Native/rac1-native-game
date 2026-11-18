// Pokitaru (level 11) transliterated moby classes, class hooks, draw callbacks, the camera mode hook and level-only engine functions: the declarations of
// pokitaru_classes_tr_N.cpp (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI).
#ifndef RAC1_GAME_OVL_POKITARU_MOBYS_H
#define RAC1_GAME_OVL_POKITARU_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// ---- hand-written (pokitaru_props.cpp, pokitaru_talkers.cpp, pokitaru_resort_owner.cpp, pokitaru_teleporter.cpp,
// pokitaru_raft.cpp, pokitaru_gate.cpp, pokitaru_sewer.cpp, pokitaru_crates.cpp, pokitaru_jet.cpp; their helpers in pokitaru_own.h)
void pok_class23_update_l11(uint32_t m);    // L11:0x002cb668 a pickup that ends the resort owner's errand
void pok_class65_update_l11(uint32_t m);    // L11:0x002cb810 a gate and its twin
void pok_class90_update_l11(uint32_t m);    // L11:0x002d0710 Bob (the Thruster-Pack)
void pok_class114_update_l11(uint32_t m);   // L11:0x002d0fa8 the resort owner (the O2 Mask)
void pok_class298_update_l11(uint32_t m);   // L11:0x002f0e40 a talker on the beach (a skill point's clock)
void pok_class318_update_l11(uint32_t m);   // L11:0x002f2518 the teleporter
void pok_class361_update_l11(uint32_t m);   // L11:0x002f3350 a height trigger
void pok_class1075_update_l11(uint32_t m);  // L11:0x00309ac0 the raft
void pok_class1156_update_l11(uint32_t m);  // L11:0x0030c788 the great gate
void pok_class1157_update_l11(uint32_t m);  // L11:0x0030d800 the sewer iris gate
void pok_class1158_update_l11(uint32_t m);  // L11:0x0030e088 the sewer's rising water
void pok_class1159_update_l11(uint32_t m);  // L11:0x0030e978 a sliding plug and its twin
void pok_class1178_update_l11(uint32_t m);  // L11:0x0030eb90 a tilting platform
void pok_class1180_update_l11(uint32_t m);  // L11:0x0030f2a8 a swinging valve
void pok_class1218_update_l11(uint32_t m);  // L11:0x0030f728 a floating crate drop
void pok_class1219_update_l11(uint32_t m);  // L11:0x00310028 a crate's parachute
void pok_class1231_update_l11(uint32_t m);  // L11:0x00310180 the energy-ball thrower
void pok_class1242_update_l11(uint32_t m);  // L11:0x00313290 the fighter jet

void pok_class1111_update_l11(uint32_t a0);  // L11:0x0030b358 (class 1111, the open sea: kept generated, the PAL 2.00 build changed it beyond fix 24)
TrRet pok_315968_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00315968
TrRet pok_315f58_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00315f58
TrRet pok_316090_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00316090
void pok_class1246_update_l11(uint32_t a0);  // L11:0x00314318
void pok_class1248_update_l11(uint32_t a0);  // L11:0x00316320
TrRet pok_317678_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00317678
TrRet pok_317820_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00317820
TrRet pok_318488_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00318488
TrRet pok_317c98_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00317c98
TrRet pok_3180a0_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x003180a0
void pok_class1264_update_l11(uint32_t a0);  // L11:0x003172c0
TrRet pok_31ada0_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0031ada0
void pok_class1265_update_l11(uint32_t a0);  // L11:0x003181f0
TrRet pok_319028_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00319028
TrRet pok_3191c0_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x003191c0
void pok_class1297_update_l11(uint32_t a0);  // L11:0x00318b30
TrRet pok_30f5a8_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0030f5a8
void pok_class1319_update_l11(uint32_t a0);  // L11:0x00319838
void pok_class1350_update_l11(uint32_t a0);  // L11:0x0031aa80
TrRet pok_31ae68_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0031ae68
TrRet pok_31b098_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0031b098
void pok_class1524_update_l11(uint32_t a0);  // L11:0x0031ab08
TrRet pok_31e048_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0031e048
void pok_class1903_update_l11(uint32_t a0);  // L11:0x0031e2f0
TrRet pok_31eea8_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0031eea8
void pok_class1919_update_l11(uint32_t a0);  // L11:0x0031f150
TrRet pok_hook_2fb7f0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb7f0
TrRet pok_hook_2fb840_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb840
TrRet pok_hook_2fb890_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb890
TrRet pok_hook_2fb8c0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb8c0
TrRet pok_hook_316160_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00316160
TrRet pok_hook_3162d0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x003162d0
TrRet pok_hook_3162f0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x003162f0
void pok_cb_2f2d58_l11(uint32_t a0);  // L11:0x002f2d58
TrRet pok_231380_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00231380
TrRet pok_22e0a8_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0022e0a8
void pok_cb_30a908_l11(uint32_t a0);  // L11:0x0030a908 (class 1111's draw-list callback: kept generated with its update)
TrRet pok_311710_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00311710
TrRet pok_311a88_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00311a88
TrRet pok_311be8_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00311be8
TrRet pok_318050_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00318050
TrRet pok_311948_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00311948
TrRet pok_311c80_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00311c80
void pok_cb_311d50_l11(uint32_t a0);  // L11:0x00311d50
TrRet pok_31d8d8_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0031d8d8
void pok_cb_31db50_l11(uint32_t a0);  // L11:0x0031db50
TrRet pok_31e6a8_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0031e6a8
void pok_cb_31e930_l11(uint32_t a0);  // L11:0x0031e930

}  // namespace rac1

#endif
