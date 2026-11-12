// Gemlik Base (level 13, the code's "Oltanis Orbit") on its own overlay. Level 13's engine build is Veldin's almost everywhere
// (1887 of Veldin's 1974 functions found in its overlay, 95.5 %, like Gaspar's / Orxon's): the hero controller and the engine run the
// Veldin ports; what is level 13's own is its class table (60 update functions), camera type 0x13, sound emitter type 1, the five hooks of
// class 63's table, the camera mode hook and the classes' callbacks, transliterated at its addresses (`// orig: L13:0x00xxxxxx`,
// gemlik_classes_tr_*.cpp). The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L13_H
#define RAC1_GAME_OVL_VARIANTS_L13_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int GEMLIK = 13;

// orig: (host) the current translation target is Gemlik Base's overlay (level 13)
// (its hero code and camera code also in a session whose hero context it is: variants.h on_hero_level)
inline bool on_gemlik() { return on_hero_level(GEMLIK); }

// A level-13 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L13_ADDR(a) LVL_ADDR(13, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L13_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L13_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
