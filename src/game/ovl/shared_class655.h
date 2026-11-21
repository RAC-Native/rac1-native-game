// Class 655 of Quartu and Drek's fleet: an electric field that strikes the hero while it stands in its area on the ground,
// with its arcs (shared_class655.cpp: one port of both levels' copies, the same code relinked; its glows are
// shared_class655_glows.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by
// oracle batch 516.
#ifndef RAC1_GAME_OVL_SHARED_CLASS655_H
#define RAC1_GAME_OVL_SHARED_CLASS655_H

#include <cstdint>

namespace rac1 {

// Class 655's update.
void class655_update(uint32_t m);  // L15:0x002d6810 L17:0x002d4e88
// The arc's points stepped: the main arc from pvar +0xf0 to +0x110 wobbling, four branches flickering, three sparks.
void class655_arcs(uint32_t m);  // L15:0x002d7bb0 L17:0x002d6228
// The arc started again (once, while the level's word says so): its points along the line, the branches' timers.
void class655_arcs_reset(uint32_t m);  // L15:0x002d79e8 L17:0x002d6060
// 1 when the hero is on the ground for the strike (or in a scene), else 0.
uint32_t class655_hero_grounded();  // L15:0x002d6770 L17:0x002d4de8
// A class-1179 moby in state 2 put back to state 1, 1.5 higher; 1 (always).
uint32_t class1179_lift(uint32_t o);  // L15:0x002d95a0 L17:0x002d77a8

}  // namespace rac1

#endif
