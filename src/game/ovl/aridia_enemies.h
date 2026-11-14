// Aridia's own moby classes, hand-ported from Aridia's overlay (`// orig: L2:0x00xxxxxx`), one file each (aridia_<class>.cpp):
// the class updates and the Sand Shark's hooks (registered in aridia_mobys.cpp / aridia_hooks.cpp under the names the
// generated code had), and their private helpers (for the oracle's direct checks, batches 109 and 137).
#ifndef RAC1_GAME_OVL_ARIDIA_ENEMIES_H
#define RAC1_GAME_OVL_ARIDIA_ENEMIES_H

#include <cstdint>

namespace rac1 {

// ---- the Sand Shark (aridia_sand_shark.cpp)
void sand_shark_update_l2(uint32_t m);  // L2:0x002d3e50  class 580
uint32_t shark_hook_14_l2(uint32_t m);  // L2:0x002d6340  its class hook +0x14 (the result: none, the callers drop it)
void shark_sand_line_l2(uint32_t from, uint32_t to, int32_t count, float size_lo, float size_hi);  // L2:0x002d3a50
void shark_sand_edge_l2(uint32_t m, uint32_t i, uint32_t j, int32_t count);                       // L2:0x002d3c98
void shark_sand_burst_l2(uint32_t m);                                                              // L2:0x002d3dd0
uint32_t shark_group_mate_l2(uint32_t m);                                                          // L2:0x002d5658
void shark_group_alert_l2(uint32_t m);                                                             // L2:0x002d5728
void shark_sand_spray_l2(uint32_t pos, int32_t count);                                             // L2:0x002d57f8
void shark_sand_puff_l2(uint32_t pos);                                                             // L2:0x002d5a68
void shark_frame_l2(uint32_t m);                                                                   // L2:0x002d5c88
uint32_t shark_path_check_l2(uint32_t m);                                                          // L2:0x002d63c0

// ---- the Constructorbot (aridia_constructorbot.cpp)
void constructorbot_update_l2(uint32_t m);  // L2:0x002d7748
void constructorbot_hits_l2(uint32_t m);  // L2:0x002d85b8

// ---- the elevator (aridia_elevator.cpp)
void elevator_update_l2(uint32_t m);  // L2:0x002dbd38

// ---- the Sand Shark's hook +0x10 and its egg (aridia_sand_shark.cpp)
uint32_t shark_hook_10_l2(uint32_t m);  // L2:0x002d6310  its carry record (pvar + 0x60)
void shark_egg_update_l2(uint32_t m);   // L2:0x002e0280  class 755 (nothing on Aridia spawns it)

// ---- the nest (aridia_nest.cpp)
void nest_update_l2(uint32_t m);                                        // L2:0x002dcb38
void nest_hits_l2(uint32_t m);                                          // L2:0x002dcfe0
uint32_t nest_find_shark_l2(uint32_t m);                                // L2:0x002dd2a8
void nest_shark_launch_l2(uint32_t shark, uint32_t pos, uint32_t vel);  // L2:0x002d6118

// ---- the rocks (aridia_rocks.cpp)
void rocks_update_l2(uint32_t m);  // L2:0x002e0720

// ---- the Rocket Launcher (aridia_rocket_launcher.cpp)
void rocket_launcher_update_l2(uint32_t m);                                  // L2:0x002df3d8
void launcher_sound_l2(uint32_t m);                                          // L2:0x002df730
uint32_t launcher_rocket_spawn_l2(uint32_t m, uint32_t pos, uint32_t dir);   // L2:0x002ec228  class 1208 (the oracle's generator uses it)

// ---- Skid and his agent (aridia_skid.cpp)
void skid_update_l2(uint32_t m);        // L2:0x002e1950
void skid_agent_update_l2(uint32_t m);  // L2:0x002e0dc0
void skid_race_start_l2(uint32_t m);    // L2:0x002e1fb8

// ---- class 1324, the hint giver (aridia_class1324.cpp), and its hint helpers (also Skid's agent's)
void class1324_update_l2(uint32_t m);                          // L2:0x002ee890
int32_t hint_play_steps();                                     // (inlined in the original)
bool hint_due(uint32_t last);                                  // (inlined in the original)
void hint_seen(uint32_t last);                                 // (inlined in the original)
void hint_visit(uint32_t rec, int32_t text, int32_t slot);     // (inlined in the original)

// ---- class 1479, the steam vent (aridia_class1479.cpp)
void class1479_update_l2(uint32_t m);  // L2:0x002ef020

// ---- the transport pod (aridia_transport_pod.cpp)
void transport_pod_update_l2(uint32_t m);                // L2:0x002ddc88
void pod_doors_seek_l2(uint32_t m);                      // L2:0x002def80
void pod_doors_snap_l2(uint32_t m);                      // L2:0x002df0b0
void pod_hero_out_l2();                                  // L2:0x00237478
void pod_camera_mode_l2(uint32_t mode, uint32_t t);      // L2:0x002f8a18

// ---- the liftcar's platform (aridia_liftcar.cpp)
void liftcar_platform_update_l2(uint32_t m);  // L2:0x002e2228

}  // namespace rac1

#endif
