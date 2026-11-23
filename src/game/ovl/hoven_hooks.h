// Hoven (level 12) own sound emitters, camera type 0x12's accept and camera mode hook: the dispatch from the Veldin ports' indirect
// calls (`jalr`) to the level's hand-written ports, while the translation target is level 12's overlay. Each returns
// false when the address is not one of level 12's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_HOVEN_HOOKS_H
#define RAC1_GAME_OVL_HOVEN_HOOKS_H

#include <cstdint>

#include <game/ovl/hoven_mobys.h>
#include <game/ovl/variants_l12.h>

namespace rac1 {

// orig: (none) -- level 12's registrations of its classes, the class hooks +0 / +8 / +0xc and the draw callbacks (hoven_mobys.cpp)
void hoven_moby_fns_register();

// orig: (host) level 12's sound emitter type function `fn` (types 1, 3, 5 and 6, section 5 of the overlay) on emitter `em`
bool hoven_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 12's camera type function `fn` (type 0x12's accept, 0x316478; its init / update / exit are Veldin's): the result in *ret
bool hoven_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 12's camera mode hook (0x3148f0, set by 0x3148b8) and the end-of-update callback it queues (0x314950)
bool hoven_camera_hook_call(uint32_t fn);
bool hoven_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
