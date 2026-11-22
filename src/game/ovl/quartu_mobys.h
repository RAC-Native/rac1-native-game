// Quartu (level 15) transliterated moby classes, camera types, class hooks, draw callbacks and level-only engine functions: the declarations of
// quartu_classes_tr_N.cpp (generated until stage 4: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI). Oracle batch 240 (ORACLE_LEVEL=15).
#ifndef RAC1_GAME_OVL_QUARTU_MOBYS_H
#define RAC1_GAME_OVL_QUARTU_MOBYS_H

#include <cstdint>

#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// Hand-ported Quartu (level 15) moby classes (quartu_doors.cpp, quartu_destructibles.cpp, quartu_class221.cpp):
void qua_class67_update_l15(uint32_t m);   // L15:0x0029aff0
void qua_class92_update_l15(uint32_t m);   // L15:0x002a36d0
void qua_class93_update_l15(uint32_t m);   // L15:0x002a3ba8
void qua_class148_update_l15(uint32_t m);  // L15:0x002a7880
void qua_class154_update_l15(uint32_t m);  // L15:0x002aa280
void qua_class221_update_l15(uint32_t m);  // L15:0x002c2938
// The flying craft (class 233) and its helpers (quartu_class233.cpp):
void qua_class233_update_l15(uint32_t m);  // L15:0x002c5630
uint32_t qua_247e90_l15(uint32_t m, uint32_t vel, uint32_t flag, float lift, float radius);  // L15:0x00247e90 the move against the walls
void qua_2c4f88_l15(uint32_t m, const Vec4& goal, const Vec4& look);  // L15:0x002c4f88 the flight toward a goal
void qua_2c52b8_l15(uint32_t m, uint32_t out, uint32_t target);  // L15:0x002c52b8 its place near the target
void qua_2c6900_l15(uint32_t m);  // L15:0x002c6900 its hits

// The melee robot (class 491) and its helpers (quartu_class491.cpp):
void qua_class491_update_l15(uint32_t m);  // L15:0x002cf3a8
float qua_2cf110_l15(uint32_t m, uint32_t out, float range);  // L15:0x002cf110 its place near the target (returns the yaw toward it)
void qua_2d0fa8_l15(uint32_t m);  // L15:0x002d0fa8 its hits and its target

// The small classes of the Giant Clank part (quartu_props.cpp):
void qua_class937_update_l15(uint32_t m);   // L15:0x002e46b0 a wheel turning with a linked moby
void qua_class1250_update_l15(uint32_t m);  // L15:0x002e73c0 a conveyor piece
void qua_class1560_update_l15(uint32_t m);  // L15:0x002edb20 a scene's smoke
void qua_class1394_update_l15(uint32_t m);  // L15:0x002eabd8 a ball dropper
uint32_t qua_2e7ed8_l15(uint32_t pos);      // L15:0x002e7ed8 its ball (class 1257) spawned at `pos`
void qua_class1257_update_l15(uint32_t m);  // L15:0x002e7f68 the ball
void qua_class1425_update_l15(uint32_t m);  // L15:0x002eb928 a steam vent
// The story triggers and the scene's subtitle (quartu_triggers.cpp):
void qua_class1388_update_l15(uint32_t m);  // L15:0x002ea748 the item pickup
void qua_class1419_update_l15(uint32_t m);  // L15:0x002eb4c0 the level's end
void qua_cb_2eb6b8_l15(uint32_t m);         // L15:0x002eb6b8 the scene's subtitle (a draw callback)
// The Giant Clank pad and its helpers (quartu_clank_pad.cpp):
void qua_class1430_update_l15(uint32_t m);              // L15:0x002ec030
uint32_t qua_2eb9e0_l15(uint32_t pos, uint32_t rot);    // L15:0x002eb9e0 the Clank (class 1428) spawned
void qua_2ebf28_l15(int32_t group, bool show);          // L15:0x002ebf28 a group's mobys hidden / shown
int32_t qua_2ebfb0_l15(int32_t group);                  // L15:0x002ebfb0 a group's gone mobys counted
bool qua_2f78e0_l15(int32_t camera);                    // L15:0x002f78e0 the level camera running and settled
// The talking character (quartu_class1446.cpp) and the help-message director (quartu_class1469.cpp):
void qua_class1446_update_l15(uint32_t m);  // L15:0x002ec760
void qua_class1469_update_l15(uint32_t m);  // L15:0x002ed398
// The camera type 22 (Giant Clank's camera; quartu_cam22.cpp): the accept in the uniform ABI (quartu_hooks.cpp's dispatch)
TrRet qua_cam_type22_accept_u(uint32_t cam, uint32_t other, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L15:0x002f8ba8
bool qua_2f85a8_l15(uint32_t cam);                                // L15:0x002f85a8 the hero inside the camera's volume
bool qua_2f8698_l15(uint32_t cam, float turn_tol, float tilt_tol);  // L15:0x002f8698 the camera within the angles of its box
void qua_2f88e8_l15(uint32_t cam);                                // L15:0x002f88e8 the camera steered for Giant Clank

void qua_cutscene_level_extras_l15();  // L15:0x0022db50 (quartu_props.cpp: the scenes' level extras, cutscene.cpp's dispatch)
TrRet qua_hook_2bc4b0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L15:0x002bc4b0
TrRet qua_hook_2bc500_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L15:0x002bc500
TrRet qua_hook_2bc550_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L15:0x002bc550
TrRet qua_hook_2bc5a0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L15:0x002bc5a0
TrRet qua_hook_2bc5d0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L15:0x002bc5d0

}  // namespace rac1

#endif
