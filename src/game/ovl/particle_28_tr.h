// Particle type 0x28's update (Veldin 0x26ff10, slot 0x1b1f20 of the particle table 0x1b1e80), first reached unported on Orxon
// (L10:0x262cf0), transliterated at Veldin's addresses: the declarations of particle_28_tr.cpp (tools/codegen/gen/mkall/veldin_particle28.json).
#ifndef RAC1_GAME_OVL_PARTICLE_28_TR_H
#define RAC1_GAME_OVL_PARTICLE_28_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

void particle_28_update(uint32_t a0);  // 0x0026ff10 (particle type 0x28)

}  // namespace rac1

#endif
