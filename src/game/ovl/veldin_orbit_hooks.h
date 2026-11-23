// Drek's fleet (level 17) own sound emitter, camera type, class hooks and camera mode hook: the dispatch from the Veldin ports' indirect
// calls (`jalr`) to the level's hand-written ports, while the translation target is level 17's overlay. Each returns
// false when the address is not one of level 17's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_VELDIN_ORBIT_HOOKS_H
#define RAC1_GAME_OVL_VELDIN_ORBIT_HOOKS_H

#include <cstdint>

#include <game/ovl/veldin_orbit_mobys.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

// orig: (none) -- level 17's registrations of its classes, the class hooks +0 / +8 / +0xc and the draw callbacks (veldin_orbit_mobys.cpp)
void veldin_orbit_moby_fns_register();

// orig: (host) level 17's camera type 3 (accept / init / update / exit, section 4 of the overlay) called through `fn`
bool veldin_orbit_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 17's sound emitter type function `fn` (type 5, section 5 of the overlay) on emitter `em`
bool veldin_orbit_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 17's camera mode hook (0x2fff38, set by 0x2fff00) and the end-of-update callback it queues (0x2fff98)
bool veldin_orbit_camera_hook_call(uint32_t fn);
bool veldin_orbit_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
