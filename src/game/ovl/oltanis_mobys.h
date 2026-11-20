// Oltanis (level 14) transliterated moby classes, camera types, class hooks, draw callbacks, the camera mode hook, the cutscene extras and level-only engine functions: the declarations of
// oltanis_classes_tr_N.cpp (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI).
#ifndef RAC1_GAME_OVL_OLTANIS_MOBYS_H
#define RAC1_GAME_OVL_OLTANIS_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// Oltanis's own classes written by hand: the lift (oltanis_lift.cpp), the searchlight drones (oltanis_searchlight.cpp),
// the pop-up turrets (oltanis_popup_turret.cpp) and the bombers (oltanis_bomber.cpp) ...
void olt_class903_update_l14(uint32_t m);  // L14:0x002aba80
void olt_class8_update_l14(uint32_t m);    // L14:0x002ac618
void olt_class30_update_l14(uint32_t m);   // L14:0x002b3bf0
void olt_class31_update_l14(uint32_t m);   // L14:0x002b46e8
// ... the bombers' wings (oltanis_wing.cpp), the rail drone (oltanis_class211.cpp), the tilting platform
// (oltanis_tilt_platform.cpp), the electric arc (oltanis_arc_fence.cpp), the flying craft and its platform
// (oltanis_jet_platform.cpp), the wind (oltanis_wind.cpp), the floating mine (oltanis_floating_mine.cpp), the turrets' shot
// (oltanis_turret_shot.cpp), the arrival scene and its lightning (oltanis_lightning.cpp), the gondola (oltanis_gondola.cpp).
void olt_class81_update_l14(uint32_t m);   // L14:0x002bac78
void olt_class211_update_l14(uint32_t m);  // L14:0x002d5f40
void olt_class309_update_l14(uint32_t m);  // L14:0x002de1f8
void olt_class386_update_l14(uint32_t m);  // L14:0x002dee28
void olt_class557_update_l14(uint32_t m);  // L14:0x002e7bd8
void olt_class610_update_l14(uint32_t m);  // L14:0x002eaf88
void olt_class643_update_l14(uint32_t m);  // L14:0x002ec810
void olt_class681_update_l14(uint32_t m);  // L14:0x002ece00
void olt_class684_update_l14(uint32_t m);  // L14:0x002ed280
void olt_class685_update_l14(uint32_t m);  // L14:0x002ee8d8

TrRet olt_2f1140_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002f1140
TrRet olt_2f13f8_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002f13f8
void olt_class712_update_l14(uint32_t a0);  // L14:0x002f0538
TrRet olt_26b390_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x0026b390
TrRet olt_2ef578_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002ef578
TrRet olt_2ef5a8_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002ef5a8
void olt_class851_update_l14(uint32_t a0);  // L14:0x002fba20
TrRet olt_2fc3e8_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002fc3e8
TrRet olt_2fc9a0_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002fc9a0
void olt_class908_update_l14(uint32_t a0);  // L14:0x002fc0f0
TrRet olt_2fd310_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002fd310
TrRet olt_2fd918_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002fd918
TrRet olt_2fe238_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002fe238
void olt_class921_update_l14(uint32_t a0);  // L14:0x002fcbb0
TrRet olt_2fded0_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002fded0
void olt_class922_update_l14(uint32_t a0);  // L14:0x002fdbb8
TrRet olt_2fea80_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002fea80
void olt_class923_update_l14(uint32_t a0);  // L14:0x002fe2a0
void olt_class924_update_l14(uint32_t a0);  // L14:0x002fefe0
void olt_class1193_update_l14(uint32_t a0);  // L14:0x00300e00
TrRet olt_305600_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00305600
TrRet olt_305680_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00305680
void olt_class1352_update_l14(uint32_t a0);  // L14:0x00305408
TrRet olt_305a38_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00305a38
TrRet olt_305b18_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00305b18
void olt_class1354_update_l14(uint32_t a0);  // L14:0x00305758
TrRet olt_306158_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00306158
void olt_class1395_update_l14(uint32_t a0);  // L14:0x00305d28
void olt_class1397_update_l14(uint32_t a0);  // L14:0x003061d8
void olt_class1416_update_l14(uint32_t a0);  // L14:0x00306b78
TrRet olt_307510_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00307510
TrRet olt_307620_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00307620
TrRet olt_3135b8_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x003135b8
void olt_class1417_update_l14(uint32_t a0);  // L14:0x00306ee0
void olt_class1418_update_l14(uint32_t a0);  // L14:0x00307a80
void olt_class1559_update_l14(uint32_t a0);  // L14:0x003087c0
TrRet olt_213028_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00213028
TrRet olt_20f9d0_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x0020f9d0
TrRet olt_2ae0a8_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002ae0a8
TrRet olt_2ada18_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002ada18
TrRet olt_2add48_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002add48
TrRet olt_2aef28_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002aef28
void olt_cb_2aee28_l14(uint32_t a0);  // L14:0x002aee28
void olt_cb_2d71f0_l14(uint32_t a0);  // L14:0x002d71f0
TrRet olt_2df510_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002df510
void olt_cb_2df458_l14(uint32_t a0);  // L14:0x002df458
void olt_cb_2eb810_l14(uint32_t a0);  // L14:0x002eb810
void olt_cb_2edc18_l14(uint32_t a0);  // L14:0x002edc18
TrRet olt_255ce0_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00255ce0
TrRet olt_2aac78_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002aac78
TrRet olt_2ab378_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002ab378
void olt_cb_303350_l14(uint32_t a0);  // L14:0x00303350
void olt_cb_2ae260_l14(uint32_t a0);  // L14:0x002ae260
TrRet olt_cam_type20_accept_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00315920
void olt_cam_type20_init_l14(uint32_t a0);  // L14:0x00314f00
TrRet olt_2d6570_l14(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002d6570
void olt_cam_type20_update_l14(uint32_t a0);  // L14:0x00315290
TrRet olt_cam_type21_accept_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x00316748
void olt_cam_type21_init_l14(uint32_t a0);  // L14:0x003159d8
void olt_cam_type21_update_l14(uint32_t a0);  // L14:0x00315d48
void olt_cam_type21_exit_l14(uint32_t a0);  // L14:0x003167e0
TrRet olt_hook_2daed8_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L14:0x002daed8
void olt_cutscene_level_extras_l14(uint32_t a0);  // L14:0x002460f8

}  // namespace rac1

#endif
