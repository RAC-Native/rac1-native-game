#ifndef RAC1_GAME_OVL_NOVALIS_HOOKS_H
#define RAC1_GAME_OVL_NOVALIS_HOOKS_H
// Novalis's own class hooks for the Suck Cannon: the Peckbot (class 577) and the Large Amoeboid
// (class 572), at Novalis's own addresses (`// orig: L1:0x...`), transliterated (novalis_hooks_asm.cpp). The class
// hook tables (class +0x2c) of Novalis's mobys hold these addresses; the dispatchers below find them while the
// played level's overlay is Novalis's own.
#include <cstdint>

namespace rac1 {

int32_t peckbot_hook0_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L1:0x002f1c78
int32_t peckbot_hook4_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L1:0x002f1cc8
int32_t peckbot_hook8_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L1:0x002f1d18 L13:0x002b7a20
int32_t peckbot_hookc_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);   // L1:0x002f1dc8
uint32_t peckbot_hook10_l1(uint32_t m);                                          // L1:0x002f1df8
int32_t amoeboid_hook0_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // L1:0x002efa88 L11:0x002fb780
int32_t amoeboid_hook4_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // L1:0x002efaf8
int32_t amoeboid_hook8_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // L1:0x002efb48
int32_t amoeboid_hookc_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12);  // L1:0x002efb98
uint32_t amoeboid_hook10_l1(uint32_t m);                                         // L1:0x002efbc8

// orig: (host) the hook (+0 / +4 / +8 / +0xc: moby m, registers a1 / a2 / f12 passed on) at the code address `fn`,
// when it is one of Novalis's own and the played level's overlay is Novalis's (RAC1_OWN_OVERLAY=1 RAC1_LEVEL=1):
// the result in `ret`, true; false for any other address
bool class_hook_call_l1(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret);
// orig: (host) the same for the hook +0x10 (its pvars record pointer in `ret`)
bool class_hook10_call_l1(uint32_t fn, uint32_t m, uint32_t& ret);

void novalis_hooks_fns_register();  // orig: (none)

}  // namespace rac1
#endif
