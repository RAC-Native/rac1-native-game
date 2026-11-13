// The flame spray of Aridia's and Kalebo III's flamethrowers (shared_flame_spray.cpp: one port for both levels' copies).
// Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_FLAME_SPRAY_H
#define RAC1_GAME_OVL_SHARED_FLAME_SPRAY_H

#include <cstdint>

namespace rac1 {

// A spray's record: seven flames that burn (+0 .. +0x18, particles of type 0xc; 0 an empty slot), its smoke's timer +0x1c (s16).
void flame_spray_clear(uint32_t rec);                                // L2:0x00264e40 L16:0x0025ea88
// the flames of `pos` (EE vector) along `dir` (EE vector), `len` long (cut short by the walls); every fourth frame one kept
void flame_spray(uint32_t rec, uint32_t pos, uint32_t dir, float len);  // L2:0x00264e70 L16:0x0025eab8
// the kept flames burn the mobys they touch (mobys_in_sphere with `ignore` and the record `hit`); the spent ones let go
void flame_spray_burn(uint32_t rec, uint32_t ignore, uint32_t hit);  // L2:0x002651d0 L16:0x0025ee18

}  // namespace rac1

#endif
