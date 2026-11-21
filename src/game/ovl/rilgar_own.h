// Rilgar's (level 5) own classes ported by hand (part 1: the water monster, the air traffic, the race bots, the
// spitter; part 2: the flame tank, the race rivals, the water): their helpers. The class updates keep their entries' declarations in rilgar_mobys.h.
#ifndef RAC1_GAME_OVL_RILGAR_OWN_H
#define RAC1_GAME_OVL_RILGAR_OWN_H

#include <cstdint>

namespace rac1 {

// ---- the water monster (class 35): rilgar_water_monster.cpp
void ril_2d1e30_l5(uint32_t m);    // L5:0x002d1e30 its wake
void ril_2d1fd8_l5(uint32_t pos);  // L5:0x002d1fd8 a splash at `pos` (the original's first argument unused)

// ---- the air traffic (class 79): rilgar_air_traffic.cpp
void ril_2d7020_l5(uint32_t m);                 // L5:0x002d7020 the running lights
void ril_2d7890_l5(uint32_t m, uint32_t path);  // L5:0x002d7890 the move along `path`

// ---- the race bots (class 133): rilgar_race_bots.cpp
void ril_2daf58_l5(uint32_t m);                // L5:0x002daf58 the hit
void ril_2db018_l5(uint32_t m);                // L5:0x002db018 the puddle rings
void ril_2db238_l5(uint32_t m, uint32_t vel);  // L5:0x002db238 the sparks

// ---- the spitter (class 623): rilgar_spitter.cpp
void ril_303390_l5(uint32_t m);                                          // L5:0x00303390 the frame before its state
void ril_303b08_l5(uint32_t m, uint32_t to, float offset);               // L5:0x00303b08 heads for `to` round the walls
int32_t ril_303e50_l5(uint32_t m, uint32_t to, float r);                 // L5:0x00303e50 the way past `to` clear
uint32_t ril_303f80_l5(uint32_t m, int32_t group);                       // L5:0x00303f80 the nearest of a group
float ril_304058_l5(int32_t path, uint32_t pos, uint32_t out, float r);  // L5:0x00304058 pushed off the walls

// ---- the flame tank (class 625): rilgar_flame_tank.cpp
void ril_305178_l5(uint32_t m);  // L5:0x00305178 the hit, the death, the treads
void ril_305898_l5(uint32_t m);  // L5:0x00305898 the fire's damage

// ---- the race rivals (class 717): rilgar_race_rivals.cpp
uint32_t ril_2f8718_l5(uint32_t m);                                     // L5:0x002f8718 its board spawned
void ril_306d30_l5(uint32_t m, int32_t i, uint32_t out);                // L5:0x00306d30 a point of its path
void ril_306e10_l5(uint32_t m);                                         // L5:0x00306e10 the next point, the side paths
void ril_307358_l5(uint32_t m);                                         // L5:0x00307358 a jump's landing
int32_t ril_307470_l5(int32_t set);                                     // L5:0x00307470 the board's animation set
void ril_307480_l5(uint32_t m, int32_t set, int32_t frame, int32_t frames);  // L5:0x00307480 its animation and the board's
void ril_307570_l5(uint32_t m);                                         // L5:0x00307570 the animations, the tricks

// ---- the water (class 831): rilgar_water.cpp
void ril_30d5c0_l5(uint32_t m);  // L5:0x0030d5c0 its group kept level
void ril_30d6a0_l5(int32_t k);   // L5:0x0030d6a0 a cell's neighbours

}  // namespace rac1

#endif
