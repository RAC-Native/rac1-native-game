// Particle types 0x47 (Veldin 0x274e48) and 0x4f (0x275d68) updates, hand-ported in veldin_particles_2.cpp,
// and their registration (particle_types4.cpp).
#ifndef RAC1_GAME_OVL_PARTICLE_TYPES4_TR_H
#define RAC1_GAME_OVL_PARTICLE_TYPES4_TR_H

#include <cstdint>

namespace rac1 {

void particle_47_update(uint32_t p);  // 0x00274e48
void particle_4f_update(uint32_t p);  // 0x00275d68

// orig: (none) -- registers them for the indirect calls (the particle table's slots 0x47 / 0x4f)
void particle_types4_register();

}  // namespace rac1

#endif
