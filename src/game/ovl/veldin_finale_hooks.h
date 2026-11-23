// Veldin, the finale (level 18) own sound emitters, camera type, class hooks and camera mode hook: the dispatch from the Veldin ports'
// indirect calls (`jalr`) to the level's hand-written ports, while the translation target is level 18's overlay. Each
// returns false when the address is not one of level 18's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_VELDIN_FINALE_HOOKS_H
#define RAC1_GAME_OVL_VELDIN_FINALE_HOOKS_H

#include <cstdint>

#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/variants_l18.h>

namespace rac1 {

// orig: (none) -- level 18's registrations of its classes, the class hooks +0 / +8 / +0xc and the draw callbacks (veldin_finale_mobys.cpp)
void veldin_finale_moby_fns_register();

// orig: (host) level 18's camera type 0x12's accept function (section 4 of the overlay; its init / update / exit are Veldin's modifiers' empty ones)
bool veldin_finale_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 18's sound emitter type function `fn` (types 3, 5 and 6, section 5 of the overlay) on emitter `em`
bool veldin_finale_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 18's camera mode hook (0x305318, set by 0x3052e0) and the end-of-update callback it queues (0x305378)
bool veldin_finale_camera_hook_call(uint32_t fn);
bool veldin_finale_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
