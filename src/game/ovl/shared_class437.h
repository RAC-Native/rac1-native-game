// Class 437 of Novalis, Aridia and Eudora: an older build of the Sandmouse (class 1818, shared_sandmouse.cpp) that waits at
// its house, takes the hero, runs about shooting class 1633's shots at targets and burrows back (shared_class437.cpp: one
// port of the three levels' copies, the same code relinked). Phase 1 of stage 4: the levels' generated copies still run;
// these are proved against them by oracle batch 520.
#ifndef RAC1_GAME_OVL_SHARED_CLASS437_H
#define RAC1_GAME_OVL_SHARED_CLASS437_H

#include <cstdint>

namespace rac1 {

// The update.
void class437_update(uint32_t m);  // L1:0x002e3f20 L2:0x002ccc08 L4:0x002c7e28
// Its step toward its goal (+0 of its pvars `pv`): turning, walking, pushed away from the hero.
void class437_move(uint32_t m, uint32_t pv);  // L1:0x002e3830 L2:0x002cc518 L4:0x002c7738
// The best target from `from` for a shooter turned by `angles` (+4 its pitch, +8 its yaw) among the mobys of the update
// list: shown, flagged 0x1000, of kind 5 alive (its pvars' first word above 0), within `range`, within the yaw and pitch
// reaches (or `near` and within the near reaches), seen; 0: none.
uint32_t class437_find_target(uint32_t m, uint32_t from, uint32_t angles, float yaw_reach, float pitch_reach, float range,
                              float near, float near_yaw, float near_pitch);  // L1:0x002e32a8 L2:0x002cbf90 L4:0x002c71b0
// The point of the moby's collision primitive `joint` (scaled, turned and placed as the moby; its kind in `kind`, its
// size in `size`, a second size or 0 in `size2`); 0 when the moby has no collision.
uint32_t moby_collision_point(uint32_t m, uint32_t joint, uint32_t kind, uint32_t point, uint32_t size,
                              uint32_t size2);  // L1:0x00263a28 L2:0x002512b8 L4:0x00241980
// A shot at its target when its timer runs out.
void class437_shoot(uint32_t m, uint32_t pv);  // L1:0x002e35a8 L2:0x002cc290 L4:0x002c74b0
// Its goal beside the hero: 1.1 ahead, 1 to the side, on the floor (or 1 up with a target).
void class437_goal_by_hero(uint32_t m, uint32_t pv);  // L1:0x002e3110 L2:0x002cbdf8 L4:0x002c7018
// Its fall back into its house: slowed, dropping, walked (the walker's result).
uint32_t class437_sink(uint32_t m, uint32_t pv);  // L1:0x002e3208 L2:0x002cbef0 L4:0x002c7110

}  // namespace rac1

#endif
