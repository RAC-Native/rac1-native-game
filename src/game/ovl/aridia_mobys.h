// Aridia's (level 2) own moby classes: the update / draw functions of the classes only Aridia
// registers, ported at Aridia's own addresses (`// orig: L2:0x00xxxxxx`).
// They are registered per level (ovl_register_level_moby_fn): the indirect call of the original (`jalr`
// through moby +0x74) reaches them only while the played level's overlay is Aridia's own
// (RAC1_OWN_OVERLAY=1 RAC1_LEVEL=2), keyed by Aridia's address. Where Aridia's build of a class is the same
// code as Novalis's (the matcher's masked comparison; classes 805 spawn point, ...) the Novalis port is
// registered at Aridia's address too (its level-own data addresses passed in).
#ifndef RAC1_GAME_OVL_ARIDIA_MOBYS_H
#define RAC1_GAME_OVL_ARIDIA_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/variants_l2.h>

namespace rac1 {

// orig: (none) -- Aridia's registrations
void aridia_moby_fns_register();

// ---- the props, platforms and mechanisms (aridia_props.cpp)
// (trackdrive_left_update_l2, escalator_update_l2, draw_hook_add_dc80_l2 / _de80_l14 and ocean_wave_table_set are shared: the
// other levels' copies are the same code; their addresses on each level are listed after the declarations)
void trackdrive_left_update_l2(uint32_t m);   // L2:0x002cb978  class 331 (L8:0x002da3c0 L18:0x002ce7d0)
void empty_update_l2(uint32_t m);             // L2:0x002dd4d0 / 0x002e2220 / 0x002f2058  classes 690, 789, 1820
void platform_update_l2(uint32_t m);          // L2:0x002ddc00 / 0x002df8c0  classes 707, 734
void bulldozer_update_l2(uint32_t m);         // L2:0x002dca10  class 656
void grab_crane_update_l2(uint32_t m);        // L2:0x002dd370  class 675
void white_flower_update_l2(uint32_t m);      // L2:0x002f1f60  class 1819
void trespasser_door1_update_l2(uint32_t m);  // L2:0x002dffc8  class 743
void trespasser_door2_update_l2(uint32_t m);  // L2:0x002e0138  class 744
void rocket_door_update_l2(uint32_t m);       // L2:0x002df948  classes 735, 736
void crane_update_l2(uint32_t m);             // L2:0x002df1a8  class 732
void escalator_update_l2(uint32_t m);         // L2:0x002dc6b0  classes 652, 653 (L12:0x002e8c10 class 296)
void draw_hook_add_dc80_l2(uint32_t fn, uint32_t arg);  // L2:0x0020a390 (L5:0x00228110 L7:0x00221290 L9:0x00218810 L12:0x0021bf18 L14:0x0020c698)
void draw_hook_add_de80_l14(uint32_t fn, uint32_t arg);  // the fourth list's: L14:0x0020c760 L15:0x001f2c08
// the ocean's wave table (`alpha` to rec +0x3f; the level's table scaled by k): L2:0x002a4058 (L3:0x00291198 L5:0x002ce098
// L7:0x002cc608 L8:0x002acc70 L9:0x002c11a0 L14:0x002aabf8)
void ocean_wave_table_set(uint32_t rec, uint32_t alpha, float k);
void ocean_wave_scale_l2(float k);            // L2:0x002a47f8 (through 0x002a40d0: ocean_wave_table_set, alpha 0x80)
void ocean_update_l2(uint32_t m);             // L2:0x002ea198  class 854

}  // namespace rac1

#endif
