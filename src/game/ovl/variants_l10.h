// Orxon (level 10) on its own overlay. Level 10's engine build is Veldin's almost
// everywhere (1897 of Veldin's 1974 functions found in its overlay, 95.8 %): the hero controller and the engine run the Veldin
// ports; what is level 10's own is its class table (62 update functions), a camera type (0x13), a sound emitter type (3), the five
// hooks of class 827's table and the camera mode hook, transliterated at its addresses (`// orig: L10:0x00xxxxxx`, orxon_classes_tr_*.cpp).
// The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L10_H
#define RAC1_GAME_OVL_VARIANTS_L10_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int ORXON = 10;

// orig: (host) the current translation target is Orxon overlay
// (its hero code and camera code also in a session whose hero context it is: variants.h on_hero_level)
inline bool on_orxon() { return on_hero_level(ORXON); }

// A level-10 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L10_ADDR(a) LVL_ADDR(10, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L10_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L10_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
