// Class 1885, a creature that throws blobs (class 1886, shared_class1886.h) at its target (Gaspar, Gemlik Base): its update
// and its five helpers, one port of the two levels' copies (shared_class1885.cpp). Phase 1 of stage 4: the levels'
// generated copies still run; these are proved against them by oracle batch 517.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1885_H
#define RAC1_GAME_OVL_SHARED_CLASS1885_H

#include <cstdint>

namespace rac1 {

void class1885_update(uint32_t m);  // L9:0x0030ab80 L13:0x0030d550
// Its hits (pvars `pv`): the health; killed, its bolts and state 2. (The 1.02 build's fix 25 here.)
void class1885_hit(uint32_t m, uint32_t pv);  // L9:0x0030a9e8 L13:0x0030d3b8
// Its blobs counted (pvars +0x91: its group's live members and its live blobs); the last free slot of its list (+0xb0,
// 20 words), or -1.
int32_t class1885_free_slot(uint32_t m, uint32_t pv);  // L9:0x0030aae0 L13:0x0030d4b0
// A blob (class 1886) of `m`'s at `pos` flying at `vel` (EE vectors), its kind `kind` and its slot `slot`; the blob or 0.
uint32_t class1885_throw(uint32_t m, uint32_t pos, uint32_t vel, uint32_t kind, uint32_t slot);  // L9:0x0030b218 L13:0x0030dbe8
// `out` (an EE vector) a random horizontal offset within `radius` (more often near the edge).
void rand_disc_offset(uint32_t out, float radius);  // L9:0x00277758 L13:0x00263a58
// A random length within `radius`: radius x (1 - u^2), u uniform in [0, 1) (libc_rand's 12 bits).
float rand_radius(float radius);  // L9:0x00277688 L13:0x00263988

}  // namespace rac1

#endif
