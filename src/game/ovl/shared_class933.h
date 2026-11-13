// Class 933 (Oltanis and Kalebo III) and its carrier's helpers, shared: one port of the two levels' copies
// (shared_class933.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS933_H
#define RAC1_GAME_OVL_SHARED_CLASS933_H

#include <cstdint>

namespace rac1 {

// A glowing object that spins and bursts when it is hit.
void class933_update(uint32_t m);  // L14:0x002ff640 L16:0x002ddde0
// One of `owner` at `pos` (an EE address); the moby, or 0.
uint32_t class933_spawn(uint32_t owner, uint32_t pos);  // L14:0x002ffaf8 L16:0x002de298
// The carrier's (class 923 on Oltanis, 1401 on Kalebo III) helpers: at the start of its path, the one it holds kept on it,
// its sparks.
void class933_carrier_start(uint32_t c);   // L14:0x002fe900 L16:0x002e3e20
void class933_carrier_hold(uint32_t c);    // L14:0x002fec28 L16:0x002e4110
void class933_carrier_sparks(uint32_t c);  // L14:0x002feca8 L16:0x002e4190

}  // namespace rac1

#endif
