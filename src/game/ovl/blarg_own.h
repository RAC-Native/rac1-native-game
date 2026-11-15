// Blarg's Nebula G34 (level 6) own moby classes hand-ported from level 6's overlay (`// orig: L6:0x00xxxxxx`): the class
// updates, registered in blarg_mobys.cpp under the names the generated code had, and their helpers (for the oracle's
// checks, batch 117). blarg_doors.cpp: classes 55 and 367; blarg_flame_jet.cpp: class 911; blarg_bridge.cpp: class 1028;
// blarg_barrier.cpp: class 1035; blarg_creature.cpp: class 1048 (part 1); blarg_groups.cpp: the shared helpers,
// blarg_snagglebeast.cpp: class 1051, blarg_station_props.cpp: classes 1054, 1062, 1066, blarg_hydro_station.cpp: class
// 1061, blarg_tank.cpp: class 1083 (part 2).
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

// ---- part 2 (P6.2): the shared helpers (blarg_groups.cpp; class 1105's and class 1108's generated code calls the ones
// marked "glued")
void panels_slide_out_l6(int32_t group);            // L6:0x002d9e08
void panels_slide_back_l6(int32_t group);           // L6:0x002d9e78
uint32_t group_walkers_down_l6(int32_t group);      // L6:0x002e9ea0  (glued)
void door_set_open_l6(uint32_t m);                  // L6:0x002f5360  (glued)
void door_set_shut_l6(uint32_t m);                  // L6:0x002f53e8  (glued)
void door_set_reset_l6(uint32_t m);                 // L6:0x002f54a0
void group_put_away_l6(int32_t group);              // L6:0x002f9948  (glued)
void group_bring_back_l6(int32_t group);            // L6:0x002f99b0  (glued)
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

// orig: (helper) repeated in L6:0x002f3ad8 / 0x002f55a0 / 0x002f6470 -- the moby `owner`'s sound in the sound slot `slot`
// (-1: none) stopped while that slot still plays it (its owner +0x88, playing +0x74)
inline void blarg_sound_stop(uint32_t owner, int32_t slot) {
    if (slot == -1) return;
    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
    if (lwu(e + 0x88) == owner && lbu(e + 0x74) != 0) sub_0028d918(slot);
}

}  // namespace rac1

#endif
