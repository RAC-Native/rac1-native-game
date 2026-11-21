// Three glows at a moby's pvar points in its own colour, the draw callback of class 44 (Quartu, Drek's fleet) and class
// 1382 (Drek's fleet): one port of the levels' copies (shared_three_glows.cpp). Phase 1 of stage 4: the levels' generated
// copies still run; these are proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_THREE_GLOWS_H
#define RAC1_GAME_OVL_SHARED_THREE_GLOWS_H

#include <cstdint>

namespace rac1 {

// The glows at the pvar points +`first`, +`first` + 0x10, +`first` + 0x20.
void three_glows_draw(uint32_t m, uint32_t first);
void class44_glows_draw(uint32_t m);    // L15:0x0029aba8 L17:0x002a2aa8 (points at pvar +0x2a0)
void class1382_glows_draw(uint32_t m);  // L17:0x002f0448 (points at pvar +0x200)

}  // namespace rac1

#endif
