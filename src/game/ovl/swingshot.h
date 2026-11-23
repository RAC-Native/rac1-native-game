#ifndef RAC1_GAME_OVL_SWINGSHOT_H
#define RAC1_GAME_OVL_SWINGSHOT_H
// The Swingshot (item 0x0c, hand class 0xd0): the hand's update and the spawn of its grapple moby (class 0xd1),,
// ported by hand (swingshot.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

uint32_t swingshot_spawn_grapple(uint32_t m, float pf12);                    // orig: 0x002c6f88
void swingshot_update(uint32_t m);                                           // orig: 0x002c6158
void swingshot_fns_register();                                               // orig: (none)

}  // namespace rac1
#endif
