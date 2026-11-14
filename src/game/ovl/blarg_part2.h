// Blarg's Nebula G34 (level 6) part 2: the declarations of blarg_part2_tr.cpp (generated, regen group
// blarg_part2: `void` = a draw callback in the entry ABI (a0 = the moby, called through ovl_call_moby_fn), `TrRet` = a helper in the
// uniform ABI) and their registration.
#ifndef RAC1_GAME_OVL_BLARG_PART2_H
#define RAC1_GAME_OVL_BLARG_PART2_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

void blg_class302_draw_l6(uint32_t a0);  // L6:0x002d8738
TrRet bl2_212938_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint64_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x00212938
TrRet bl2_2e3248_l6(uint64_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x002e3248
TrRet bl2_2e3530_l6(uint64_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x002e3530
TrRet bl2_2e37a8_l6(uint64_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x002e37a8
TrRet bl2_2e3cf8_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x002e3cf8
void blg_class615_draw_l6(uint32_t a0);  // L6:0x002e2258
void blg_class832_draw_l6(uint32_t a0);  // L6:0x002ea830
void blg_class857_draw_l6(uint32_t a0);  // L6:0x002f37d0
TrRet bl2_217ab8_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x00217ab8
TrRet bl2_214428_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x00214428
TrRet bl2_216b88_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x00216b88
void blg_class1035_draw_l6(uint32_t a0);  // L6:0x002f6dd0
void blg_class1062_draw_a_l6(uint32_t a0);  // L6:0x002fd1a0
void blg_class1062_draw_b_l6(uint32_t a0);  // L6:0x002fd460
void blg_class1062_draw_c_l6(uint32_t a0);  // L6:0x002fd748
void blg_class1068_draw_l6(uint32_t a0);  // L6:0x002ff680
void blg_class1083_draw_a_l6(uint32_t a0);  // L6:0x003003f0
void blg_class1083_draw_b_l6(uint32_t a0);  // L6:0x00300588
void blg_class1083_draw_c_l6(uint32_t a0);  // L6:0x00300720
void blg_class1083_draw_d_l6(uint32_t a0);  // L6:0x00300958
void blg_class1108_draw_l6(uint32_t a0);  // L6:0x003021d8
void blg_class1123_draw_l6(uint32_t a0);  // L6:0x00305170
void blg_class1302_draw_l6(uint32_t a0);  // L6:0x00307578
void blg_class1818_draw_l6(uint32_t a0);  // L6:0x0030adb0
void blg_class1898_draw_l6(uint32_t a0);  // L6:0x0030bfa8
void blg_class1051_hud_draw_l6(uint32_t a0);  // L6:0x00249738  the draw function (+0x18) of the HUD element class 1051 requests (0x2454c0), a0 = the element

// orig: (none) -- registers the draw callbacks for the indirect calls (ovl_call_moby_fn); called by blarg_moby_fns_register
void blarg_part2_register();

}  // namespace rac1

#endif
