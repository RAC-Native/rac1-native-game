// Umbris (level 7) on its own overlay. Level 7's engine build is Veldin's almost
// everywhere (1887 of Veldin's 1974 functions found in its overlay, 95.5 %): the hero controller and the engine run the Veldin ports; what is
// level 7's own is its class table (52 update functions), camera type 0x12, two sound emitter types, the class hooks +0xc / +0x10, the camera mode
// hook, `level_sound_play` and `cutscene_level_extras`, ported by hand at its addresses (`// orig: L7:0x00xxxxxx`).
// The generic parts (the table rows, on_level) are in variants.h.
#ifndef RAC1_GAME_OVL_VARIANTS_L7_H
#define RAC1_GAME_OVL_VARIANTS_L7_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>

namespace rac1 {

constexpr int UMBRIS = 7;

// orig: (host) the current translation target is Umbris overlay
inline bool on_umbris() { return on_level(UMBRIS); }

// A level-7 address of the port's own (level-only data / code, not a Veldin equivalent): no Veldin
// translation; on another region's disc, the region table's.
#define L7_ADDR(a) LVL_ADDR(7, a)
// the same address as a `case` label: its switch compares lvl_key(pointer)
#define L7_CASE(a) (a)
// a `lui` half kept apart from its low half (ovl_reloc.h lvl_page): never translated
#define L7_PAGE(a) LVL_PAGE(a)

}  // namespace rac1

#endif
