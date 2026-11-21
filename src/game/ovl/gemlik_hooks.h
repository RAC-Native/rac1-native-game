// Gemlik Base (level 13) own camera type, sound emitter, camera mode hook and class hooks: the dispatch from the Veldin ports' indirect
// calls (`jalr`) to their hand-written ports, while the translation target is level 13's overlay. Each returns
// false when the address is not one of level 13's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_GEMLIK_HOOKS_H
#define RAC1_GAME_OVL_GEMLIK_HOOKS_H

#include <cstdint>

#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/variants_l13.h>

namespace rac1 {

// orig: (none) -- level 13's registrations of its classes, the classes' draw callbacks, its HUD element and the class hooks +0 / +8 / +0xc (gemlik_mobys.cpp)
void gemlik_moby_fns_register();

// orig: (host) level 13's own camera type (0x13, section 4 of the overlay): accept (cam, other) -> *ret, init / update (its exit is
// Veldin's empty 0x2ed340)
bool gemlik_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 13's sound emitter type function `fn` (type 1, section 5 of the overlay) on emitter `em`
bool gemlik_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 13's camera mode hook (0x316b48, set by 0x316b10: the look height follows a value, the code of Pokitaru's 0x327a90 at
// level 13's data) and the end-of-update callback it queues (0x316ba8)
bool gemlik_camera_hook_call(uint32_t fn);
bool gemlik_camera_callback_call(uint32_t fn);

// orig: (host) level 13's class hooks: class 63's table 0x1f6174 (five hooks of class 749's shape with level 13's state numbers and
// record offset; the overlay map pairs +0xc / +0x10 with Veldin's 0x2d57d0 / 0x2d5800, whose state 9 / record +0xd0 differ): +0 / +4 /
// +0xc / +0x10 are word for word Gaspar's class 193's (L9:0x2e2680 / 0x2e26d0 / 0x2e2770 / 0x2e27a0: their shared ports), +8 has a
// sound of its own (gml_hook_2b7a20_l13, gemlik_hooks.cpp): the hook's result in `ret`
bool gemlik_class_hook_call(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret);

}  // namespace rac1

#endif
