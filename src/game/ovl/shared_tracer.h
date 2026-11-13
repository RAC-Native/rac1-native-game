// The tracer rounds of Batalia and Gaspar (class 1292): shared_tracer.cpp, one port for both levels' copies. Phase 1 of
// stage 4: the levels' generated copies still run; this is proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_TRACER_H
#define RAC1_GAME_OVL_SHARED_TRACER_H

#include <cstdint>

namespace rac1 {

void tracer_update(uint32_t m);  // class 1292 (L8:0x00307298, L9:0x00308f08)

}  // namespace rac1

#endif
