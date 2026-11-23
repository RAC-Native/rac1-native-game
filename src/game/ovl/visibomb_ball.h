#ifndef RAC1_GAME_OVL_VISIBOMB_BALL_H
#define RAC1_GAME_OVL_VISIBOMB_BALL_H
// visibomb_ball, ported by hand (visibomb_ball.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void visibomb_ball_update(uint32_t m);  // orig: 0x002b6140
void visibomb_ball_2b57f0(uint32_t p0, uint32_t p1);  // orig: 0x002b57f0
void visibomb_ball_2b5b20(uint32_t p0, uint32_t p1);  // orig: 0x002b5b20
void visibomb_ball_2b5ba0(uint32_t p0, uint32_t p1);  // orig: 0x002b5ba0
void visibomb_ball_2b5d00(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);  // orig: 0x002b5d00
void visibomb_ball_203b78();  // orig: 0x00203b78
void visibomb_ball_2eba18(uint32_t p0);  // orig: 0x002eba18
void visibomb_ball_fns_register();  // orig: (none)

}  // namespace rac1
#endif
