// Class 848, a helicopter's burst of gunfire (the helicopters: class 631 on Kerwan and Umbris, class 233 on Quartu): an aim
// point that runs along the ground towards the nearest target while the owner's two guns fire bullets at it by turns, with
// sparks at the guns and where the line of fire meets the scenery, until the owner stops or dies, its timer ends or the
// fire hits the hero; and the bullets' particle. One port of the three levels' copies (shared_class848.cpp). Phase 1 of
// stage 4: the levels' generated copies still run; these are proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS848_H
#define RAC1_GAME_OVL_SHARED_CLASS848_H

#include <cstdint>

namespace rac1 {

void class848_update(uint32_t m);  // L3:0x002d47c0 L7:0x00304ff0 L15:0x002de430
// A bullet (particle 0x31) of `owner`: from `from` to `to`, its velocity `vel`, `life` frames.
void class848_bullet(uint32_t owner, uint32_t to, uint32_t from, uint32_t vel, int32_t life);  // L3:0x00260130 L7:0x0029a018 L15:0x00261560

}  // namespace rac1

#endif
