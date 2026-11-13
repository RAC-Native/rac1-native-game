// Small helpers several of the levels 1, 7, 14, 15 and 17 carry a copy of (shared_level_helpers.cpp). Phase 1 of stage 4:
// the levels' generated copies still run; these are proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_LEVEL_HELPERS_H
#define RAC1_GAME_OVL_SHARED_LEVEL_HELPERS_H

#include <cstdint>

namespace rac1 {

// A spark (particle 0x3d) at the moby's chain point `joint`, pulled `pull` towards the camera, of size `size`; the
// particle (0: none free).
uint32_t chain_spark_spawn(uint32_t m, int32_t joint, float size, float pull);  // L7:0x0029b8d8 L15:0x00262d68 L17:0x0026cf30
// The moby held at 0x13e030 (if any) drawn again: its +0x34 bits 0 / 1 cleared, its model (+0x94) its class's.
void held_moby_show();  // L1:0x002a2480 L14:0x00294d38

}  // namespace rac1

#endif
