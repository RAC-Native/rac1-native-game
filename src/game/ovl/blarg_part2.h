// Blarg's Nebula G34 (level 6) part 2: the declarations of blarg_part2_tr.cpp (generated, regen group
// blarg_part2: `void` = a draw callback in the entry ABI (a0 = the moby, called through ovl_call_moby_fn), `TrRet` = a helper in the
// uniform ABI) and their registration.
#ifndef RAC1_GAME_OVL_BLARG_PART2_H
#define RAC1_GAME_OVL_BLARG_PART2_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

TrRet bl2_217ab8_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x00217ab8
TrRet bl2_214428_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x00214428
TrRet bl2_216b88_l6(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L6:0x00216b88
void blg_class1035_draw_l6(uint32_t a0);  // L6:0x002f6dd0
void blg_class1062_draw_a_l6(uint32_t a0);  // L6:0x002fd1a0
void blg_class1123_draw_l6(uint32_t a0);  // L6:0x00305170

// orig: (none) -- registers the draw callbacks for the indirect calls (ovl_call_moby_fn); called by blarg_moby_fns_register
void blarg_part2_register();

}  // namespace rac1

#endif
