// Class 341 (a switch pad the hero steps on, its glow and its animation), shared by every level that has it: one port of
// the levels' copies (shared_class341.cpp). Each function's `orig:` line lists every level's address.
#ifndef RAC1_GAME_OVL_SHARED_CLASS341_H
#define RAC1_GAME_OVL_SHARED_CLASS341_H

#include <cstdint>

namespace rac1 {

// The pad's glow colour (+0x90) and its "pressed" byte (+0xbc).
void class341_glow(uint32_t m);  // L5:0x002f81b8 L7:0x002f5640 L11:0x002f31e8 L12:0x002ec5b0 L18:0x002ce938
// The pad's update.
void class341_update(uint32_t m);  // L5:0x002f8080 L7:0x002f5508 L11:0x002f30b0 L12:0x002ec478 L18:0x002ce800

}  // namespace rac1

#endif
