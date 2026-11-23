// Drek's fleet (level 17, code name Veldin Orbit) on its own overlay. Level 17's engine build: 1542 of Veldin's 1974 functions
// found in its overlay (84.4 %, docs/reversing/level_change.md section 2). The hero controller has twelve functions of its own build
// (its shapes are Veldin's, with Blarg's in places); they, the class table, the sound emitters and the class hooks are level 17's own,
// ported by hand at its addresses (`// orig: L17:0x00xxxxxx`). The generic
// parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L17_H
#define RAC1_GAME_OVL_VARIANTS_L17_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int VELDIN_ORBIT = 17;

// orig: (host) the current translation target is Drek's fleet's overlay (level 17)
inline bool on_veldin_orbit() { return on_level(VELDIN_ORBIT); }

// A level-17 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L17_ADDR(a) LVL_ADDR(17, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L17_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L17_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
