// Class 77, a mine that hides, wakes, creeps to the hero and blows up (Quartu, Drek's fleet), and the glow its group draws
// while it is awake: one port of the two levels' copies (shared_class77.cpp). Phase 1 of stage 4: the levels' generated
// copies still run; these are proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS77_H
#define RAC1_GAME_OVL_SHARED_CLASS77_H

#include <cstdint>

namespace rac1 {

void class77_update(uint32_t m);  // L15:0x002a2488 L17:0x002a7b58
// The group's draw (queued by the update with sub_001f93e0): four quads about each of the group's mobys not resting.
void class77_draw(uint32_t m);    // L15:0x002a29b8 L17:0x002a8088

}  // namespace rac1

#endif
