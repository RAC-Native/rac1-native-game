// Blarg's Nebula G34 (level 6) own moby classes hand-ported from level 6's overlay (`// orig: L6:0x00xxxxxx`): the class
// updates, registered in blarg_mobys.cpp under the names the generated code had, and their helpers (for the oracle's
// checks, batch 117). blarg_doors.cpp: classes 55 and 367; blarg_flame_jet.cpp: class 911; blarg_bridge.cpp: class 1028;
// blarg_barrier.cpp: class 1035; blarg_creature.cpp: class 1048 (part 1); blarg_groups.cpp: the shared helpers,
// blarg_snagglebeast.cpp: class 1051, blarg_station_props.cpp: classes 1054, 1062, 1066, blarg_hydro_station.cpp: class
// 1061, blarg_tank.cpp: class 1083 (part 2); blarg_scene_props.cpp: classes 1091 and 1551, blarg_scientist.cpp: class 1105,
// blarg_countdown.cpp: class 1108, blarg_ship.cpp: class 1109, blarg_valve_wheel.cpp: class 1118, blarg_electric_arc.cpp:
// class 1123, blarg_help_voices.cpp: class 1348, blarg_smoke_sparks.cpp: class 1512, blarg_engine_l6.cpp: the level's own
// builds of two engine functions (part 3; the draws: blarg_part2.h).
#ifndef RAC1_GAME_OVL_BLARG_OWN_H
#define RAC1_GAME_OVL_BLARG_OWN_H

#include <cstdint>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// ---- the landing bay's doors and a sliding panel (blarg_doors.cpp)
void blg_class55_update_l6(uint32_t m);   // L6:0x002b4770  class 55
void blg_class367_update_l6(uint32_t m);  // L6:0x002d9d10  class 367

// ---- the flame jet (blarg_flame_jet.cpp)
void blg_class911_update_l6(uint32_t m);      // L6:0x002f3ad8  class 911
void flame_jet_particles_l6(uint32_t m);      // L6:0x002f3d70
void flame_jet_sweep_hits_l6(uint32_t m);     // L6:0x002f4130
void flame_jet_burst_hits_l6(uint32_t m);     // L6:0x002f4390

// ---- the bridge (blarg_bridge.cpp)
void blg_class1028_update_l6(uint32_t m);     // L6:0x002f55a0  class 1028
void bridge_place_pieces_l6(uint32_t m);      // L6:0x002f5d78
void hero_enter_idle_l6();                    // L6:0x00239528  (Blarg's build of Veldin's 0x227638; class 1109's call glued)

// ---- the electric barrier (blarg_barrier.cpp)
void blg_class1035_update_l6(uint32_t m);     // L6:0x002f6470  class 1035
void barrier_bolts_l6(uint32_t m);            // L6:0x002f7000

// ---- the creature (blarg_creature.cpp)
void blg_class1048_update_l6(uint32_t m);     // L6:0x002f7d78  class 1048
void creature_frame_l6(uint32_t m);           // L6:0x002f91e8
void creature_pack_alert_l6(uint32_t m);      // L6:0x002f9698
float creature_heading_l6(uint32_t m);        // L6:0x002f96f8

// ---- part 2 (P6.2): the shared helpers (blarg_groups.cpp)
void panels_slide_out_l6(int32_t group);            // L6:0x002d9e08
void panels_slide_back_l6(int32_t group);           // L6:0x002d9e78
uint32_t group_walkers_down_l6(int32_t group);      // L6:0x002e9ea0
void door_set_open_l6(uint32_t m);                  // L6:0x002f5360
void door_set_shut_l6(uint32_t m);                  // L6:0x002f53e8
void door_set_reset_l6(uint32_t m);                 // L6:0x002f54a0
void group_put_away_l6(int32_t group);              // L6:0x002f9948
void group_bring_back_l6(int32_t group);            // L6:0x002f99b0
uint32_t creature_let_out_l6(uint32_t m, uint32_t from);  // L6:0x002e9d10

// ---- the Snagglebeast (blarg_snagglebeast.cpp)
void blg_class1051_update_l6(uint32_t m);           // L6:0x002f9a28  class 1051
void beast_frame_l6(uint32_t m);                    // L6:0x002fa9c0
uint32_t beast_rise_camera_l6(uint32_t m);          // L6:0x002fb2a8
void beast_circle_l6(uint32_t m);                   // L6:0x002fbc88
void beast_drop_pack_l6(uint32_t m);                // L6:0x002fb090

// ---- the station's props (blarg_station_props.cpp) and the water lift (blarg_hydro_station.cpp)
void blg_class1054_update_l6(uint32_t m);           // L6:0x002fbfb0  class 1054
void blg_class1062_update_l6(uint32_t m);           // L6:0x002fd088  class 1062
void blg_class1066_update_l6(uint32_t m);           // L6:0x002fda30  class 1066
void blg_class1061_update_l6(uint32_t m);           // L6:0x002fc640  class 1061

// ---- the glass tank (blarg_tank.cpp)
void blg_class1083_update_l6(uint32_t m);           // L6:0x002ffcb0  class 1083
void tank_break_l6(uint32_t m, bool debris);        // L6:0x00300278

// ---- part 3 (P6.3): the floating pieces and the scene's smoke (blarg_scene_props.cpp), the Grindboots' scientist
// (blarg_scientist.cpp), the help voices (blarg_help_voices.cpp)
void blg_class1091_update_l6(uint32_t m);  // L6:0x00300df0  class 1091 (and 1092 .. 1098, 1103)
void blg_class1551_update_l6(uint32_t m);  // L6:0x00309348  class 1551
void blg_class1105_update_l6(uint32_t m);  // L6:0x00301070  class 1105
void blg_class1348_update_l6(uint32_t m);  // L6:0x003083b0  class 1348

// ---- the countdown (blarg_countdown.cpp)
void blg_class1108_update_l6(uint32_t m);          // L6:0x00301b90  class 1108
void group_hold_creatures_l6(int32_t group);       // L6:0x002e9e30
void group_restart_l6(int32_t group);              // L6:0x002f45a0
void group_wait_l6(int32_t group);                 // L6:0x002f45f0
void group_hold_l6(int32_t group);                 // L6:0x002ffc38
void countdown_voice_l6(uint32_t m, int32_t which);  // L6:0x00302400
void moby_set_off_l6(uint32_t m);                  // L6:0x003047b8
void smoke_sources_start_l6(int32_t group);        // L6:0x00309240

// ---- Ratchet's ship (blarg_ship.cpp)
void blg_class1109_update_l6(uint32_t m);                                                    // L6:0x00302578  class 1109
void ship_offer_l6(uint32_t m);                                                              // L6:0x00302fd8
void ship_explosion_l6(uint32_t m, uint32_t pos, uint32_t dir, int32_t count, float scale);  // L6:0x00303748
void ship_piece_launch_l6(uint32_t piece, uint32_t vel, uint32_t spin);                      // L6:0x00300f70
void ship_blast_group_l6(uint32_t m);                                                        // L6:0x00303b68
bool ship_flight_l6(uint32_t m, int32_t route);                                              // L6:0x00303160
void ship_pieces_retire_l6(uint32_t m);                                                      // L6:0x00304028

// ---- the valve wheel (blarg_valve_wheel.cpp), the electric arc (blarg_electric_arc.cpp), smoke and sparks
// (blarg_smoke_sparks.cpp)
void blg_class1118_update_l6(uint32_t m);  // L6:0x00304098  class 1118
void wheel_burst_l6(uint32_t at);          // L6:0x00304818
void blg_class1123_update_l6(uint32_t m);  // L6:0x003049f8  class 1123
void arc_hit_test_l6(uint32_t m);          // L6:0x00305000
void blg_class1512_update_l6(uint32_t m);  // L6:0x00308c68  class 1512

// ---- level 6's own builds of two small engine functions (blarg_engine_l6.cpp)
// Blarg's build of level_sound_play (Veldin's 0x28ddd8 without the volume): the level's sound `idx` (+ the level's base
// 0x15f574, below the count 0x15f5f0) started on the moby `moby` with the flags `flags`; its slot (-1 none)
int32_t level_sound_play_l6(int32_t idx, uint32_t flags, uint32_t moby);  // L6:0x0029aec0
// class hook +0x10 of level 6's class 827 copies: the moby's pvars + 0x60 (0 without a moby or pvars)
uint32_t class_hook_pvars60_l6(uint32_t m);  // L6:0x002ea800

// orig: (helper) repeated in L6:0x002f3ad8 / 0x002f55a0 / 0x002f6470 -- the moby `owner`'s sound in the sound slot `slot`
// (-1: none) stopped while that slot still plays it (its owner +0x88, playing +0x74)
inline void blarg_sound_stop(uint32_t owner, int32_t slot) {
    if (slot == -1) return;
    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
    if (lwu(e + 0x88) == owner && lbu(e + 0x74) != 0) sub_0028d918(slot);
}

}  // namespace rac1

#endif
