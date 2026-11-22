// Kalebo III (level 16) own camera types, sound emitters and camera mode hook: the dispatch from the Veldin ports' indirect calls
// (`jalr`) to their hand-written ports, while the translation target is level 16's overlay. Each returns false when the
// address is not one of level 16's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_KALEBO_HOOKS_H
#define RAC1_GAME_OVL_KALEBO_HOOKS_H

#include <cstdint>

#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

// orig: (none) -- level 16's registrations of its classes, the class hooks +0 / +8 / +0xc and the callbacks (kalebo_mobys.cpp)
void kalebo_moby_fns_register();

// orig: (host) level 16's own camera types (0x3 and 0x8, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool kalebo_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 16's sound emitter type function `fn` (types 1 / 3 / 6, section 5 of the overlay) on emitter `em`
bool kalebo_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 16's camera mode hook (0x2f3a88, set by 0x2f3a50) and the end-of-update callback it queues (0x2f3ae8)
bool kalebo_camera_hook_call(uint32_t fn);
bool kalebo_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
