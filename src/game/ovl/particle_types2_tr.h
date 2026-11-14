// Particle types 0's (Veldin's 0x268d48), 1's (0x268fe8), 0x4d's (0x275860), 0x2b's (0x2710a0) and 0x4a's (0x275510, the Sandmouse's)
// updates (met on Batalia, level 8), transliterated: the declarations of particle_types2_tr.cpp (regen group particle_types2).
#ifndef RAC1_GAME_OVL_PARTICLE_TYPES2_TR_H
#define RAC1_GAME_OVL_PARTICLE_TYPES2_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

void particle_00_update(uint32_t a0);  // 0x00268d48
void particle_01_update(uint32_t a0);  // 0x00268fe8
void particle_4d_update(uint32_t a0);  // 0x00275860
void particle_2b_update(uint32_t a0);  // 0x002710a0
void particle_4a_update(uint32_t a0);  // 0x00275510

}  // namespace rac1

#endif
