// Batalia (level 8) own camera types, sound emitters and class hooks: the dispatch from the Veldin ports' indirect calls
// (`jalr`) to the level's hand-written ports, while the translation target is level 8's overlay. Each returns false when the
// address is not one of level 8's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_BATALIA_HOOKS_H
#define RAC1_GAME_OVL_BATALIA_HOOKS_H

#include <cstdint>

#include <game/ovl/batalia_mobys.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

// orig: (none) -- level 8's registrations of its classes and the class hooks +0 / +8 / +0xc (batalia_mobys.cpp)
void batalia_moby_fns_register();

// orig: (host) level 8's camera type function `fn` (types 3 and 0x12, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool batalia_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 8's sound emitter type function `fn` (types 1, 3, 4 and 6, section 5 of the overlay) on emitter `em`
bool batalia_sound_emitter_call(uint32_t fn, uint32_t em);

}  // namespace rac1

#endif
