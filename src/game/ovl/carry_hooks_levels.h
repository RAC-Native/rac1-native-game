#ifndef RAC1_GAME_OVL_CARRY_HOOKS_LEVELS_H
#define RAC1_GAME_OVL_CARRY_HOOKS_LEVELS_H
// The levels' own class hooks of the carried / sucked mobys. A moby class's hook table (`class +0x2c`, five
// functions: +0 the Suck Cannon takes the moby, +4, +8, +0xc, +0x10 its carry record) holds the level's own functions:
// most levels have a class of their own with its own hooks (Eudora's Circular Saw Enemy, Rilgar's 0x23c / 0x362, Blarg's
// 0x33b...), and the overlay matcher pairs several of them with a Veldin hook whose state numbers differ (it masks small
// immediates). The ports below are at each level's own address (carry_hooks_levels_asm.cpp, plus the ones the levels'
// class tasks transliterated earlier); class_hook_call_level finds them by address while that level's overlay is played.
#include <cstdint>

namespace rac1 {

int32_t ari_carry_hook0_l2(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L2:0x002d61c0
int32_t ari_carry_hook4_l2(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L2:0x002d6210
int32_t ari_carry_hook8_l2(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L2:0x002d6290
int32_t ker_carry_hookc_l3(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L3:0x002bcbd0
int32_t eud_carry_hook0_l4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L4:0x002c4030
int32_t eud_carry_hook4_l4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L4:0x002c4080
int32_t eud_carry_hook8_l4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L4:0x002c40d0
int32_t ril_carry_hook0_l5(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L5:0x00300ad8
int32_t ril_carry_hook8_l5(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L5:0x00300b98
int32_t ril_carry_hookc_l5(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L5:0x00300be8
int32_t blg_carry_hookc_l6(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L6:0x002d8118
int32_t umb_carry_hookc_l7(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L7:0x002f44b0
int32_t bat_carry_hookc_l8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L8:0x002d8180
int32_t orx_carry_hookc_l10(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // L10:0x002bcc60
int32_t vo_carry_hookc_l17(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L17:0x002f0040

// orig: (host) the class hook at the code address `fn` (+0 .. +0xc: moby m, the registers a1 / a2 / f12 passed on; +0x10:
// moby m, its carry record returned) when it is one of the played level's own class hooks (levels 1..14): the hook's
// result in `ret`, true; false for any other address (the caller goes on to Veldin's hooks through the overlay map)
bool class_hook_call_level(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret);

}  // namespace rac1
#endif
