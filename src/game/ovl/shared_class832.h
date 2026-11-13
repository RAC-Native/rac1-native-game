// Class 832 (the Visibomb's range: its zones, and the static over the screen when the bomb leaves them), shared by every level
// that has it: one port of the levels' copies (shared_class832.cpp). Each definition's `orig:` line lists every level's
// address.
#ifndef RAC1_GAME_OVL_SHARED_CLASS832_H
#define RAC1_GAME_OVL_SHARED_CLASS832_H

#include <cstdint>

namespace rac1 {

// The update: counts the frames the Visibomb (class 172) flies out of its zones, queues the static, ends the bomb at 1.5 s.
void class832_update(uint32_t m);  // L1:0x00302648 L2:0x002e4a48 L3:0x002d40e8 L4:0x002dc168 L5:0x0030dac8 L6:0x002eaa40 L7:0x00304e50 L8:0x002fbb58 L9:0x002fb288 L10:0x002cea78 L11:0x00303070 L12:0x002fa238 L13:0x002fce18 L14:0x002f6338 L15:0x002de290 L18:0x002e4168
// The draw callback: a full-screen static of 32 x 32 sprites, thicker as the count grows.
void class832_draw(uint32_t m);  // L1:0x00302438 L2:0x002e4838 L3:0x002d3ed8 L4:0x002dbf58 L5:0x0030d8b8 L6:0x002ea830 L7:0x00304c40 L8:0x002fb948 L9:0x002fb078 L10:0x002ce868 L11:0x00302e60 L12:0x002fa028 L13:0x002fcc08 L14:0x002f6128 L15:0x002de080 L18:0x002e3f58
// The level's address of the draw callback (its copy of class832_draw, which the update queues), 0 on a level without one.
uint32_t class832_draw_addr();

}  // namespace rac1

#endif
