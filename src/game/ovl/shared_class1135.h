// Class 1135 (the teleporter's ring of light), shared by every level that has it: one port of the levels' copies of its
// draw callback (shared_class1135.cpp). Its `orig:` line lists every level's address.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1135_H
#define RAC1_GAME_OVL_SHARED_CLASS1135_H

#include <cstdint>

namespace rac1 {

// The ring's draw callback.
void class1135_draw(uint32_t m);  // L1:0x003094f0 L7:0x0031ce40 L8:0x00304bc0 L12:0x00301600 L13:0x003051a0 L16:0x002e0a38 L17:0x002e5848 L18:0x002ee490

}  // namespace rac1

#endif
