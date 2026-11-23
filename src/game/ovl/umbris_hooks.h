// Umbris (level 7) own camera type, sound emitters, camera hook and the engine functions of its own build: the dispatch from the
// Veldin ports' indirect calls (`jalr`) and calls to the level's hand-written ports, while the translation target is level 7's
// overlay. Each returns false when the address is not one of level 7's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_UMBRIS_HOOKS_H
#define RAC1_GAME_OVL_UMBRIS_HOOKS_H

#include <cstdint>

#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

// orig: (none) -- level 7's registrations of its classes and the class hook +0xc (umbris_mobys.cpp)
void umbris_moby_fns_register();

// orig: (host) level 7's camera type function `fn` (type 0x12, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool umbris_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 7's sound emitter type function `fn` (types 3 and 6, section 5 of the overlay) on emitter `em`
bool umbris_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 7's camera mode hook (0x328090, set by 0x328058: the look height follows a value; Eudora's 0x2f0448 at level 7's data) and the end-of-update callback it queues (0x3280f0)
bool umbris_camera_hook_call(uint32_t fn);
bool umbris_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
