// Quartu (level 15) own camera types, sound emitters and camera mode hook: the dispatch from the Veldin ports' indirect calls
// (`jalr`) to the level's hand-written ports, while the translation target is level 15's overlay. Each returns false when the
// address is not one of level 15's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_QUARTU_HOOKS_H
#define RAC1_GAME_OVL_QUARTU_HOOKS_H

#include <cstdint>

#include <game/ovl/quartu_mobys.h>
#include <game/ovl/variants_l15.h>

namespace rac1 {

// orig: (none) -- level 15's registrations of its classes, the class hooks +0 / +8 / +0xc and the callbacks (quartu_mobys.cpp)
void quartu_moby_fns_register();

// orig: (host) level 15's own camera types (0x13 and 0x16, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool quartu_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 15's sound emitter type function `fn` (types 3 / 5, section 5 of the overlay) on emitter `em`
bool quartu_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 15's camera mode hook (0x2f6980, set by 0x2f6948) and the end-of-update callback it queues (0x2f69e0)
bool quartu_camera_hook_call(uint32_t fn);
bool quartu_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
