// Particle types 0x00 (0x268d48), 0x01 (0x268fe8), 0x4d (0x275860), 0x2b (0x2710a0), and 0x4a (0x275510, the
// Sandmouse's) updates, hand-ported in veldin_particle_fx.cpp.
#ifndef RAC1_GAME_OVL_PARTICLE_TYPES2_TR_H
#define RAC1_GAME_OVL_PARTICLE_TYPES2_TR_H

#include <cstdint>

namespace rac1 {

void particle_00_update(uint32_t p);  // 0x00268d48
void particle_01_update(uint32_t p);  // 0x00268fe8
void particle_4d_update(uint32_t p);  // 0x00275860
void particle_2b_update(uint32_t p);  // 0x002710a0
void particle_4a_update(uint32_t p);  // 0x00275510

}  // namespace rac1

#endif
