// Class 857 of Blarg and Orxon, the Gadgebot: its update, its helpers and its draw callback (the glows of the class's
// mobys of one group), shared by the two levels that carry it: one port of the levels' copies (shared_class857.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS857_H
#define RAC1_GAME_OVL_SHARED_CLASS857_H

#include <cstdint>

namespace rac1 {

void class857_update(uint32_t m);                                                  // L6:0x002f0040 L10:0x002d4078
void class857_assign();                                                            // L6:0x002f2250 L10:0x002d6288
void class857_arrive(uint32_t m, uint32_t pv, uint32_t o, uint32_t busy, uint32_t out);  // L6:0x002f2c28 L10:0x002d6c60
void class857_pick_dest(uint32_t m);                                               // L6:0x002f2910 L10:0x002d6948
int32_t class857_find(uint32_t m, uint32_t busy);                                  // L6:0x002f32f0 L10:0x002d7328
float class857_score(uint32_t m, uint32_t o, int32_t state);                       // L6:0x002f2148 L10:0x002d6180
void class857_react(uint32_t m);                                                   // L6:0x002f3640 L10:0x002d7678
void class857_glow(uint32_t m);                                                    // L6:0x002f38c8 L10:0x002d7900
void class857_draw(uint32_t m);                                                    // L6:0x002f37d0 L10:0x002d7808

}  // namespace rac1

#endif
