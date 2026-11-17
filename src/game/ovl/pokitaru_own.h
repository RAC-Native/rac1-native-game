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

}  // namespace rac1

#endif
