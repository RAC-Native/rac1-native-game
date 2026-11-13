// Class 123's draw callback on Quartu and Drek's fleet (its update queues it): a beam from just off the moby to its target
// point, a quad turned to the camera; one port of the two levels' copies (shared_class123_draw.cpp). Phase 1 of stage 4:
// the levels' generated copies still run; this is proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS123_DRAW_H
#define RAC1_GAME_OVL_SHARED_CLASS123_DRAW_H

#include <cstdint>

namespace rac1 {

void class123_draw(uint32_t m);  // L15:0x002a7628 L17:0x002acf00

}  // namespace rac1

#endif
