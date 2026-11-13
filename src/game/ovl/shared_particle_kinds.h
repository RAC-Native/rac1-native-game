// Particle types' update and spawns that Veldin and other levels carry copies of (shared_particle_kinds.cpp: one port for
// every copy). Phase 1 of stage 4: the levels' generated copies are still compiled; Veldin's registration
// (veldin_extras.cpp) runs the update; the Veldin copies of the spawns stay generated (their callers are).
#ifndef RAC1_GAME_OVL_SHARED_PARTICLE_KINDS_H
#define RAC1_GAME_OVL_SHARED_PARTICLE_KINDS_H

#include <cstdint>

namespace rac1 {

// Particle type 0x50 (Drek's fleet's laser beams): it fades its size and colour over its life, and is gone out of the
// level's box or when its life is out.
void beam_particle_update(uint32_t p);  // L0:0x00275eb0 L5:0x002a00f0 L7:0x0029e4a0
// A particle 0x43 at `pos` (EE address) for `life` frames: its owner word `owner`, its two sizes `a` / `b`, its colour's
// low byte `alpha` (0x409b9b<alpha>), its velocity `vel` (an EE address, or 0 for none), `mode` 0 (fading from 127 over its
// life) or not. The particle, or 0.
uint32_t particle_43_spawn(uint32_t owner, uint32_t pos, int32_t life, uint32_t mode, uint32_t alpha, uint32_t vel, float a, float b);  // L0:0x002743a8 L5:0x0029e698 L16:0x0026f7e8
// A particle 0x3a at `pos` of kind `kind` (0 / 1: grey, living `b1` frames; 2: a coloured spark thrown at random, living
// `b5` frames, `speed` scaling its throw) with its bytes `b1` .. `b5` and its word `word` and size `size`. The particle, or 0.
uint32_t particle_3a_spawn(uint32_t pos, uint32_t kind, uint32_t b1, uint32_t b2, uint32_t b3, uint32_t b5, uint32_t word, float size,
                           float speed);  // L0:0x002729c8 L7:0x0029ae58
// A particle 0x28 (a grey puff) at `pos` moving by `vel` (EE addresses), of kind `kind` (below 5: a random size and a
// random part of its life), its word `word`. The particle, or 0.
uint32_t particle_28_spawn(uint32_t pos, uint32_t vel, uint32_t kind, uint32_t word);  // L0:0x0026fd28 L10:0x00262b08

}  // namespace rac1

#endif
