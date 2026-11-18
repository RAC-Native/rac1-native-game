// Hoven (level 12) hand-written moby classes, class hooks and draw callbacks.
#ifndef RAC1_GAME_OVL_HOVEN_MOBYS_H
#define RAC1_GAME_OVL_HOVEN_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// ---- hand-written (task P12.3): classes 1269, 1274, 1281, 1345, 1404, 1557, 1901, their helpers, hooks 0x2e16b8 / 0x2e17f8,
// and draw callbacks 0x2ecac0, 0x3028c8, 0x304218, 0x304d98, 0x308350, 0x30be68
void hov_class1269_update_l12(uint32_t m);     // L12:0x00304e00 (hoven_drone_hatch.cpp)
void hov_class1274_update_l12(uint32_t m);     // L12:0x003069d0 (hoven_dropship.cpp)
void hov_class1281_update_l12(uint32_t m);     // L12:0x00307840 (hoven_drone_hatch.cpp)
void hov_class1345_update_l12(uint32_t m);     // L12:0x003081b0 (hoven_scene_fx.cpp)
void hov_class1404_update_l12(uint32_t m);     // L12:0x003093c8 (hoven_scene_fx.cpp)
void hov_class1557_update_l12(uint32_t m);     // L12:0x003094a0 (hoven_scene_fx.cpp)
void hov_class1901_update_l12(uint32_t m);     // L12:0x0030bff0 (hoven_scene_fx.cpp)
int32_t hov_hook_2e16b8_l12(uint32_t m, uint32_t a1, uint32_t a2, float f12);  // L12:0x002e16b8 (hoven_carry.cpp)
int32_t hov_hook_2e17f8_l12(uint32_t m, uint32_t a1, uint32_t a2, float f12);  // L12:0x002e17f8 (hoven_carry.cpp)
void hov_cb_2ecac0_l12(uint32_t m);            // L12:0x002ecac0 (hoven_draws.cpp)
void hov_cb_3028c8_l12(uint32_t m);            // L12:0x003028c8 (hoven_draws.cpp)
void hov_cb_304218_l12(uint32_t m);            // L12:0x00304218 (hoven_draws.cpp)
void hov_cb_304d98_l12(uint32_t m);            // L12:0x00304d98 (hoven_draws.cpp)
void hov_cb_308350_l12(uint32_t m);            // L12:0x00308350 (hoven_draws.cpp)
void hov_cb_30be68_l12(uint32_t m);            // L12:0x0030be68 (hoven_draws.cpp)
void dropship_piece_init(uint32_t m, int32_t idx);  // L12:0x00306350 (hoven_dropship.cpp)
void dropship_pieces_fall(uint32_t m);              // L12:0x003064e8 (hoven_dropship.cpp)
void dropship_turrets_update(uint32_t m);           // L12:0x003054b0 (hoven_dropship.cpp)
void dropship_pulse_tint(uint32_t m);               // L12:0x00306828 (hoven_dropship.cpp)
void drone_hatch_spawn(uint32_t m);                 // L12:0x003076e0 (hoven_drone_hatch.cpp)
void drone_hatch_target(uint32_t m);                // L12:0x00307ab8 (hoven_drone_hatch.cpp)

// ---- hand-written (task P12.1): the helpers of classes 238 (hoven_snow_critter.cpp), 240 (hoven_jet_platform.cpp) and the
// carry at a joint (hoven_raritanium_npc.cpp); the class updates keep their names
void hov_class19_update_l12(uint32_t a0);   // L12:0x002bf140 (hoven_water.cpp)
void hov_class238_update_l12(uint32_t a0);  // L12:0x002e1be0 (hoven_snow_critter.cpp)
void hov_class240_update_l12(uint32_t a0);  // L12:0x002e3ed8 (hoven_jet_platform.cpp)
void hov_class282_update_l12(uint32_t a0);  // L12:0x002e6c48 (hoven_raritanium_npc.cpp)
void hov_class293_update_l12(uint32_t a0);  // L12:0x002e7208 (hoven_sky_objects.cpp)
void critter_walk_anim(uint32_t m);                       // L12:0x002e18e8
void critter_snow_burst(uint32_t at);                     // L12:0x002e19a8
void critter_to_floor(uint32_t m);                        // L12:0x002e1878
uint32_t critter_walk(uint32_t m, float yaw);             // L12:0x002e2eb8
void critter_debris(uint32_t m);                          // L12:0x002e3098
void critter_hits(uint32_t m);                            // L12:0x002e3528
void jet_platform_jets(uint32_t m);                       // L12:0x002e3948
void jet_platform_move(uint32_t m, uint32_t ang, uint32_t target, uint32_t ease);  // L12:0x002e3c08
void jet_platform_tilt(uint32_t m, uint32_t out, uint32_t on);                     // L12:0x002e3e28
void moby_carry_at_joint(uint32_t m, uint32_t child, uint32_t joint);              // L12:0x0027b9c0

// ---- hand-written (task P12.2): classes 294, 326, 328, 336, 339, 384, 409, 422, 1259, 1267, their helpers, and hooks 0x2e1848 / 0x2e6b00
void hov_class294_update_l12(uint32_t m);   // L12:0x002e72c0 (hoven_trooper.cpp)
void hov_class326_update_l12(uint32_t m);   // L12:0x002e9f68 (hoven_gunship.cpp)
void hov_class328_update_l12(uint32_t m);   // L12:0x002eb570 (hoven_hydropack_npc.cpp)
void hov_class336_update_l12(uint32_t m);   // L12:0x002ebea8 (hoven_targets.cpp)
void hov_class339_update_l12(uint32_t m);   // L12:0x002ec1d0 (hoven_targets.cpp)
void hov_class384_update_l12(uint32_t m);   // L12:0x002ec720 (hoven_targets.cpp)
void hov_class409_update_l12(uint32_t m);   // L12:0x002ece00 (hoven_gunship.cpp)
void hov_class422_update_l12(uint32_t m);   // L12:0x002ed280 (hoven_targets.cpp)
void hov_class1259_update_l12(uint32_t m);  // L12:0x00302f30 (hoven_turret.cpp)
void hov_class1267_update_l12(uint32_t m);  // L12:0x00303540 (hoven_turret.cpp)
uint32_t hov_hook_2e1848_l12(uint32_t m);                                     // L12:0x002e1848 (hoven_carry.cpp)
int32_t hov_hook_2e6b00_l12(uint32_t m, uint32_t a1, uint32_t a2, float f12); // L12:0x002e6b00 (hoven_carry.cpp)
uint32_t trooper_in_cone(uint32_t m, uint32_t pt, float range, float max_dz, float half_cone);  // L12:0x0027b8e0
void trooper_death_burst(uint32_t m, uint32_t pos);                                             // L12:0x002e7290
void trooper_retire(uint32_t m);                                                                // L12:0x002e86d0
void trooper_hits(uint32_t m);                                                                  // L12:0x002e81e0
void trooper_aim_joints(uint32_t m, uint32_t target);                                           // L12:0x002e8608
void trooper_gun_update(uint32_t m);                                                            // L12:0x002e8720
void trooper_to_floor(uint32_t m);                                                              // L12:0x002e87b0
uint32_t trooper_walk_step(uint32_t m, uint32_t rec);                                           // L12:0x00271dd8
void trooper_walk_toward(uint32_t m, uint32_t target, float yaw_offset, float speed_override);  // L12:0x002e8808
void gunship_path_lengths(int32_t path_idx);                                                    // L12:0x0027ba70
void gunship_tilt(uint32_t m);                                                                  // L12:0x002eae00
uint32_t gunship_follow_path(uint32_t m, int32_t path_idx);                                     // L12:0x002eaf68
void gunship_hits(uint32_t m);                                                                  // L12:0x002eb220
uint32_t gunship_rocket_spawn(uint32_t vel, uint32_t pos, uint32_t owner, int32_t life, float yaw, float speed, float f14);  // L12:0x002ecce8
void target336_hits(uint32_t m, uint32_t off_screen);                                           // L12:0x002ebca8
void beam1259_arcs(uint32_t m);                                                                 // L12:0x00302c58
void turret_hero_pose(uint32_t m);                                                              // L12:0x003032e8
void turret_lock_on(uint32_t m);                                                                // L12:0x00303370

}  // namespace rac1

#endif
