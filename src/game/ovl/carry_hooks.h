#ifndef RAC1_GAME_OVL_CARRY_HOOKS_H
#define RAC1_GAME_OVL_CARRY_HOOKS_H
// The class hooks (class +0x2c table: +0 .. +0x10) of the mobys the Suck Cannon takes, ported by hand
// (carry_hooks.cpp). Addresses are Veldin's overlay addresses (the shared engine). The hooks take the moby and
// pass the rest of the registers on (a1, a2, f12) to the generic bodies.
#include <cstdint>

namespace rac1 {

int32_t carry_hook0(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);     // orig: 0x002db8f8
int32_t carry_hook4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);     // orig: 0x002dbb20
int32_t carry_hook8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);     // orig: 0x002dbc30
int32_t carry_hookc(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);     // orig: 0x002dbe20
int32_t e749_hook0(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);      // orig: 0x002d56e0
int32_t e749_hook4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);      // orig: 0x002d5730
int32_t e749_hook8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);      // orig: 0x002d5780
int32_t e749_hookc(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);      // orig: 0x002d57d0
int32_t enemy10e_hook0(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // orig: 0x002cadc0
int32_t enemy10e_hook4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // orig: 0x002cae10
int32_t enemy10e_hook8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // orig: 0x002cae60
int32_t enemy10e_hookc(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // orig: 0x002caf10
// the hand-written trivial ones (carry_hooks.cpp)
int32_t hook_default_0(uint32_t m);                                          // orig: 0x002db848
int32_t hook_default_4(uint32_t m);                                          // orig: 0x002db850
int32_t hook_default_8(uint32_t m);                                          // orig: 0x002db858
int32_t hook_default_c(uint32_t m);                                          // orig: 0x002db860
uint32_t enemy10e_hook_10(uint32_t m);                                       // orig: 0x002caf40

}  // namespace rac1
#endif
