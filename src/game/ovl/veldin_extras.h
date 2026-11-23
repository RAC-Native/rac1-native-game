// Veldin's engine functions (every level runs them) first reached unported on Rilgar, and their registration for the
// indirect calls.
//  - class 775's update 0x2d7f38: the short-lived moby spawn_moby_307 (0x2d7e90, Rilgar L5:0x309ba8) puts out, called from
//    enemy_physics (0x25c698), the hero's 0x209ca8, the mine (74) and Rilgar's Sharkigator (35): it grows by 1.025 a frame while its
//    byte +0x23 drops by 3, and leaves play (moby_retire) when that byte is below 4;
//  - particle types 0x43's update 0x274518, 0x30's 0x271c88 and 0x32's 0x271df8 (formerly particle_types_tr.cpp).
// - class 167's update 0x2b4198 and its draw-list callback 0x2b3628: the Thruster-Pack's two jets (veldin_shared_classes.cpp,
// formerly shared_classes_tr.cpp).
#ifndef RAC1_GAME_OVL_VELDIN_EXTRAS_H
#define RAC1_GAME_OVL_VELDIN_EXTRAS_H

#include <cstdint>

namespace rac1 {

void class775_update(uint32_t m);  // orig: 0x002d7f38  class 775
// orig: (none) — registers class 775's update and the particle types' for the indirect calls (ovl_call_moby_fn)
void veldin_extras_register();

}  // namespace rac1
#endif
