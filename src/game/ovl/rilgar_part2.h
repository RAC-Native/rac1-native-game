// Rilgar's (level 5) part 2: the declarations of rilgar_part2_tr.cpp (generated, regen group
// rilgar_part2: `void` = an entry in the entry ABI, `TrRet` = a helper in the uniform ABI) and their registration.
#ifndef RAC1_GAME_OVL_RILGAR_PART2_H
#define RAC1_GAME_OVL_RILGAR_PART2_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// ---- the draw callbacks the classes queue (draw_callback_add), called through ovl_call_moby_fn with a0 = the moby
void ril_class79_draw_a_l5(uint32_t a0);  // L5:0x002d6f48  queued by 0x2d7020 (from class 79's update)
void ril_class79_draw_b_l5(uint32_t a0);  // L5:0x002d7920  queued by class 79's update 0x2d7140
void ril_class823_draw_l5(uint32_t a0);  // L5:0x0030c220  queued by class 823's update 0x30c0a8
void ril_class838_draw_l5(uint32_t a0);  // L5:0x0030de90  queued by class 838's update 0x30dc68
void ril_class879_draw_l5(uint32_t a0);  // L5:0x00316070  queued by class 879's update 0x316110
void ril_class1511_draw_l5(uint32_t a0);  // L5:0x0031cf58  queued by class 1511's update 0x31c8e0
void ril_class1818_draw_l5(uint32_t a0);  // L5:0x0031e730  queued by 0x31e798 (from class 1818's update)

// ---- the helpers (uniform ABI)
TrRet rl2_22de38_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x0022de38
TrRet rl2_2775c8_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x002775c8
TrRet rl2_277900_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x00277900
TrRet rl2_22e700_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x0022e700
TrRet rl2_277b20_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x00277b20
TrRet rl2_2ce130_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x002ce130
TrRet rl2_2ce720_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x002ce720
TrRet rl2_2ce830_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x002ce830

// orig: (none) -- registers the draw callbacks for the indirect calls (ovl_call_moby_fn); called by rilgar_moby_fns_register
void rilgar_part2_register();

}  // namespace rac1

#endif
