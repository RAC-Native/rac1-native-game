// The debris pieces (class 344 and its kin on Batalia, 980 on Rilgar), shared: one port of the two levels' copies
// (shared_class344.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS344_H
#define RAC1_GAME_OVL_SHARED_CLASS344_H

#include <cstdint>

namespace rac1 {

// A piece flying off something broken (classes 344, 547 .. 551, 588 .. 598, 782 .. 785 on Batalia, 980 on Rilgar).
void class344_update(uint32_t m);  // L5:0x003186f8 L8:0x002dba40
// The piece `m` thrown (state 1): its velocity `vel` and its offset `offset` (EE addresses), its life `frames`, its spin
// at random. (The second argument is not read.)
void class344_throw(uint32_t m, uint32_t unused, uint32_t vel, uint32_t offset, int32_t frames);  // L5:0x00318648 L8:0x002db990

}  // namespace rac1

#endif
