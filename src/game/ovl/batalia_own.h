// Batalia (level 8) own moby classes hand-ported from level 8's overlay (`// orig: L8:0x00xxxxxx`): the class updates,
// registered in batalia_mobys.cpp under the names the generated code had, and their helpers (for the oracle's checks,
// batch 120). Part 1 (P8.1): batalia_bolt_prize.cpp: class 152; batalia_projectiles.cpp: classes 248 and 425;
// batalia_bomber_tank.cpp: class 253; batalia_sea.cpp: class 327; batalia_grenadier.cpp: class 333; batalia_shuttle.cpp:
// class 424. Part 2 (P8.2): batalia_missile.cpp: class 435; batalia_fighter.cpp: class 438; batalia_turret.cpp: class 440;
// batalia_warship.cpp: classes 441 and 444; batalia_bolt_thief.cpp: class 452; batalia_gunship.cpp: class 462.
#ifndef RAC1_GAME_OVL_BATALIA_OWN_H
#define RAC1_GAME_OVL_BATALIA_OWN_H

#include <cstdint>

namespace rac1 {

// ---- the bolt prize (batalia_bolt_prize.cpp)
void bat_class152_update_l8(uint32_t m);                               // L8:0x002b82a0  class 152
void bolt_prize_setup_l8(uint32_t m);                                  // L8:0x002b8228
uint32_t bolt_prize_glow_l8(uint32_t m);                               // L8:0x00279f00

// ---- the grenade and the Bomber Tank's shell (batalia_projectiles.cpp)
void bat_class248_update_l8(uint32_t m);  // L8:0x002d23f0  class 248 (the grenade class 333 throws)
void bat_class425_update_l8(uint32_t m);  // L8:0x002dc9a8  class 425 (the shell class 253 fires)

// ---- the Bomber Tank (batalia_bomber_tank.cpp)
void bat_class253_update_l8(uint32_t m);                                    // L8:0x002d42d8  class 253
void bomber_tank_frame_l8(uint32_t m);                                      // L8:0x002d4ca0
void bomber_tank_drive_l8(uint32_t m, int32_t route);                       // L8:0x002d55c8
void bomber_tank_smoke_l8(uint32_t m, uint32_t joint, int32_t count);       // L8:0x002d5950
void bomber_tank_break_wall_l8(uint32_t m);                                 // L8:0x002d5d08
void bomber_tank_wall_gone_l8(uint32_t m);                                  // L8:0x002d6450
uint32_t bomber_tank_bomb_l8(uint32_t m, uint32_t at, uint32_t vel);        // L8:0x002dc8a0

// ---- the sea below the fort (batalia_sea.cpp; its draw L8:0x2da0d0 stays generated with the ocean's chain)
void bat_class327_update_l8(uint32_t m);  // L8:0x002da0f0  class 327

// ---- the grenadier (batalia_grenadier.cpp)
void bat_class333_update_l8(uint32_t m);                                 // L8:0x002dabf0  class 333
uint32_t grenade_spawn_l8(uint32_t thrower, uint32_t at, uint32_t vel);  // L8:0x002d22e0
uint32_t grenadier_next_post_l8(uint32_t m);                             // L8:0x002daa10

// ---- the shuttle (batalia_shuttle.cpp)
void bat_class424_update_l8(uint32_t m);  // L8:0x002dbdb0  class 424
void shuttle_corners_l8(uint32_t m);         // L8:0x002dc0c8
void shuttle_to_nearest_l8(uint32_t m);      // L8:0x002dc180
bool shuttle_ride_l8(uint32_t m);            // L8:0x002dc648
int32_t shuttle_help_l8(uint32_t m);         // L8:0x002dc7f0

// ---- the Blarg missile (batalia_missile.cpp)
void bat_class435_update_l8(uint32_t m);                                                        // L8:0x002dd3c0  class 435
uint32_t bat_class435_spawn_l8(uint32_t at, uint32_t target, uint32_t vel, float f40, float f44);  // L8:0x002de3e0
void missile_rubble_l8(uint32_t m, int32_t box, int32_t count, bool small);                     // L8:0x002de528

// ---- the Blarg fighter (batalia_fighter.cpp)
void bat_class438_update_l8(uint32_t m);                // L8:0x002de848  class 438
void fighter_exhaust_l8(uint32_t m, uint32_t old_pos);  // L8:0x002dec90

// ---- the anti-air turret (batalia_turret.cpp)
void bat_class440_update_l8(uint32_t m);                                 // L8:0x002e0328  class 440
uint32_t turret_aimed_missile_l8(uint32_t m);                            // L8:0x002e0228
void turret_commander_end_l8(uint32_t commander, int32_t outcome);       // L8:0x00306b30

// ---- the gunship pieces and the three-drum Blarg warship (batalia_warship.cpp)
void bat_class441_update_l8(uint32_t m);                                                             // L8:0x002e16c0  class 441
uint32_t bat_piece_spawn_l8(uint32_t parent, uint32_t cls, uint32_t piece_joint, uint32_t parent_joint);  // L8:0x002e1c98
void warship_build_l8(uint32_t m);                                                                   // L8:0x002e1d70
void warship_pieces_follow_l8(uint32_t m);                                                           // L8:0x002e2078
void warship_hits_l8(uint32_t m);                                                                    // L8:0x002e2250
void bat_class444_update_l8(uint32_t m);                                                             // L8:0x002e24e8  class 444

// ---- the Bolt Thief (batalia_bolt_thief.cpp)
void bat_class452_update_l8(uint32_t m);  // L8:0x002e2df0  class 452
void bolt_thief_gather_l8(uint32_t m);    // L8:0x002e2da0

// ---- the one-drum Blarg gunship (batalia_gunship.cpp)
void bat_class462_update_l8(uint32_t m);    // L8:0x002e8cd0  class 462
void gunship_build_l8(uint32_t m);          // L8:0x002e8788
void gunship_hits_l8(uint32_t m);           // L8:0x002e88d8
void gunship_pieces_follow_l8(uint32_t m);  // L8:0x002e8ba0

// ---- the winged Blarg gunship (batalia_gunship2.cpp)
void bat_class463_update_l8(uint32_t m);     // L8:0x002e9b70  class 463
void gunship2_build_l8(uint32_t m);          // L8:0x002e9558
void gunship2_hits_l8(uint32_t m);           // L8:0x002e9758
void gunship2_pieces_follow_l8(uint32_t m);  // L8:0x002e9a18

// ---- the fort's gates, drawbridge, scrolling layers, mine, rail triggers, ship smoke and steam vent (batalia_fort.cpp)
void bat_class467_update_l8(uint32_t m);   // L8:0x002ea398  class 467
void bat_class468_update_l8(uint32_t m);   // L8:0x002ea4c0  class 468
void bat_class600_update_l8(uint32_t m);   // L8:0x002f1548  class 600
void bat_class621_update_l8(uint32_t m);   // L8:0x002f44c0  class 621
void bat_class1349_update_l8(uint32_t m);  // L8:0x00307540  class 1349
void bat_class1553_update_l8(uint32_t m);  // L8:0x003084d8  class 1553
void steam_puff_spawn_l8(uint32_t pos, uint32_t vel, int32_t life, float scale);  // L8:0x0027fe00
void bat_class1641_update_l8(uint32_t m);  // L8:0x00308c90  class 1641
void bat_class1649_update_l8(uint32_t m);  // L8:0x003097b0  class 1649

// ---- the Plumber, Deserter, Commando and Commander (batalia_npcs.cpp)
void bat_class774_update_l8(uint32_t m);        // L8:0x002f90c8  class 774
void bat_class1130_update_l8(uint32_t m);       // L8:0x00302ce8  class 1130
void commando_scene_scale_l8(uint32_t m);       // L8:0x00305138
void bat_class1144_update_l8(uint32_t m);       // L8:0x00305270  class 1144
int32_t turret_mount_start_l8(uint32_t turret); // L8:0x002e1698
void commander_scene_scale_l8(uint32_t m);      // L8:0x003063f0
void bat_class1283_update_l8(uint32_t m);       // L8:0x003065d8  class 1283

// ---- the turret HUD, Bolt Thief glow and waving banner (batalia_draws.cpp)
void bat_class440_draw_l8(uint32_t m);   // L8:0x002deee0  class 440 draw
void bat_class452_draw_l8(uint32_t m);   // L8:0x002e5188  class 452 draw
uint32_t banner_spark_spawn_l8(uint32_t owner, uint32_t pos, uint32_t vel, int16_t life, uint8_t r, uint8_t g,
                               uint8_t b, uint8_t a, float size);  // L8:0x0027b450
float banner_wind_sample_l8(uint32_t pv, float t);                 // L8:0x002f5d98
void bat_class671_draw_l8(uint32_t m);                             // L8:0x002f5e58  class 671 draw
void bat_class671_update_l8(uint32_t m);                           // L8:0x002f70a0  class 671

}  // namespace rac1

#endif
