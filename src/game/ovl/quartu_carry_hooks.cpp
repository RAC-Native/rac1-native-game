// Quartu (level 15): the five class hooks of the level's carried class (its five-hook table +0 .. +0x10), ported by hand
// from the level's own code. They are Quartu's build of Veldin's class 749 hooks (0x2d56e0, 0x2d5730, 0x2d5780, 0x2d57d0,
// 0x2d5800 in enemy_749.cpp, which the overlay map pairs them with): the same calls of the shared carry hooks
// (carry_hooks.h), but with the class's own states (7 while it is held, 1 after, where Veldin's class has 9 and 5) and its
// carry record at the pvars' +0x60 (Veldin's +0xd0). They keep the uniform entry ABI (carry_hooks_levels.cpp's dispatch
// and quartu_mobys.cpp's adapters call them so); the result is v0, f0 is not set (0). Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/ovl/carry_hooks.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

namespace {

// a carry hook's result: held (nonzero) -> state 7; let go while in state 7 -> state 1
uint32_t held_state(uint32_t m, int32_t held) {
    if (held != 0) {
        sb(m + 0x20, 7);
    } else if (lbu(m + 0x20) == 7) {
        sb(m + 0x20, 1);
    }
    return static_cast<uint32_t>(held);
}

}  // namespace

// orig: L15:0x002bc4b0
TrRet qua_hook_2bc4b0_u(uint32_t m, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float, float, float, float, float) {
    return TrRet{held_state(m, carry_hook0(m, a1, a2, f12)), 0.0f};
}

// orig: L15:0x002bc500
TrRet qua_hook_2bc500_u(uint32_t m, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float, float, float, float, float) {
    return TrRet{held_state(m, carry_hook4(m, a1, a2, f12)), 0.0f};
}

// orig: L15:0x002bc550
TrRet qua_hook_2bc550_u(uint32_t m, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float, float, float, float, float) {
    return TrRet{held_state(m, carry_hook8(m, a1, a2, f12)), 0.0f};
}

// The hook +0xc: always held (state 7) after the shared hook.
// orig: L15:0x002bc5a0
TrRet qua_hook_2bc5a0_u(uint32_t m, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float, float, float, float, float) {
    const int32_t r = carry_hookc(m, a1, a2, f12);
    sb(m + 0x20, 7);
    return TrRet{static_cast<uint32_t>(r), 0.0f};
}

// The hook +0x10: the moby's carry record (its pvars' +0x60), 0 without a moby or pvars.
// orig: L15:0x002bc5d0
TrRet qua_hook_2bc5d0_u(uint32_t m, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float) {
    if (m == 0) return TrRet{0, 0.0f};
    const uint32_t pv = lwu(m + 0x78);
    return TrRet{pv != 0 ? pv + 0x60 : 0u, 0.0f};
}

}  // namespace rac1
