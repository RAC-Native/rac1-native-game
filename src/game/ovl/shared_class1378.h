// Class 1378 of Orxon and Drek's fleet (a field of rippling quads, a pair a layer, that holds a hero flag while he is inside
// its box) and its draw callback, shared by the two levels that carry it: one port of the levels' copies
// (shared_class1378.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1378_H
#define RAC1_GAME_OVL_SHARED_CLASS1378_H

#include <cstdint>

namespace rac1 {

void class1378_update(uint32_t m);  // L10:0x002e9028 L17:0x002e9608
void class1378_draw(uint32_t m);    // L10:0x002e91b8 L17:0x002e9798

}  // namespace rac1

#endif
