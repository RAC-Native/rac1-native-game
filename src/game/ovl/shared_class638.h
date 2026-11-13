// Class 638, a hovering gun drone that patrols a path and fires class 49's shots (Kalebo III, Veldin), and its helpers: one
// port of the two levels' copies (shared_class638.cpp). Phase 1 of stage 4: the levels' generated copies still run; this
// is proved against them by oracle batch 505.
#ifndef RAC1_GAME_OVL_SHARED_CLASS638_H
#define RAC1_GAME_OVL_SHARED_CLASS638_H

#include <cstdint>

namespace rac1 {

void class638_update(uint32_t m);  // L16:0x002d2cf0 L18:0x002dc918
void class638_hits(uint32_t m);    // L16:0x002d38b0 L18:0x002dd4d8
// moves its hover point towards `to` (an EE vector); the distance it was
float class638_seek(uint32_t m, uint32_t to);  // L16:0x002d3eb8 L18:0x002ddae0
void class638_turn(uint32_t m);    // L16:0x002d4070 L18:0x002ddc98
void class638_burst(uint32_t m);   // L16:0x002d41f8 L18:0x002dde20
void class638_aim(uint32_t m);     // L16:0x002d4590 L18:0x002de1b8
void class638_sparks(uint32_t m);  // L16:0x002d4908 L18:0x002de530
// its group's (or its own) next look-round timer, `lo` .. `hi` frames
void class638_group_timers(uint32_t m, int32_t lo, int32_t hi);  // L16:0x002d3f78 L18:0x002ddba0

}  // namespace rac1

#endif
