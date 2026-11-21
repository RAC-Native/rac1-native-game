// Drek's fleet (level 17) transliterated moby classes, camera type, class hooks, draw callbacks, the camera mode hook and level-only engine functions: the declarations of
// veldin_orbit_classes_tr_N.cpp (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI).
#ifndef RAC1_GAME_OVL_VELDIN_ORBIT_MOBYS_H
#define RAC1_GAME_OVL_VELDIN_ORBIT_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// hand-written (veldin_orbit_beams.cpp, _cannon.cpp, _hazards.cpp, _doors.cpp, _ship.cpp, _lift.cpp, _trooper.cpp, _pod.cpp, _props.cpp,
// _monitor.cpp, _backdrop.cpp, _fighter.cpp)
void vo_class99_update_l17(uint32_t m);    // L17:0x002a8da0 (the laser beam)
void vo_cb_2a95b8_l17(uint32_t m);         // L17:0x002a95b8 (class 99's draw: the beam's quad)
void vo_class347_update_l17(uint32_t m);   // L17:0x002cb310 (the laser cannon)
void vo_class669_update_l17(uint32_t m);   // L17:0x002d77f0 (the electric fan)
void vo_cb_2d7cf0_l17(uint32_t m);         // L17:0x002d7cf0 (class 669's draw: the arms' quads)
void vo_class835_update_l17(uint32_t m);   // L17:0x002dc3f8 (the floating mine)
void vo_class1359_update_l17(uint32_t m);  // L17:0x002e87d8 (the sliding doors, classes 1359 .. 1373)
void vo_class1368_update_l17(uint32_t m);  // L17:0x002e8e08 (the cannon's shot)
void vo_class1379_update_l17(uint32_t m);  // L17:0x002ed018 (the hero's ship)
void vo_class1380_update_l17(uint32_t m);  // L17:0x002ee2b0 (a lift)
void vo_class1382_update_l17(uint32_t m);  // L17:0x002eeb68 (a trooper)
void vo_class1448_update_l17(uint32_t m);  // L17:0x002f1940 (a pod that carries the hero)
void vo_class1470_update_l17(uint32_t m);  // L17:0x002f26d0 (the level's monitor: skill points, hints, a camera look)
void vo_class1562_update_l17(uint32_t m);  // L17:0x002f2e78 (a cutscene's moby kept smoking)
void vo_class1772_update_l17(uint32_t m);  // L17:0x002f3848 (a far backdrop of light beams)
void vo_cb_2f32f0_l17(uint32_t m);         // L17:0x002f32f0 (class 1772's draw: the beams and the far squares)
void vo_class1843_update_l17(uint32_t m);  // L17:0x002f40d8 (Drek's fighter)
void vo_class1873_update_l17(uint32_t m);  // L17:0x002f5260 (a prop that breaks)
void vo_class1876_update_l17(uint32_t m);  // L17:0x002f5390 (a prop that breaks)

// generated (veldin_orbit_classes_tr_N.cpp)
TrRet vo_2eaae0_l17(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L17:0x002eaae0
TrRet vo_2eace8_l17(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L17:0x002eace8
TrRet vo_2eb768_l17(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L17:0x002eb768
void vo_cb_2eb9a8_l17(uint32_t a0);  // L17:0x002eb9a8

}  // namespace rac1

#endif
