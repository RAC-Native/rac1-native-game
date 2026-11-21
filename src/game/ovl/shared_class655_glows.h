// Class 655's two glows (Quartu, Drek's fleet): a helper of its draw callback, two quads facing the camera at its pvar point
// +0xf0, pulled towards the camera, their sizes a little random; one port of the two levels' copies
// (shared_class655_glows.cpp). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by
// oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS655_GLOWS_H
#define RAC1_GAME_OVL_SHARED_CLASS655_GLOWS_H

#include <cstdint>

namespace rac1 {

void class655_glows(uint32_t m);  // L15:0x002d8e48 L17:0x002d74c0
// Veldin's finale's build (task F11): the glows at the moby's place.
void class655_glows_l18(uint32_t m);  // L18:0x002ec130

}  // namespace rac1

#endif
