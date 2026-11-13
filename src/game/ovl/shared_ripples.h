// The rippling surfaces of Quartu (classes 1408, 1409, 1565, 1567) and Drek's fleet (classes 1405, 1406, 1407, 1426): one
// update for all eight classes (it only queues the class's draw) and the draws of two kinds of surface, meshes whose points
// bob on a sine of their distance from the centre and whose texture scrolls; one port of the levels' copies
// (shared_ripples.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by
// oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_RIPPLES_H
#define RAC1_GAME_OVL_SHARED_RIPPLES_H

#include <cstdint>

namespace rac1 {

// The classes' update, with the class's own draw callback `draw` (the level's address).
void ripple_update(uint32_t m, uint32_t draw);
void class1408_update(uint32_t m);  // L15:0x002eb0a0
void class1409_update(uint32_t m);  // L15:0x002eb458
void class1565_update(uint32_t m);  // L15:0x002edf58
void class1567_update(uint32_t m);  // L15:0x002ee338
void class1405_update(uint32_t m);  // L17:0x002f08e0
void class1406_update(uint32_t m);  // L17:0x002f0ca0
void class1407_update(uint32_t m);  // L17:0x002f1060
void class1426_update(uint32_t m);  // L17:0x002f1418
// The ring's draw (class 1408 on Quartu: three meshes; class 1405 on Drek's fleet: two).
void ripple_rings_draw(uint32_t m);  // L15:0x002eac90 L17:0x002f04d0
// The pool's draw (class 1409 on Quartu: 68 points; class 1426 on Drek's fleet: 45).
void ripple_pool_draw(uint32_t m);   // L15:0x002eb108 L17:0x002f10c8

}  // namespace rac1

#endif
