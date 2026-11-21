// Particle type 0x28's update (Veldin 0x26ff10, slot 0x1b1f20 of the particle table 0x1b1e80), hand-ported in
// veldin_particle_fx.cpp.
#ifndef RAC1_GAME_OVL_PARTICLE_28_TR_H
#define RAC1_GAME_OVL_PARTICLE_28_TR_H

#include <cstdint>

namespace rac1 {

void particle_28_update(uint32_t p);  // 0x0026ff10 (particle type 0x28)

}  // namespace rac1

#endif
