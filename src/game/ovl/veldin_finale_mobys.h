// Veldin, the finale (level 18) transliterated moby classes, camera type, class hooks, draw callbacks, the camera mode hook and level-only engine functions: the declarations of
// veldin_finale_classes_tr_N.cpp (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI).
#ifndef RAC1_GAME_OVL_VELDIN_FINALE_MOBYS_H
#define RAC1_GAME_OVL_VELDIN_FINALE_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// the level's own classes hand-written (veldin_finale_shots.cpp, _walking_bomb.cpp, _rail_switch.cpp, _gate.cpp, _platforms.cpp and the files named below;
// their helpers in veldin_finale_own.h)
void vf_class41_update_l18(uint32_t a0);   // L18:0x002a7220
void vf_class564_update_l18(uint32_t a0);  // L18:0x002d5050
void vf_class568_update_l18(uint32_t a0);  // L18:0x002d5918
void vf_class582_update_l18(uint32_t a0);  // L18:0x002d62e8
void vf_class583_update_l18(uint32_t a0);  // L18:0x002d6600
void vf_class586_update_l18(uint32_t a0);  // L18:0x002d7b20
void vf_class587_update_l18(uint32_t a0);  // L18:0x002d82c0
void vf_class624_update_l18(uint32_t a0);  // L18:0x002db460
void vf_class628_update_l18(uint32_t a0);  // L18:0x002dc260
void vf_class644_update_l18(uint32_t a0);   // L18:0x002df608 (veldin_finale_camera_pan.cpp)
void vf_class885_update_l18(uint32_t a0);   // L18:0x002e9768 (veldin_finale_breakable.cpp)
void vf_class983_update_l18(uint32_t a0);   // L18:0x002e9e70 (veldin_finale_energy_ball.cpp)
void vf_class1355_update_l18(uint32_t a0);  // L18:0x002efb88 (veldin_finale_diver.cpp)
void vf_class1381_update_l18(uint32_t a0);  // L18:0x002f16f0 (veldin_finale_raft.cpp)
void vf_class1392_update_l18(uint32_t a0);  // L18:0x002f1e68 (veldin_finale_barrier.cpp)
void vf_class1402_update_l18(uint32_t a0);  // L18:0x002f2310 (veldin_finale_water.cpp)

TrRet vf_2d5348_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d5348
TrRet vf_2d5cf8_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d5cf8
TrRet vf_2d6b58_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d6b58
TrRet vf_2d6c80_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d6c80
TrRet vf_2d6d50_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d6d50
TrRet vf_2d6dc0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d6dc0
TrRet vf_28d878_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x0028d878
TrRet vf_2d7f68_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d7f68
TrRet vf_2d7fd0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d7fd0
TrRet vf_2db938_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002db938
TrRet vf_2dc3e0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002dc3e0
TrRet vf_2dc458_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002dc458
TrRet vf_2dfaa0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002dfaa0
TrRet vf_2dfb90_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002dfb90
TrRet vf_2ea0f0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002ea0f0
TrRet vf_2ea168_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002ea168
TrRet vf_2f0030_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f0030
TrRet vf_2f1d08_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f1d08
TrRet vf_2fa888_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002fa888
TrRet vf_2f1c38_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f1c38
TrRet vf_2f2b18_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f2b18
TrRet vf_2f5e18_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f5e18
TrRet vf_265ff8_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x00265ff8
TrRet vf_2d8050_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d8050
TrRet vf_2f1cc8_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f1cc8
TrRet vf_2f5e80_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f5e80
TrRet vf_2f6878_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f6878
TrRet vf_2f6960_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f6960
TrRet vf_2f6aa0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f6aa0
TrRet vf_2d8858_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d8858
TrRet vf_2d8888_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002d8888
TrRet vf_2f6e10_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f6e10
TrRet vf_2f6fa8_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f6fa8
TrRet vf_2f7028_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f7028
TrRet vf_2f70b8_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f70b8
TrRet vf_2f7220_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f7220
TrRet vf_2f7288_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f7288
TrRet vf_2f76a0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f76a0
TrRet vf_2fc668_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002fc668
TrRet vf_2fc7e0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002fc7e0
void vf_class1422_update_l18(uint32_t a0);  // L18:0x002f2bf0
void vf_class1432_update_l18(uint32_t a0);  // L18:0x002f7ab0
void vf_class1434_update_l18(uint32_t a0);  // L18:0x002f7ad8
TrRet vf_2a75f0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002a75f0
TrRet vf_2f8050_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f8050
TrRet vf_2f8658_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002f8658
void vf_class1454_update_l18(uint32_t a0);  // L18:0x002f7c40
TrRet vf_2fa250_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002fa250
void vf_class1563_update_l18(uint32_t a0);  // L18:0x002f88e8
void vf_class1568_update_l18(uint32_t a0);  // L18:0x002fa640
void vf_class1584_update_l18(uint32_t a0);  // L18:0x002fa728
TrRet vf_281fa8_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x00281fa8
void vf_class1750_update_l18(uint32_t a0);  // L18:0x002fad08
void vf_class1799_update_l18(uint32_t a0);  // L18:0x002fad28
void vf_class1899_update_l18(uint32_t a0);  // L18:0x002fb868
TrRet vf_2fc100_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002fc100
TrRet vf_2fc4e0_l18(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002fc4e0
void vf_class1906_update_l18(uint32_t a0);  // L18:0x002fbb98
TrRet vf_hook_2fcca0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L18:0x002fcca0
void vf_cb_2d5768_l18(uint32_t a0);  // L18:0x002d5768
void vf_cb_2dba20_l18(uint32_t a0);  // L18:0x002dba20
void vf_cb_2dc4b8_l18(uint32_t a0);  // L18:0x002dc4b8
void vf_cb_2f0390_l18(uint32_t a0);  // L18:0x002f0390
void vf_cb_2f06c0_l18(uint32_t a0);  // L18:0x002f06c0
void vf_cb_2f1fd8_l18(uint32_t a0);  // L18:0x002f1fd8
void vf_cb_2f7880_l18(uint32_t a0);  // L18:0x002f7880
void vf_cb_2f9780_l18(uint32_t a0);  // L18:0x002f9780
void vf_cb_2f9a98_l18(uint32_t a0);  // L18:0x002f9a98
void vf_cb_2f9c20_l18(uint32_t a0);  // L18:0x002f9c20
void vf_cb_2f9eb8_l18(uint32_t a0);  // L18:0x002f9eb8
void vf_cb_2fc860_l18(uint32_t a0);  // L18:0x002fc860
void vf_cb_2fca98_l18(uint32_t a0);  // L18:0x002fca98

}  // namespace rac1

#endif
