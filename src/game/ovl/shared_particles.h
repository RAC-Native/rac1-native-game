// Two particle types' updates (shared_particles.cpp: one port for every copy). Phase 1 of stage 4: the levels' generated
// copies are still compiled; Veldin's registration (veldin_extras.cpp) runs these.
#ifndef RAC1_GAME_OVL_SHARED_PARTICLES_H
#define RAC1_GAME_OVL_SHARED_PARTICLES_H

#include <cstdint>

namespace rac1 {

// a bullet: it flies by its velocity, stops where its way hits something, and is gone when its life is out
// (particle type 0x31: Veldin 0x271d20, L5:0x0029bda0)
void bullet_particle_update(uint32_t p);
// a particle that only lives its life out (particle types 0x21 / 0x3f: Veldin 0x26f048 / 0x2738b0)
void timed_particle_update(uint32_t p);

}  // namespace rac1

#endif
