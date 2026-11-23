// Gaspar (level 9) own sound emitters and class hooks: the dispatch from the Veldin ports' indirect calls
// (`jalr`) to the level's hand-written ports, while the translation target is level 9's overlay. Each returns false when the
// address is not one of level 9's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_GASPAR_HOOKS_H
#define RAC1_GAME_OVL_GASPAR_HOOKS_H

#include <cstdint>

#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

// orig: (none) -- level 9's registrations of its classes and the class hooks +0 / +8 / +0xc (gaspar_mobys.cpp)
void gaspar_moby_fns_register();

// orig: (host) level 9's sound emitter type function `fn` (types 1 and 3, section 5 of the overlay) on emitter `em`
bool gaspar_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 9's camera mode hook (0x314220: the look height follows a value; Eudora's 0x2f0448 at level 9's data) and the end-of-update callback it queues (0x314280)
bool gaspar_camera_hook_call(uint32_t fn);
bool gaspar_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
