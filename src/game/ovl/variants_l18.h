// Veldin, the finale (level 18) on its own overlay. Level 18's engine build: 1532 of Veldin's 1974 functions found in its overlay (82.1 %,
// docs/reversing/level_change.md section 2). The hero controller is the Novalis family's (26 functions of its own build, as Pokitaru's);
// they, the class table, the camera type, the sound emitters and the class hooks are level 18's own, ported by hand at its addresses (`//
// orig: L18:0x00xxxxxx`). The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L18_H
#define RAC1_GAME_OVL_VARIANTS_L18_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int VELDIN_FINALE = 18;

// orig: (host) the current translation target is Veldin's finale's overlay (level 18)
inline bool on_veldin_finale() { return on_level(VELDIN_FINALE); }

// A level-18 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L18_ADDR(a) LVL_ADDR(18, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L18_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L18_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
