// Class 78's draw callback on Quartu and Drek's fleet (its update queues it): six layers of two textured quads in the
// moby's frame, their texture coordinates swaying with the frame counter; one port of the two levels' copies
// (shared_class78_draw.cpp). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by
// oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS78_DRAW_H
#define RAC1_GAME_OVL_SHARED_CLASS78_DRAW_H

#include <cstdint>

namespace rac1 {

void class78_draw(uint32_t m);  // L15:0x002a3138 L17:0x002a8808

}  // namespace rac1

#endif
