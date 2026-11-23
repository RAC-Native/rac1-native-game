// Orxon (level 10) own sound emitters and class hooks: the dispatch from the Veldin ports' indirect calls
// (`jalr`) to the level's hand-written ports, while the translation target is level 10's overlay. Each returns false when the
// address is not one of level 10's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_ORXON_HOOKS_H
#define RAC1_GAME_OVL_ORXON_HOOKS_H

#include <cstdint>

#include <game/ovl/orxon_mobys.h>
#include <game/ovl/variants_l10.h>

namespace rac1 {

// orig: (none) -- level 10's registrations of its classes and the class hooks +0 / +8 / +0xc (orxon_mobys.cpp)
void orxon_moby_fns_register();

// orig: (host) level 10's own camera type (0x13, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool orxon_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 10's sound emitter type function `fn` (type 3, section 5 of the overlay) on emitter `em`
bool orxon_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 10's camera mode hook (0x2f4b18: the look height follows a value; Eudora's 0x2f0448 at level 10's data) and the end-of-update callback it queues (0x2f4b78)
bool orxon_camera_hook_call(uint32_t fn);
bool orxon_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
