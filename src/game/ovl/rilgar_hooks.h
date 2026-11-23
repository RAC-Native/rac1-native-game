// Rilgar's own camera types, sound emitters and hooks: the dispatch from the Veldin ports' indirect calls (`jalr`) to the level's
// hand-written ports, while the translation target is Rilgar's overlay. Each returns false when the address is not one of Rilgar's own
// functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_RILGAR_HOOKS_H
#define RAC1_GAME_OVL_RILGAR_HOOKS_H

#include <cstdint>

#include <game/ovl/variants_l5.h>

namespace rac1 {

// orig: (host) Rilgar's camera type function `fn` (types 3 and 8, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool rilgar_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) Rilgar's sound emitter type function `fn` (types 1, 3, 5, 6, section 5 of the overlay) on emitter `em`
bool rilgar_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) Rilgar's camera mode hook (0x328f80, set by 0x328f48: the look height follows a value) and the end-of-update callback it queues (0x328fe0)
bool rilgar_camera_hook_call(uint32_t fn);
bool rilgar_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
