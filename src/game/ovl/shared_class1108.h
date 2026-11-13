// Class 1108's draw callback (a countdown on the screen: a rounded box and the time left as minutes, seconds and tenths,
// the last digit flickering), shared by the levels that carry it: one port of the levels' copies (shared_class1108.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1108_H
#define RAC1_GAME_OVL_SHARED_CLASS1108_H

#include <cstdint>

namespace rac1 {

void class1108_draw(uint32_t m);  // L6:0x003021d8 L15:0x002d77c0 L17:0x002d5e38 L18:0x002d8098

}  // namespace rac1

#endif
