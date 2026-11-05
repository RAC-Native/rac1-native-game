// The world's static lighting: see world_lighting.cpp.
#ifndef RAC1_GAME_OVL_WORLD_LIGHTING_H
#define RAC1_GAME_OVL_WORLD_LIGHTING_H

#include <cstdint>

namespace rac1 {

// orig: 0x00295350
void world_tfrag_lighting(uint32_t list);
void world_tfrag_lighting_tr(uint32_t list);  // orig: (host) its transliteration (world_lighting.cpp), the hand-written routine's reference
// orig: 0x00297728
void world_tie_lighting(uint32_t list);
// orig: 0x00288908
void world_shrub_lighting(uint32_t list);

}  // namespace rac1

#endif
