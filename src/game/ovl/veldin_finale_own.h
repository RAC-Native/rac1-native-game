// Veldin's finale (level 18): the hand-written helpers of the level's own classes (the class updates themselves keep their
// names, declared in veldin_finale_mobys.h). Level 18 only; the files: veldin_finale_shots.cpp, veldin_finale_walking_bomb.cpp,
// veldin_finale_rail_switch.cpp, veldin_finale_platforms.cpp,
// veldin_finale_camera_pan.cpp, veldin_finale_breakable.cpp, veldin_finale_energy_ball.cpp, veldin_finale_diver.cpp,
// veldin_finale_raft.cpp, veldin_finale_barrier.cpp, veldin_finale_water.cpp, veldin_finale_boss_props.cpp,
// veldin_finale_boss_spawns.cpp, veldin_finale_boss_level.cpp, veldin_finale_boss_moves.cpp, veldin_finale_boss_frame.cpp,
// veldin_finale_troopers.cpp, veldin_finale_scene.cpp, veldin_finale_robots.cpp.
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
int32_t vf_bomb_hookc_l18(uint32_t m, uint32_t a1, uint32_t a2, float f12);       // L18:0x002d6230 L11:0x003162d0
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

// the boss (class 1422), what it does to the other mobys (veldin_finale_boss_props.cpp; called by the boss's
// update and other classes' generated code): the level's sound `idx` on `moby` (the slot, -1 none); its moby groups hidden / shown; the
// group's raft platforms' +0xbc; the group's robots (class 1906) out of their state 8; the gates (class 583): the one
// farthest round from `from` seen from `ref`, the nearest to `at`, all of a group opened, one opened; the floating
// platforms (class 587) sunk / rocked; the lift (class 586) polled, sent up, stopped
int32_t vf_level_sound_l18(int32_t idx, uint32_t flags, uint32_t moby);               // L18:0x0028d878
void vf_group_hide_l18(int32_t group);                                                // L18:0x002f6fa8
void vf_group_show_l18(int32_t group);                                                // L18:0x002f7028
void vf_group_platforms_flag_l18(int32_t group, uint32_t value);                      // L18:0x002fa888
int32_t vf_robots_out_l18(int32_t group);                                             // L18:0x002fc7e0
uint32_t vf_gate_farthest_round_l18(uint32_t from, uint32_t ref, int32_t group);      // L18:0x002d6b58
uint32_t vf_gate_nearest_l18(uint32_t at, int32_t group);                             // L18:0x002d6c80
void vf_gates_open_l18(int32_t group);                                                // L18:0x002d6d50
void vf_gate_open_l18(uint32_t gate);                                                 // L18:0x002d6dc0
uint32_t vf_float_sink_l18(uint32_t o);                                               // L18:0x002d8858
uint32_t vf_float_rock_l18(uint32_t o, int32_t rock_wait, int32_t sink_wait);         // L18:0x002d8888
int32_t vf_lift_poll_l18(uint32_t o);                                                 // L18:0x002d8050
void vf_lift_start_l18(uint32_t o, int32_t wait);                                     // L18:0x002d7f68
void vf_lift_stop_l18(uint32_t o);                                                    // L18:0x002d7fd0

// the boss's shots and launches (veldin_finale_boss_spawns.cpp; called by the boss's update;
// EE points / vectors by address): its shell (class 564), ring (624), burst (628) spawned, the burst moved, its energy
// ball (983) spawned and launched; a group's walking bomb (568) thrown, robot (1906) sent off, divers (1355) sent off
uint32_t vf_shell_spawn_l18(uint32_t at, uint32_t vel, uint32_t to, int32_t frames, uint32_t flags, float f12, float f13);  // L18:0x002d5348
void vf_ring_spawn_l18(uint32_t at, uint32_t dir, uint32_t owner, float radius, float speed);                        // L18:0x002db938
uint32_t vf_burst_spawn_l18(uint32_t owner, uint32_t at, float radius);                                              // L18:0x002dc3e0
void vf_burst_move_l18(uint32_t m, uint32_t at, float radius);                                                       // L18:0x002dc458
uint32_t vf_ball_spawn_l18(uint32_t owner, uint32_t at, uint32_t frames);                                            // L18:0x002ea0f0
void vf_ball_launch_l18(uint32_t m, uint32_t at, uint32_t target, uint32_t go, float f12);                           // L18:0x002ea168
uint32_t vf_bomb_throw_l18(uint32_t owner, int32_t group, uint32_t from, uint32_t to, uint32_t fuse, float speed);  // L18:0x002d5cf8
uint32_t vf_robot_launch_l18(int32_t group, uint32_t from, uint32_t to, float speed);                                // L18:0x002fc668
uint32_t vf_divers_launch_l18(uint32_t owner, int32_t group, uint32_t from);                                         // L18:0x002f0030

// the boss in the level (veldin_finale_boss_level.cpp; called by the boss's update): the camera
// move (class 644) started from five EE vectors, its progress; a raft (class 1381) sent off with its platforms, the ties
// of a kind freed in a box, a raft sunk by the energy ball; the byte +0xa of a moby's pvar-word-0 record by the hero's
// distance; the hero's ride on a floating platform counted; the scene's Drek model scaled
void vf_pan_start_l18(uint32_t pan, uint32_t first, uint32_t last, uint32_t first_angles, uint32_t last_angles,
                      uint32_t hero_at, int32_t frames, int32_t wait, uint32_t clank);  // L18:0x002dfaa0
float vf_pan_progress_l18(uint32_t pan);                                                  // L18:0x002dfb90
void vf_raft_hold_l18(uint32_t o);                                                        // L18:0x002f1cc8
void vf_ties_free_l18(uint32_t kind, int32_t box);                                        // L18:0x002f1d08
void vf_raft_sink_l18(uint32_t o);                                                        // L18:0x002f1c38
void vf_record_by_hero_distance_l18(uint32_t m, float scale);                             // L18:0x00265ff8
void vf_hero_float_count_l18(uint32_t m);                                                 // L18:0x002f5e18
void vf_drek_scale_l18();                                                                 // L18:0x002f2b18

// the boss's moves (veldin_finale_boss_moves.cpp; called by the boss's update): its home
// stepped toward an EE point (1: there), the charge at the hero, the orbit about the arena, the floating platform to
// fly to (0: none), the next attack (a state), a bearing against a moby's yaw into `out` + 0x68
uint32_t vf_boss_home_step_l18(uint32_t m, uint32_t goal, float max);       // L18:0x002f6878
void vf_boss_charge_l18(uint32_t m);                                         // L18:0x002f6960
void vf_boss_orbit_l18(uint32_t m);                                          // L18:0x002f6aa0
uint32_t vf_boss_float_pick_l18(uint32_t m);                                 // L18:0x002f6e10
int32_t vf_boss_next_attack_l18(uint32_t m);                                 // L18:0x002f70b8
void vf_boss_bearing_l18(uint32_t m, uint32_t out, uint32_t from, uint32_t to);  // L18:0x002f7220

// the boss every frame around its state (veldin_finale_boss_frame.cpp; called by the boss's
// update): its preamble (hits, health, the attached mobys, the target, the lift's end, the phase change), the fight's
// camera, the smoke and sparks at its mouth (the EE point `at`)
void vf_boss_preamble_l18(uint32_t m);                // L18:0x002f5e80
void vf_boss_camera_l18(uint32_t m);                  // L18:0x002f7288
void vf_boss_smoke_l18(uint32_t m, uint32_t at);      // L18:0x002f76a0

// the troopers (class 1454, veldin_finale_troopers.cpp): their rocket (class 41) at the EE point `at` with the EE
// velocity `vel`, homing when `homing` (0: none spawned); their every frame before the state (hits, target, aim); their
// walk along their path
uint32_t vf_rocket_spawn_l18(uint32_t owner, uint32_t at, uint32_t vel, uint32_t homing);  // L18:0x002a75f0
void vf_trooper_frame_l18(uint32_t m);                                                     // L18:0x002f8050
void vf_trooper_walk_l18(uint32_t m);                                                      // L18:0x002f8658

// the classes the scenes drive (veldin_finale_scene.cpp): the game's save image built once into `buf`; the end scene's
// sparks at the EE points `at` / `at2` moving by the EE vector `vel`
void vf_save_image_once_l18(uint32_t buf);                                                  // L18:0x00281fa8
void vf_scene_sparks_l18(uint32_t at, uint32_t at2, uint32_t vel, float size_a, float size_b);  // L18:0x002fa250

// the boss's robots (class 1906, veldin_finale_robots.cpp): their every frame before the state (glow, hits, target);
// their step toward the EE point `goal` (the distance to it); their carry hook +0xc
void vf_robot_frame_l18(uint32_t m);                                               // L18:0x002fc100
float vf_robot_walk_l18(uint32_t m, uint32_t goal);                                // L18:0x002fc4e0
int32_t vf_robot_hookc_l18(uint32_t m, uint32_t a1, uint32_t a2, float f12);       // L18:0x002fcca0

}  // namespace rac1

#endif
