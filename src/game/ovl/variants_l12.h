// Hoven (level 12) on its own overlay. Level 12's engine build is Novalis's family, as Pokitaru's (the hero controller has the shapes of Novalis's /
// Pokitaru's, with its own sizes): 1518 of Veldin's functions are found in its overlay (81.5 %). The hero controller's variants, the class table, the
// sound emitters, the class hooks and camera type 0x12's first function are level 12's own, ported by hand at its addresses (`// orig: L12:0x00xxxxxx`).
// The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L12_H
#define RAC1_GAME_OVL_VARIANTS_L12_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int HOVEN = 12;

// orig: (host) the current translation target is Hoven's overlay
inline bool on_hoven() { return on_level(HOVEN); }

// A level-12 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L12_ADDR(a) LVL_ADDR(12, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L12_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L12_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
