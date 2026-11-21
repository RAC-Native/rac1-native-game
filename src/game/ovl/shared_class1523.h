// Class 1523, Giant Clank's charged shot (Quartu, Veldin): charged while the button is held (streaks drawn in, a light that
// grows), then fired along his heading, hurting what it passes; one port of the two levels' copies, and its streak
// (shared_class1523.cpp). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle
// batch 505.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1523_H
#define RAC1_GAME_OVL_SHARED_CLASS1523_H

#include <cstdint>

namespace rac1 {

void class1523_update(uint32_t m);  // L15:0x0029edb0 L18:0x002abaa0
// a streak of the colour `colour` from `from` (an EE vector) a quarter of the way to the moby `m`, following it
void class1523_streak(uint32_t colour, uint32_t from, uint32_t m);  // L15:0x0025e2c0 L18:0x00270af0

}  // namespace rac1

#endif
