// The electric arcs of Eudora's and Batalia's machines (shared_arcs.cpp: one port for both levels' copies; the arc's sparks
// are another build on each level, the difference explicit). Phase 1 of stage 4: the levels' generated copies still run;
// these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_ARCS_H
#define RAC1_GAME_OVL_SHARED_ARCS_H

#include <cstdint>

namespace rac1 {

// the moby's fourteen arcs between pairs of its points (L4:0x002c46d0, L8:0x002da8d8)
void arcs_update(uint32_t m);
// one arc of `count` sparks between the moby's points `a` and `b` (L4:0x002c45b8, L8:0x002da7c0)
void arc_between(uint32_t m, int32_t a, int32_t b, int32_t count);
// the arc's sparks from `from` to `to` (EE vectors), `count` of them, widths `w0` .. `w1` (L4:0x002c41e0, L8:0x002da3f0)
void arc_sparks(uint32_t m, uint32_t from, uint32_t to, int32_t count, float w0, float w1);

}  // namespace rac1

#endif
