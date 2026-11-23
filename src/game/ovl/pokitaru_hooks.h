// Pokitaru (level 11) own sound emitters, class hooks and camera mode hook: the dispatch from the Veldin ports' indirect calls
// (`jalr`) to the level's hand-written ports, while the translation target is level 11's overlay. Each returns false when the
// address is not one of level 11's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_POKITARU_HOOKS_H
#define RAC1_GAME_OVL_POKITARU_HOOKS_H

#include <cstdint>

#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/variants_l11.h>

namespace rac1 {

// orig: (none) -- level 11's registrations of its classes, the class hooks +0 / +8 / +0xc and the draw callbacks (pokitaru_mobys.cpp)
void pokitaru_moby_fns_register();

// orig: (host) level 11's sound emitter type function `fn` (types 1 and 5, section 5 of the overlay) on emitter `em`
bool pokitaru_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 11's camera mode hook (0x327a90, set by 0x327a58) and the end-of-update callback it queues (0x327af0)
bool pokitaru_camera_hook_call(uint32_t fn);
bool pokitaru_camera_callback_call(uint32_t fn);

}  // namespace rac1

#endif
