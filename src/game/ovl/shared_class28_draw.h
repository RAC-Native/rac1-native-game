// Class 28's two draw callbacks on Oltanis and Quartu (its update queues them): its five glows and the glow at its place,
// each a quad facing the camera; one port of the two levels' copies (shared_class28_draw.cpp). Phase 1 of stage 4: the
// levels' generated copies still run; these are proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS28_DRAW_H
#define RAC1_GAME_OVL_SHARED_CLASS28_DRAW_H

#include <cstdint>

namespace rac1 {

// The five glows at the points pvars +0x1e0 .. +0x220.
void class28_draw_glows(uint32_t m);  // L14:0x002b2928 L15:0x00296710
// The glow at the moby's place, drawn towards the camera.
void class28_draw_spot(uint32_t m);   // L14:0x002b3a20 L15:0x00297808

}  // namespace rac1

#endif
