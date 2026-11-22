// Umbris (level 7) part 2: its own draw callbacks (hand-written) and their registration (umbris_part2.cpp; the seas of classes
// 460 / 1018 are the shared sea_draw / sea_draw_2, shared_sea_draw.h).
#ifndef RAC1_GAME_OVL_UMBRIS_PART2_H
#define RAC1_GAME_OVL_UMBRIS_PART2_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// hand-ported (umbris_snagglebeast_draws.cpp)
void umb_class1106_draw_a_l7(uint32_t a0);  // L7:0x00312420
void um2_312948_l7(uint32_t m, uint32_t frame_off, float rate, float radius, float span, float alpha_lo, float alpha_hi);  // L7:0x00312948
void umb_class1106_draw_b_l7(uint32_t a0);  // L7:0x00312dc8
void um2_312e70_l7(uint32_t m, uint32_t frame_off, uint32_t color, float radius, float spin_rate);  // L7:0x00312e70
void umb_class1106_draw_c_l7(uint32_t a0);  // L7:0x003131b0
void umb_draw_317910_l7(uint32_t a0);  // L7:0x00317910
void umb_class1046_draw_l7(uint32_t a0);  // L7:0x0030dcf0

// orig: (none) -- registers the draw callbacks and particle type 0x50 for the indirect calls (ovl_call_moby_fn); called by umbris_moby_fns_register
void umbris_part2_register();

}  // namespace rac1

#endif
