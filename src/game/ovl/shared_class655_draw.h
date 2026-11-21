// Class 655's draw callback (Quartu, Drek's fleet, Veldin's finale) and the strips of quads it draws: one port of the three
// levels' copies (shared_class655_draw.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved
// against them by oracle batch 517.
#ifndef RAC1_GAME_OVL_SHARED_CLASS655_DRAW_H
#define RAC1_GAME_OVL_SHARED_CLASS655_DRAW_H

#include <cstdint>

namespace rac1 {

// Two strips of quads along the `count` points at `points` (EE vectors): one facing the camera (the level's width, its
// texture scrolled), one about the line seen from the camera (`width`, stretched by `stretch` at its ends); their ends
// faded (their first and last edges in the colours without alpha, the others with `alpha0` / `alpha1`).
void class655_strip(uint32_t points, uint32_t alpha0, uint32_t alpha1, int32_t count, float width, float stretch);  // L15:0x002d8878 L17:0x002d6ef0 L18:0x002ebb60
// The draw: the scroll moved on, two strips of 20 points and four of 5, then the glows.
void class655_draw(uint32_t m);  // L15:0x002d8710 L17:0x002d6d88 L18:0x002eb9f8

}  // namespace rac1

#endif
