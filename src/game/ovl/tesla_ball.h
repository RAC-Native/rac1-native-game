#ifndef RAC1_GAME_OVL_TESLA_BALL_H
#define RAC1_GAME_OVL_TESLA_BALL_H
// tesla_ball, ported by hand (tesla_ball.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void tesla_2ba970_update(uint32_t m);  // orig: 0x002ba970
void tesla_ball_2baae0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, float pf12, float pf13, float pf14);  // orig: 0x002baae0
void tesla_ball_2bb0b0(uint32_t p0);  // orig: 0x002bb0b0
void tesla_ball_fns_register();  // orig: (none)

}  // namespace rac1
#endif
