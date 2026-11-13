// Class 1633 (the Sandmouse's shot) and the Sandmouse's glow (class 1818's draw callback), shared by every level that has
// them: one port of the levels' copies (shared_sandmouse.cpp). Each function's `orig:` line lists every level's address.
#ifndef RAC1_GAME_OVL_SHARED_SANDMOUSE_H
#define RAC1_GAME_OVL_SHARED_SANDMOUSE_H

#include <cstdint>

namespace rac1 {

// The shot's update.
void class1633_update(uint32_t m);  // L1:0x0030c9a8 L2:0x002efee0 L3:0x002e08c8 L4:0x002e5928 L5:0x0031d760 L6:0x003094f0 L8:0x00308920 L11:0x0031b758 L12:0x00309648 L14:0x00308be0
// The Sandmouse's two glows (its pvars' points +0x70 / +0x80), each through the level's own build of glow_draw.
void sandmouse_glow(uint32_t m);  // L1:0x0030de68 L2:0x002f0eb0 L3:0x002e1898 L4:0x002e68f8 L5:0x0031e730 L6:0x0030adb0 L8:0x0030ab50 L11:0x0031c728 L12:0x0030a618 L14:0x00309bb0

}  // namespace rac1

#endif
