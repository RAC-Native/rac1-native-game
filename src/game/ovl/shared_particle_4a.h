// Particle type 0x4a, a particle that rides on a moby (in the moby's own coordinates), shared by the levels that spawn it:
// one port of the levels' copies of its spawn (shared_particle_4a.cpp).
#ifndef RAC1_GAME_OVL_SHARED_PARTICLE_4A_H
#define RAC1_GAME_OVL_SHARED_PARTICLE_4A_H

#include <cstdint>

namespace rac1 {

// A particle 0x4a at `pos` (a vector's address) on the moby `m`, its velocity `vel`, its look from the record `rec` (+0 the
// word +4, +0xc its life), additive with `additive`; the particle (0: none free).
uint32_t particle_4a_spawn(uint32_t m, uint32_t pos, uint32_t vel, uint32_t rec, uint32_t additive);  // L1:0x0028a7a8 L2:0x002768c8 L3:0x002637f0 L4:0x00267eb0 L5:0x0029f5f0 L6:0x00284190 L8:0x0027f950 L11:0x0029a738 L12:0x0028de98 L13:0x00281100 L14:0x0027d310 L17:0x0026f1a8

}  // namespace rac1

#endif
