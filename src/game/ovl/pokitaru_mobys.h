// Pokitaru (level 11) transliterated moby classes, class hooks, draw callbacks, the camera mode hook and level-only engine functions: the declarations of
// pokitaru_classes_tr_N.cpp (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI).
#ifndef RAC1_GAME_OVL_POKITARU_MOBYS_H
#define RAC1_GAME_OVL_POKITARU_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// ---- hand-written (pokitaru_props.cpp, pokitaru_talkers.cpp, pokitaru_resort_owner.cpp, pokitaru_teleporter.cpp,
// pokitaru_raft.cpp, pokitaru_gate.cpp, pokitaru_sewer.cpp, pokitaru_crates.cpp, pokitaru_jet.cpp,
// pokitaru_puffer.cpp, pokitaru_train.cpp, pokitaru_bomber.cpp, pokitaru_water.cpp, pokitaru_jet_hud.cpp; their helpers in pokitaru_own.h)
void pok_class23_update_l11(uint32_t m);    // L11:0x002cb668 a pickup that ends the resort owner's errand
void pok_class65_update_l11(uint32_t m);    // L11:0x002cb810 a gate and its twin
void pok_class90_update_l11(uint32_t m);    // L11:0x002d0710 Bob (the Thruster-Pack)
void pok_class114_update_l11(uint32_t m);   // L11:0x002d0fa8 the resort owner (the O2 Mask)
void pok_class298_update_l11(uint32_t m);   // L11:0x002f0e40 a talker on the beach (a skill point's clock)
void pok_class318_update_l11(uint32_t m);   // L11:0x002f2518 the teleporter
void pok_cb_2f2d58_l11(uint32_t m);         // L11:0x002f2d58 the teleporter's glow ring draw callback
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
void pok_cb_311d50_l11(uint32_t m);         // L11:0x00311d50 the fighter jet's HUD draw callback
void pok_class1242_update_l11(uint32_t m);  // L11:0x00313290 the fighter jet
void pok_class1246_update_l11(uint32_t m);  // L11:0x00314318 the pufferfish enemy
TrRet pok_hook_316160_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00316160 class 1246's carry hook +0
void pok_class1248_update_l11(uint32_t m);  // L11:0x00316320 the rising double-panel bridge
void pok_class1264_update_l11(uint32_t m);  // L11:0x003172c0 the sky cargo train
void pok_class1265_update_l11(uint32_t m);  // L11:0x003181f0 a cargo train carriage
void pok_class1297_update_l11(uint32_t m);  // L11:0x00318b30 the energy ball
void pok_class1319_update_l11(uint32_t m);  // L11:0x00319838 the Blarg bomber
void pok_class1350_update_l11(uint32_t m);  // L11:0x0031aa80 the air-combat skill-point monitor
void pok_class1524_update_l11(uint32_t m);  // L11:0x0031ab08 a falling sludge/bomb drop
void pok_cb_31db50_l11(uint32_t m);         // L11:0x0031db50 class 1903's water draw callback
void pok_class1903_update_l11(uint32_t m);  // L11:0x0031e2f0 a water-surface scroll class
void pok_cb_31e930_l11(uint32_t m);         // L11:0x0031e930 class 1919's water draw callback
void pok_class1919_update_l11(uint32_t m);  // L11:0x0031f150 a water-surface scroll class

void pok_class1111_update_l11(uint32_t a0);  // L11:0x0030b358 (class 1111, the open sea: kept generated, the PAL 2.00 build changed it beyond fix 24)
TrRet pok_hook_2fb7f0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb7f0
TrRet pok_hook_2fb840_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb840
TrRet pok_hook_2fb890_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb890
TrRet pok_hook_2fb8c0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x002fb8c0
TrRet pok_hook_3162d0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x003162d0
TrRet pok_hook_3162f0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x003162f0
TrRet pok_231380_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x00231380
TrRet pok_22e0a8_l11(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L11:0x0022e0a8
void pok_cb_30a908_l11(uint32_t a0);  // L11:0x0030a908 (class 1111's draw-list callback: kept generated with its update)

}  // namespace rac1

#endif
