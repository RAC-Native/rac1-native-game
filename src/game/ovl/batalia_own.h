// Batalia (level 8) own moby classes hand-ported from level 8's overlay (`// orig: L8:0x00xxxxxx`): the class updates,
// registered in batalia_mobys.cpp under the names the generated code had, and their helpers (for the oracle's checks,
// batch 120). Part 1 (P8.1): batalia_bolt_prize.cpp: class 152; batalia_projectiles.cpp: classes 248 and 425;
// batalia_bomber_tank.cpp: class 253; batalia_sea.cpp: class 327; batalia_grenadier.cpp: class 333; batalia_shuttle.cpp:
// class 424.
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

}  // namespace rac1

#endif
