// Pokitaru (level 11) on its own overlay. Level 11's engine build is Novalis's family (the hero controller has the shapes of Novalis's / Rilgar's / Batalia's, with
// its own sizes): 1835 of Veldin's 1974 functions are found in its overlay (82.8 %). The hero controller's variants, the class table, the camera types, the sound
// emitters and the class hooks are level 11's own, ported by hand at its addresses (`// orig: L11:0x00xxxxxx`). The generic parts (the table rows, on_level) are in
// variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L11_H
#define RAC1_GAME_OVL_VARIANTS_L11_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int POKITARU = 11;

// orig: (host) the current translation target is Pokitaru's overlay
inline bool on_pokitaru() { return on_level(POKITARU); }

// A level-11 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L11_ADDR(a) LVL_ADDR(11, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L11_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L11_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
