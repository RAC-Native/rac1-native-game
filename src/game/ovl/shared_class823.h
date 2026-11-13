// Class 823 (Rilgar and Umbris), shared: one port of the two levels' copies (shared_class823.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS823_H
#define RAC1_GAME_OVL_SHARED_CLASS823_H

#include <cstdint>

namespace rac1 {

// A swaying, glowing plume (a fan of quads from its joint 0, facing the camera).
void class823_update(uint32_t m);  // L5:0x0030c0a8 L7:0x00304738
// Its draw callback: ten pairs of quads fanning out and up, their colours from the level's two.
void class823_draw(uint32_t m);  // L5:0x0030c220 L7:0x003048b0

}  // namespace rac1

#endif
