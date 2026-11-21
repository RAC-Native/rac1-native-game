// The classes whose update only queues their draw callback (Kalebo III's 1952, Drek's fleet's 1907, 1908, 1910, 1911, 1913,
// 1918): state 0 goes to 1, state 1 queues the class's draw each frame (shared_class1952.cpp). One body for the seven copies,
// each class's draw callback its own (the levels still register their copies of the draws). Phase 1 of stage 4: the
// levels' generated copies still run; these are proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1952_H
#define RAC1_GAME_OVL_SHARED_CLASS1952_H

#include <cstdint>

namespace rac1 {

void class1952_update(uint32_t m);  // L16:0x002e9df0
void class1907_update(uint32_t m);  // L17:0x002f57a0
void class1908_update(uint32_t m);  // L17:0x002f5ab8
void class1910_update(uint32_t m);  // L17:0x002f5dd0
void class1911_update(uint32_t m);  // L17:0x002f60e8
void class1913_update(uint32_t m);  // L17:0x002f6400
void class1918_update(uint32_t m);  // L17:0x002f6718

}  // namespace rac1

#endif
