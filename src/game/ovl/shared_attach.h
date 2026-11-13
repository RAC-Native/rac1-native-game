// The carried mobys of Hoven and Veldin's finale (shared_attach.cpp: one port for both levels' copies). Phase 1 of stage 4:
// the levels' generated copies still run; these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_ATTACH_H
#define RAC1_GAME_OVL_SHARED_ATTACH_H

#include <cstdint>

namespace rac1 {

// An attachment record (0x30 bytes): the moby's place +0 and turn +0x10 in the carrier's frame, its index +0x20 (-1: none).
// `n` records at `recs` taken from where their mobys are now around `carrier` (L12:0x0027b268, L18:0x00265250)
void attach_bind(uint32_t carrier, uint32_t recs, int32_t n);
// the records' mobys put where the carrier takes them (L12:0x0027b370, L18:0x00265358)
void attach_carry(uint32_t carrier, uint32_t recs, int32_t n);

}  // namespace rac1

#endif
