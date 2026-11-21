// Class 258 (a zone that shows, wakes and makes solid the mobys it names while the hero, or the camera, is in it), shared
// by every level that has it: one port of the levels' copies (shared_class258.cpp). Each function's `orig:` line lists every
// level's address.
#ifndef RAC1_GAME_OVL_SHARED_CLASS258_H
#define RAC1_GAME_OVL_SHARED_CLASS258_H

#include <cstdint>

namespace rac1 {

// The zone `pv`'s effect on the moby of index `idx` (`inside`: the hero in the zone).
void class258_apply(int32_t idx, uint32_t pv, uint32_t inside);  // L5:0x002f4f60 L7:0x002f23e8 L9:0x002ea098 L10:0x002bab98 L12:0x002e4a38 L13:0x002e4930 L14:0x002db0d8 L15:0x002c7118 L17:0x002c7a10
// The same on each moby of the level's group `group` (`zone`: the zone moby, unused).
void class258_apply_group(uint32_t zone, uint32_t pv, int32_t group, uint32_t inside);  // L5:0x002f5190 L7:0x002f2618 L9:0x002ea2c8 L10:0x002badc8 L12:0x002e4c68 L13:0x002e4b60 L14:0x002db308 L15:0x002c7348 L17:0x002c7c40
// The zone's update.
void class258_update(uint32_t m);  // L5:0x002f5200 L7:0x002f2688 L9:0x002ea338 L10:0x002bae38 L12:0x002e4cd8 L13:0x002e4bd0 L14:0x002db378 L15:0x002c73b8 L17:0x002c7cb0

}  // namespace rac1

#endif
