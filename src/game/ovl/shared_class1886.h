// Class 1886, a blob that class 1885 throws (Gaspar, Gemlik Base): it flies, bounces off the scenery and off the other
// blobs, glows on the floor, and where it comes to rest leaves one of its thrower's group behind before it fades. One
// port of the two levels' copies of its update and its two helpers (shared_class1886.cpp). Phase 1 of stage 4: the levels'
// generated copies still run; these are proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1886_H
#define RAC1_GAME_OVL_SHARED_CLASS1886_H

#include <cstdint>

namespace rac1 {

void class1886_update(uint32_t m);  // L9:0x0030b3b0 L13:0x0030dd80
// The next free member of the thrower's group (its pvar +0x94) woken at `pos` (an EE vector) as the thrower's mark; the
// moby, or 0 (no thrower, or the group has none left).
uint32_t class1886_leave_mark(uint32_t thrower, uint32_t pos);  // L9:0x0030a778 L13:0x0030d148
// The blob gone: its slot in its thrower's list (+0xb0, the blob's pvar byte +0x1c its index) cleared while the thrower
// lives, then the moby retired.
void class1886_retire(uint32_t m, uint32_t pv);  // L9:0x0030b350 L13:0x0030dd20

}  // namespace rac1

#endif
