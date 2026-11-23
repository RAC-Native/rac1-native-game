// Oltanis (level 14) on its own overlay. Level 14's engine build is neither Veldin's nor exactly the Novalis family's: 1850 of Veldin's 1974
// functions are found in its overlay (84.6 %); its hero controller is its own build (the family's movement-mode update, Veldin's grind rails,
// hero_fall_time with the collision flags 0xd24 as Blarg's). The hero controller's variants, the class table, the camera types 0x13 / 0x14 /
// 0x15, the sound emitters, the class hooks and the cutscene extras are level 14's own, ported by hand at its addresses (`// orig:
// L14:0x00xxxxxx`). The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L14_H
#define RAC1_GAME_OVL_VARIANTS_L14_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int OLTANIS = 14;

// orig: (host) the current translation target is Oltanis's overlay (level 14)
inline bool on_oltanis() { return on_level(OLTANIS); }

// A level-14 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L14_ADDR(a) LVL_ADDR(14, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L14_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L14_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
