// Veldin's finale (level 18): the hand-written helpers of the level's own classes (the class updates themselves keep their
// names, declared in veldin_finale_mobys.h). Level 18 only; the files: veldin_finale_shots.cpp, veldin_finale_walking_bomb.cpp,
// veldin_finale_rail_switch.cpp, veldin_finale_platforms.cpp,
// veldin_finale_camera_pan.cpp, veldin_finale_breakable.cpp, veldin_finale_energy_ball.cpp, veldin_finale_diver.cpp,
// veldin_finale_raft.cpp, veldin_finale_barrier.cpp, veldin_finale_water.cpp.
#ifndef RAC1_GAME_OVL_VELDIN_FINALE_OWN_H
#define RAC1_GAME_OVL_VELDIN_FINALE_OWN_H

#include <cstdint>

namespace rac1 {

// the boss's shell (class 564): its smoke along `dir` (an EE vector), its blast
void vf_shell_smoke_l18(uint32_t m, uint32_t dir);  // L18:0x002d5488
void vf_shell_blast_l18(uint32_t m, uint32_t dir);  // L18:0x002d5610

// the boss's ring (class 624): its sparks on the segment `a` .. `b` (EE vectors), its hurt along its edge
void vf_ring_sparks_l18(uint32_t m, uint32_t a, uint32_t b);  // L18:0x002dc0c8
void vf_ring_hurt_l18(uint32_t m);                            // L18:0x002dbe80

// the walking bombs (class 568): their checks before the state; their carry hooks +0, +0xc, +0x10 (the table L18:0x1f3498)
void vf_bomb_fuse_l18(uint32_t m);                                                // L18:0x002d5f20
int32_t vf_bomb_hook0_l18(uint32_t m, uint32_t a1, uint32_t a2, float f12);       // L18:0x002d6108
int32_t vf_bomb_hookc_l18(uint32_t m, uint32_t a1, uint32_t a2, float f12);       // L18:0x002d6230
uint32_t vf_bomb_hook10_l18(uint32_t m);                                          // L18:0x002d6250

// the rail switch (class 582): a Nanotech pickup (class 806) at `at` (an EE point); returns it (0: none)
uint32_t vf_nanotech_spawn_l18(uint32_t at);  // L18:0x002e22c0

// class 885 (veldin_finale_breakable.cpp): the moby group `group` in state 2 (its pieces break)
void vf_group_break_l18(uint32_t group);  // L18:0x002e9e50

// class 983, the energy ball (veldin_finale_energy_ball.cpp): its fizz, its blast, its arc started again, its arc stepped
void vf_ball_fizz_l18(uint32_t m);       // L18:0x002ea598
void vf_ball_blast_l18(uint32_t m);      // L18:0x002ea800
void vf_ball_arc_reset_l18(uint32_t m);  // L18:0x002eacd8
void vf_ball_arc_l18(uint32_t m);        // L18:0x002eaea0

// class 1355, the diver (veldin_finale_diver.cpp): its step toward `target` (1: a new point wanted), a new point into
// `target`, its trail
uint32_t vf_diver_step_l18(uint32_t m, uint32_t target);       // L18:0x002f0120
void vf_diver_new_point_l18(uint32_t m, uint32_t target);      // L18:0x002f02e8
void vf_diver_trail_l18(uint32_t m);                           // L18:0x002f0848

// class 1381, the raft (veldin_finale_raft.cpp): one of its platforms (class 1584) gone, with the moby its +0x84 names
void vf_raft_platform_retire_l18(uint32_t o);  // L18:0x002fa8e0

// class 1402, the water (veldin_finale_water.cpp): its group's mobys at its height
void vf_water_group_l18(uint32_t m);  // L18:0x002f2598

// the floating platforms (class 587): their bob and rocking, their bubbles
void vf_float_bob_l18(uint32_t m);      // L18:0x002d8710
void vf_float_bubbles_l18(uint32_t m);  // L18:0x002d88b8

}  // namespace rac1

#endif
