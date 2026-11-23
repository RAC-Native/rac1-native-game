// Blarg's Nebula G34 (level 6) own camera type, camera hook and class hooks: the dispatch from the Veldin ports' indirect calls
// (`jalr`) to the level's hand-written ports, while the translation target is level 6's overlay. Each returns false when the
// address is not one of level 6's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_BLARG_HOOKS_H
#define RAC1_GAME_OVL_BLARG_HOOKS_H

#include <cstdint>

#include <game/ovl/variants_l6.h>

namespace rac1 {

// orig: (none) -- level 6's registrations of its classes (blarg_mobys.cpp)
void blarg_moby_fns_register();

// orig: (host) level 6's camera type function `fn` (type 0x12, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool blarg_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 6's camera mode hook (0x3147b0, set by 0x314778: the look height follows a value; Eudora's 0x2f0448 at level 6's data) and the end-of-update callback it queues (0x314810)
bool blarg_camera_hook_call(uint32_t fn);
bool blarg_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
