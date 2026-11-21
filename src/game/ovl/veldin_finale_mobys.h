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

void vf_class1422_update_l18(uint32_t a0);  // L18:0x002f2bf0 (veldin_finale_boss.cpp)
void vf_class1432_update_l18(uint32_t a0);  // L18:0x002f7ab0 (veldin_finale_scene.cpp)
void vf_class1434_update_l18(uint32_t a0);  // L18:0x002f7ad8 (veldin_finale_scene.cpp)
void vf_class1454_update_l18(uint32_t a0);  // L18:0x002f7c40 (veldin_finale_troopers.cpp)
void vf_class1563_update_l18(uint32_t a0);  // L18:0x002f88e8 (veldin_finale_scene.cpp)
void vf_class1568_update_l18(uint32_t a0);  // L18:0x002fa640 (veldin_finale_scene.cpp)
void vf_class1584_update_l18(uint32_t a0);  // L18:0x002fa728 (veldin_finale_raft.cpp)
void vf_class1750_update_l18(uint32_t a0);  // L18:0x002fad08 (veldin_finale_scene.cpp)
void vf_class1799_update_l18(uint32_t a0);  // L18:0x002fad28 (veldin_finale_scene.cpp)
void vf_class1906_update_l18(uint32_t a0);  // L18:0x002fbb98 (veldin_finale_robots.cpp)
void vf_cb_2f9a98_l18(uint32_t a0);  // L18:0x002f9a98 (veldin_finale_draws.cpp: class 1563's)
void vf_cb_2f9780_l18(uint32_t a0);  // L18:0x002f9780 (veldin_finale_draws.cpp: class 1563's)
void vf_cb_2f9c20_l18(uint32_t a0);  // L18:0x002f9c20 (veldin_finale_draws.cpp: class 1563's)
void vf_cb_2f9eb8_l18(uint32_t a0);  // L18:0x002f9eb8 (veldin_finale_draws.cpp: class 1563's)
void vf_cb_2fc860_l18(uint32_t a0);  // L18:0x002fc860 (veldin_finale_draws.cpp: class 1906's)
void vf_cb_2fca98_l18(uint32_t a0);  // L18:0x002fca98 (veldin_finale_draws.cpp: class 1906's)
void vf_cb_2f0390_l18(uint32_t a0);  // L18:0x002f0390 (veldin_finale_draws.cpp: class 1355's)
void vf_cb_2f06c0_l18(uint32_t a0);  // L18:0x002f06c0 (veldin_finale_draws.cpp: class 1355's)
void vf_cb_2f1fd8_l18(uint32_t a0);  // L18:0x002f1fd8 (veldin_finale_draws.cpp: class 1392's)
void vf_cb_2f7880_l18(uint32_t a0);  // L18:0x002f7880 (veldin_finale_draws.cpp: class 1422's)
void vf_cb_2d5768_l18(uint32_t a0);  // L18:0x002d5768 (veldin_finale_shot_draws.cpp: class 564's)
void vf_cb_2dba20_l18(uint32_t a0);  // L18:0x002dba20 (veldin_finale_shot_draws.cpp: class 624's)
void vf_cb_2dc4b8_l18(uint32_t a0);  // L18:0x002dc4b8 (veldin_finale_shot_draws.cpp: class 628's)
void vf_cb_2ea1f8_l18(uint32_t a0);  // L18:0x002ea1f8 (veldin_finale_shot_draws.cpp: class 983's)

// still generated (veldin_finale_classes_tr_6.cpp)
void vf_class1899_update_l18(uint32_t a0);  // L18:0x002fb868

}  // namespace rac1

#endif
