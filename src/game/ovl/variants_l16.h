// Kalebo III (level 16) on its own overlay. Level 16's engine build has its own hero controller (the Novalis family's shape; several
// rows are the same words as Rilgar's (the hoverboard), transliterated at level 16's addresses rather than shared): the hero controller's variants, the class table,
// the sound emitters, the class hooks and the camera mode hook are level 16's own, transliterated at its addresses
// (`// orig: L16:0x00xxxxxx`, kalebo_hero_tr_*.cpp, kalebo_classes_tr_*.cpp). The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L16_H
#define RAC1_GAME_OVL_VARIANTS_L16_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int KALEBO = 16;

// orig: (host) the current translation target is Kalebo III's overlay
// (its hero code and camera code also in a session whose hero context it is: variants.h on_hero_level)
inline bool on_kalebo() { return on_hero_level(KALEBO); }

// A level-15 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L16_ADDR(a) LVL_ADDR(16, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L16_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L16_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
