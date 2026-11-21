// Class 325, a burning shell that flies free trailing smoke until its fuse or a hit blows it up (Oltanis, Quartu), and its
// glow, a quad facing the camera that Novalis's class 1510 draws too: one port of the levels' copies (shared_class325.cpp).
// Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS325_H
#define RAC1_GAME_OVL_SHARED_CLASS325_H

#include <cstdint>

namespace rac1 {

void class325_update(uint32_t m);  // L14:0x002de670 L15:0x002ca240
// The glow's draw callback (queued by class 325's update every other frame or so; Novalis's class 1510's own).
void class325_draw(uint32_t m);    // L1:0x0030bfa8 L14:0x002dec40 L15:0x002ca810

}  // namespace rac1

#endif
