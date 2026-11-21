// Class 1371 (Hoven and Quartu), shared: one port of the two levels' copies (shared_class1371.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1371_H
#define RAC1_GAME_OVL_SHARED_CLASS1371_H

#include <cstdint>

namespace rac1 {

// A piece knocked off something: once thrown it tumbles under the enemies' physics and bursts where it lands.
void class1371_update(uint32_t m);  // L12:0x00308918 L15:0x002ea5e0
// One spawned at `owner` (its place, turn, +0xb8 the owner), its physics record cleared; the moby, or 0.
uint32_t class1371_spawn(uint32_t owner);  // L12:0x00308670 L15:0x002ea338
// The piece `m` thrown backwards (its turn plus 180 degrees) in a jump arc, with a sound at random. (The first argument is
// not read.)
void class1371_throw(uint32_t unused, uint32_t m);  // L12:0x00308708 L15:0x002ea3d0

}  // namespace rac1

#endif
