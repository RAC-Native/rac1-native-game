// Blarg's Nebula G34 (level 6) on its own overlay. Level 6's engine build is
// close to Rilgar's (1849 of Veldin's 1974 functions found in its overlay, 84.2 %): the hero controller is Blarg's own build of the
// source (22 functions that are neither Veldin's nor any other level's, ported by hand at level 6's addresses, `// orig: L6:0x00xxxxxx`),
// 12 functions are the same code as Veldin's / another level's that the matcher could not place, and the rest
// of the engine is Veldin's. The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L6_H
#define RAC1_GAME_OVL_VARIANTS_L6_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int BLARG = 6;

// orig: (host) the current translation target is Blarg's Nebula G34 overlay
inline bool on_blarg() { return on_level(BLARG); }

// A level-6 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L6_ADDR(a) LVL_ADDR(6, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L6_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L6_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
