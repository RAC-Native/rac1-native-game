// Class 827 (a walker of Blarg and Orxon that patrols a path, chases the hero or a mate of its class and carries a rider,
// class 500) with its helpers and its class hooks, shared by the two levels that carry it: one port of the levels' copies
// (shared_class827.cpp). Each definition's `orig:` line lists every level's address.
#ifndef RAC1_GAME_OVL_SHARED_CLASS827_H
#define RAC1_GAME_OVL_SHARED_CLASS827_H

#include <cstdint>

namespace rac1 {

// The update.
void class827_update(uint32_t m);  // L6:0x002e8678 L10:0x002cc8b8
// Its hits, its death, its colour pulse and its target (once a frame, before the state).
void class827_react(uint32_t m);  // L6:0x002e94f0 L10:0x002cd730
// The sparks of a hit, pushed along `push` (a vector's address).
void class827_sparks(uint32_t m, uint32_t push);  // L6:0x002ea198 L10:0x002ce1d0
// The rider's debris (twelve chunks), the rider retired.
void class827_rider_debris(uint32_t m);  // L6:0x002e9f30 L10:0x002cdf68
// A step by the vector at `step` with gravity, kept out of walls (radius `radius`) and, with `on_path`, on its path.
void class827_move(uint32_t m, uint32_t step, uint32_t on_path, float up, float radius);  // L6:0x002e9b60 L10:0x002cdda0
// The nearest target (`*out`): a mate of class 857 or the hero, scored by distance and turn, within `range`.
void class827_target_pick(uint32_t m, uint32_t out, float range);  // L6:0x002f34d8 L10:0x002d7510
// One candidate's score against the best so far (`*best`, `*best_score`).
void class827_target_score(uint32_t m, uint32_t current, uint32_t cand, uint32_t best, uint32_t best_score, float range);  // L6:0x002f33e8 L10:0x002d7420
// The death's glow (twenty sparks).
void class827_glow_burst(uint32_t m);  // L6:0x002ea498 L10:0x002ce4d0
// The matrix turned toward the direction at `dir`, upright along the vector at `up`.
void class827_turn(uint32_t m, uint32_t up, uint32_t dir);  // L6:0x002e9998 L10:0x002cdbd8
// The ground under it along its up axis (the collision's result); nonzero when found.
int32_t class827_ground(uint32_t m);  // L6:0x002e9aa8 L10:0x002cdce8

// The class hooks (+0, +4 / +8, +0xc of the class's hook table): a carry hook, the state 16 (carried) kept while it holds.
int32_t class827_hook_carry0(uint32_t m, uint32_t a1, uint32_t a2, float f12);  // L6:0x002ea5e0 L10:0x002ce618
int32_t class827_hook_carry(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which);  // L6:0x002ea6a8 L6:0x002ea738 L10:0x002ce6e0 L10:0x002ce770
int32_t class827_hook_carryc(uint32_t m, uint32_t a1, uint32_t a2, float f12);  // L6:0x002ea7c8 L10:0x002ce800

}  // namespace rac1

#endif
