// Two families of Veldin's finale's own (level 18) with several copies of the same code: a textured strip's draw and a
// class hook (shared_finale.cpp). Phase 1 of stage 4: the generated copies still run; proved against them by batch 510.
#ifndef RAC1_GAME_OVL_SHARED_FINALE_H
#define RAC1_GAME_OVL_SHARED_FINALE_H

#include <cstdint>

namespace rac1 {

// The draw of mesh `set` of quads (0: L18:0x002f2620, 25 quads; 1: L18:0x002f27c8, 38; 2: L18:0x002f2970, 22) for the moby `m`.
void finale_quads_draw(int32_t set, uint32_t m);
// The hook that stops the moby `m`: its state `state` (5: L18:0x002d6280, 8: L18:0x002fcd00), its animation back to the
// first set, hidden.
void finale_stop_hook(uint32_t m, uint32_t state);

}  // namespace rac1

#endif
