// Class 1224, a bolt of lightning that strikes along a volume of the level (Oltanis, Quartu, Drek's fleet), shared by the
// three levels that have it: one port of the levels' copies of its update, its helpers and its draw callback
// (shared_class1224.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by
// oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1224_H
#define RAC1_GAME_OVL_SHARED_CLASS1224_H

#include <cstdint>

namespace rac1 {

void class1224_update(uint32_t m);  // L14:0x003015d0 L15:0x002e6038 L17:0x002e6468
// A new bolt: its 32 points along the volume, their drift, four forks and the sparks at the points.
void class1224_bolt(uint32_t m);    // L14:0x003017f8 L15:0x002e6260 L17:0x002e6690
// The bolt's ends clipped to the scenery once, then four sweeps of its sides that hit what they cross.
void class1224_strike(uint32_t m);  // L14:0x00302538 L15:0x002e6fa0 L17:0x002e73d0
// One frame of a bolt: the points drift, the forks follow, the colour fades.
void class1224_fade(uint32_t m);    // L14:0x00301de8 L15:0x002e6850 L17:0x002e6c80
// The draw callback: the bolt and its forks as strips of quads facing the camera.
void class1224_draw(uint32_t m);    // L14:0x00301fa8 L15:0x002e6a10 L17:0x002e6e40

}  // namespace rac1

#endif
