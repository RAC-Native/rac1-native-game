// Class 1356, a carrier that flies along a path, drops its four class 638 drones over the hero and fires class 50's homing
// shots (Kalebo III, Veldin), and its helpers: one port of the two levels' copies (shared_class1356.cpp). Phase 1 of stage
// 4: the levels' generated copies still run; this is proved against them by oracle batch 505.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1356_H
#define RAC1_GAME_OVL_SHARED_CLASS1356_H

#include <cstdint>

namespace rac1 {

void class1356_update(uint32_t m);  // L16:0x002e22d8 L18:0x002f0920
void class1356_hits(uint32_t m);    // L16:0x002e2b60 L18:0x002f11a8
// one step along the path `path` (its record: +0 the point count, the points from +0x10); 1 at its end
int32_t class1356_follow(uint32_t m, uint32_t path);  // L16:0x002e2e88 L18:0x002f14d0
// each point's heading towards the next (its w), the last one's the one before's
void class1356_path_headings(uint32_t path);  // L16:0x002e2fa0 L18:0x002f15e8
void class1356_joint(uint32_t m);   // L16:0x002e3050 L18:0x002f1698
// a drone (class 638) attached to `carrier` at `offset`, turned by `turn`'s z (EE vectors)
void class1356_attach(uint32_t drone, uint32_t carrier, uint32_t offset, uint32_t turn);  // L16:0x002d3dc0 L18:0x002dd9e8
// an attached drone let go (state 1: back to its post), its timer `frames`
void class1356_release(uint32_t drone, int32_t frames);  // L16:0x002d3de0 L18:0x002dda08

}  // namespace rac1

#endif
