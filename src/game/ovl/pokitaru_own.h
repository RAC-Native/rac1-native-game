// Pokitaru's (level 11) own classes ported by hand (part 1: the resort owner, Bob and the talker of class 298, the
// teleporter, the raft, the great gate and three small props): their helpers. The class updates keep their entries'
// declarations in pokitaru_mobys.h.
#ifndef RAC1_GAME_OVL_POKITARU_OWN_H
#define RAC1_GAME_OVL_POKITARU_OWN_H

#include <cstdint>

namespace rac1 {

// ---- the resort owner (class 114): pokitaru_resort_owner.cpp
void pok_2cb990_l11(uint32_t gate);                  // L11:0x002cb990 opens a class-65 gate (and its twin)
bool pok_30a480_l11(uint32_t m);                     // L11:0x0030a480 the hero lets go of the switch he holds
void pok_2d1030_l11(uint32_t m);                     // L11:0x002d1030 the moves of its states
void pok_2d1340_l11(uint32_t m);                     // L11:0x002d1340 the walk and the walls
void pok_2d14b0_l11(uint32_t m);                     // L11:0x002d14b0 its states (the talks, the walk, the O2 Mask)
void pok_2d2088_l11(uint32_t m, uint32_t state);     // L11:0x002d2088 a state entered: its animation and goal
bool pok_2d2340_l11(uint32_t m, uint32_t to);        // L11:0x002d2340 a way to `to` round its zone's walls
int32_t pok_2d2460_l11(uint32_t m);                  // L11:0x002d2460 an enemy of its zone near (2) or alive (1)
bool pok_2d25e8_l11(uint32_t m, uint32_t pos);       // L11:0x002d25e8 a free spot in front of it (into `pos`)
void pok_2d27b0_l11(uint32_t m);                     // L11:0x002d27b0 its head turned toward the hero
bool pok_316128_l11(uint32_t m);                     // L11:0x00316128 an enemy of class 1246 in a harmless state

// ---- the teleporter (class 318): pokitaru_teleporter.cpp
void pok_2f2cd0_l11(uint32_t m);    // L11:0x002f2cd0 its prompt
void pok_2f3040_l11(uint32_t m);    // L11:0x002f3040 it and its three pieces shown

// ---- the raft (class 1075): pokitaru_raft.cpp
void pok_30a500_l11(uint32_t m);    // L11:0x0030a500 its paddle wheels and their spray
bool pok_30a850_l11(uint32_t m);    // L11:0x0030a850 its zone's enemies all gone (or harmless)

// ---- the great gate (class 1156): pokitaru_gate.cpp
void pok_30d1b0_l11(uint32_t m);    // L11:0x0030d1b0 its eighteen parts placed

// ---- the sewer iris gate (class 1157): pokitaru_sewer.cpp
void pok_30dd18_l11(uint32_t m);    // L11:0x0030dd18 its twenty-four blades placed

// ---- the parachute drops and the energy-ball thrower (classes 1218, 1219, 1231): pokitaru_crates.cpp
uint32_t pok_30fd98_l11(uint32_t m);                                  // L11:0x0030fd98 spawns a crate's parachute (class 1219)
bool pok_30f660_l11(uint32_t m, uint32_t pv);                         // L11:0x0030f660 updates a crate's parachute attachment
void pok_30fe50_l11(uint32_t m, uint32_t pv, uint32_t hp);            // L11:0x0030fe50 a parachute's hit reaction and colour pulse
void pok_318e78_l11(uint32_t shot, uint32_t pos);                     // L11:0x00318e78 holds a charging energy ball at `pos`
uint32_t pok_318ed8_l11(uint32_t shot);                               // L11:0x00318ed8 spawns a spark converging on `shot`
uint32_t pok_318d10_l11(uint32_t m, uint32_t pos, int32_t frames);    // L11:0x00318d10 spawns an energy ball (class 1297) and its sparks
void pok_318e98_l11(uint32_t shot, uint32_t vel);                     // L11:0x00318e98 launches an energy ball with velocity `vel`
void pok_310698_l11(uint32_t m);                                      // L11:0x00310698 class 1231's hit reaction and target search

// ---- the fighter jet (class 1242): pokitaru_jet.cpp
void pok_3125a8_l11(uint32_t m, uint32_t pv, float yaw, float pitch);                                           // L11:0x003125a8 machine guns and crosshair
uint32_t pok_309378_l11(uint32_t m, uint32_t pos, uint32_t target, uint32_t rot, uint32_t life, float speed);   // L11:0x00309378 spawns a homing missile (class 1034)
void pok_3126f8_l11(uint32_t m, uint32_t pv);                                                                   // L11:0x003126f8 fires a missile
void pok_313f60_l11(uint32_t m, uint32_t steer_x, uint32_t steer_y);                                            // L11:0x00313f60 steers back into the flight bounds
void pok_312838_l11(uint32_t m, uint32_t pv);                                                                   // L11:0x00312838 flight, camera, collisions and weapons
int32_t pok_31a758_l11(int32_t group);                                                                          // L11:0x0031a758 counts active bombers in `group`
uint32_t pok_31a7d8_l11(int32_t group);                                                                         // L11:0x0031a7d8 wakes a bomber in `group`

}  // namespace rac1

#endif
