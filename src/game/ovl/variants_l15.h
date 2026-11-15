// Quartu (level 15) on its own overlay. Level 15's engine build has its own hero controller (the Novalis family's shape, with
// Giant Clank's states; none of its rows is the same code as another level's port): the hero controller's variants, the class table,
// the sound emitters, the class hooks and the camera mode hook are level 15's own, transliterated at its addresses
// (`// orig: L15:0x00xxxxxx`, quartu_hero_tr_*.cpp, quartu_classes_tr_*.cpp). The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L15_H
#define RAC1_GAME_OVL_VARIANTS_L15_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int QUARTU = 15;

// orig: (host) the current translation target is Quartu's overlay
// (its hero code and camera code also in a session whose hero context it is: variants.h on_hero_level)
inline bool on_quartu() { return on_hero_level(QUARTU); }

// A level-15 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L15_ADDR(a) LVL_ADDR(15, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L15_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L15_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
