// Oltanis (level 14) own camera types, sound emitters, camera mode hook and class hooks: the dispatch from the Veldin ports' indirect
// calls (`jalr`) to their ports (the camera types 20 / 21 accepts in the uniform ABI), while the translation target is level
// 14's overlay. (Its cutscene extras L14:0x2460f8 are Veldin's 0x23e268's level 14 branch: cutscene.cpp's port runs them.)
// Each returns false when the address is not one of level 14's own functions (the caller goes on to its Veldin cases).
#ifndef RAC1_GAME_OVL_OLTANIS_HOOKS_H
#define RAC1_GAME_OVL_OLTANIS_HOOKS_H

#include <cstdint>

#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/variants_l14.h>

namespace rac1 {

// orig: (none) -- level 14's registrations of its classes, the classes' draw callbacks and the class hooks +0 / +8 / +0xc (oltanis_mobys.cpp)
void oltanis_moby_fns_register();

// orig: (host) level 14's own camera types (0x13, 0x14, 0x15, section 4 of the overlay): accept (cam, other) -> *ret, init / update / exit
bool oltanis_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret);

// orig: (host) level 14's sound emitter type function `fn` (types 1, 3, 4, 6, section 5 of the overlay) on emitter `em`
bool oltanis_sound_emitter_call(uint32_t fn, uint32_t em);

// orig: (host) level 14's camera mode hook (0x3131e0, set by 0x3131a8: the code of Pokitaru's 0x327a90 at level 14's data) and the
// end-of-update callback it queues (0x313240)
bool oltanis_camera_hook_call(uint32_t fn);
bool oltanis_camera_callback_call(uint32_t fn);

// orig: (host) level 14's class hooks: class 252's table 0x1fd9d0 (five hooks of class 749's shape with level 14's state numbers and record
// offset; +0x10, pvars + 0x60, is Blarg's L6:0x2ea800, word for word: class_hook_pvars60_l6; the overlay map pairs it with Veldin's
// 0x2d5800, whose record is +0xd0) and class 270's +0xc (L14:0x2dd1a0: Veldin's 0x2caf10,
// the same code, which the map pairs with 0x2d57d0): the hook's result in `ret`
bool oltanis_class_hook_call(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret);

}  // namespace rac1

#endif
