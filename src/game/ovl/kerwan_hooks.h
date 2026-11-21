// Kerwan (level 3) own sound emitter types: the dispatch from sound_emitters_update's indirect call (`jalr`) to the
// shared ports (sound_emitter_types.cpp), while the translation target is level 3's overlay. Returns false when the address is
// not one of level 3's own emitter functions (the caller goes on to its other cases).
#ifndef RAC1_GAME_OVL_KERWAN_HOOKS_H
#define RAC1_GAME_OVL_KERWAN_HOOKS_H

#include <cstdint>

#include <game/ovl/variants_l3.h>

namespace rac1 {

// orig: (host) level 3's sound emitter type function `fn` (0x2ed800, 0x2ed960: section 5 of the overlay) on emitter `em`
bool kerwan_sound_emitter_call(uint32_t fn, uint32_t em);

}  // namespace rac1

#endif
