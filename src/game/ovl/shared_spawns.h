// Small spawn and group helpers several levels carry copies of (shared_spawns.cpp; Gaspar's spawn is another build, written
// as its own function). Phase 1 of stage 4: the levels' generated copies still run; proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_SPAWNS_H
#define RAC1_GAME_OVL_SHARED_SPAWNS_H

#include <cstdint>

namespace rac1 {

// a moby of class `cls` at `pos` (EE vector) turned and coloured as `src`; the moby or 0 (class 0x1f3: L4:0x002ce080,
// L12:0x002f1118; 0x23a: L4:0x002d34d8)
uint32_t spawn_like(uint32_t src, uint32_t pos, uint32_t cls);
// Gaspar's build (L9:0x002efda8): class 0x144, its alpha and its colour's high byte from `src`'s +0x30, its turn copied whole
uint32_t spawn_like_gaspar(uint32_t src, uint32_t pos);
// a moby of class `cls` over `src` at the height 59 (class 0x462: L10:0x002dd848; 0x781: L10:0x002eb8c0)
uint32_t spawn_over_at_59(uint32_t src, uint32_t cls);
// the moby's group: its members in state 1 put in state 2 (L10:0x002e0138 / 0x002e47d8)
void group_wake(uint32_t m);

}  // namespace rac1

#endif
