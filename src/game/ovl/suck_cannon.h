#ifndef RAC1_GAME_OVL_SUCK_CANNON_H
#define RAC1_GAME_OVL_SUCK_CANNON_H
// The Suck Cannon (item 0x09) and its helpers (suck_cannon.cpp), written by hand. Addresses are
// Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void suck_cannon_update(uint32_t m);                                         // orig: 0x002da790
int32_t suck_2d9f78();                                                       // orig: 0x002d9f78
void suck_2defe0(uint32_t m);                                                // orig: 0x002defe0
void suck_2ddca0();                                                          // orig: 0x002ddca0
int32_t suck_2da1e0(uint32_t m);                                             // orig: 0x002da1e0
void suck_2d9fd0(uint32_t a0, uint32_t a1, uint32_t a2);                     // orig: 0x002d9fd0
void suck_2da058(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0);  // orig: 0x002da058
void suck_2da250(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);        // orig: 0x002da250
void suck_2ddcb8(uint32_t a0, uint32_t a1);                                  // orig: 0x002ddcb8
void suck_2ddf60(uint32_t a0, uint32_t a1);                                  // orig: 0x002ddf60
void suck_2df1e0(uint32_t a0);                                               // orig: 0x002df1e0
void suck_2da360(uint32_t a0, uint32_t a1);                                  // orig: 0x002da360
void suck_2df048(uint32_t a0);                                               // orig: 0x002df048
void suck_2da660();                                                          // orig: 0x002da660
void suck_beam_draw(uint32_t m);                                             // orig: 0x002dd8e8
void suck_camera_237798(uint32_t p0);                                                 // orig: 0x00237798
void suck_camera_2377b8(uint32_t p0);                                        // orig: 0x002377b8
void suck_camera_23bac0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5);  // orig: 0x0023bac0
void suck_camera_237840(uint32_t p0);                                        // orig: 0x00237840
void suck_cannon_fns_register();                                             // orig: (none)

}  // namespace rac1
#endif
