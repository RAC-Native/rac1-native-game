// Gaspar (level 9) on its own overlay. Level 9's engine build is Veldin's almost
// everywhere (1887 of Veldin's 1974 functions found in its overlay, 95.5 %, like Umbris's): the hero controller and the engine run the Veldin
// ports; what is level 9's own is its class table (40 update functions), two sound emitter types, the five hooks of class 193's table and
// the camera mode hook, ported by hand at its addresses (`// orig: L9:0x00xxxxxx`).
// The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L9_H
#define RAC1_GAME_OVL_VARIANTS_L9_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int GASPAR = 9;

// orig: (host) the current translation target is Gaspar overlay
inline bool on_gaspar() { return on_level(GASPAR); }

// A level-9 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L9_ADDR(a) LVL_ADDR(9, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L9_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L9_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
