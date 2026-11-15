// Gaspar's own class hooks (class 193's table 0x209b00, +0 .. +0x10) — hand-ported from Gaspar's own overlay. Header
// gaspar_mobys.h; dispatched by carry_hooks_levels.cpp (the carry hooks' indirect calls) and registered for the indirect-call
// form in gaspar_mobys.cpp. The hooks +0 / +4 / +8 are the shared carry_state_hook (shared_helpers.cpp: carried, state 7;
// let go in it, state 1).
#include <game/ovl/carry_hooks.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// Class 193's hook +0xc: the carry hook 0xc, the moby in state 7 whatever it answers. Its answer.
// orig: L9:0x002e2770
int32_t gas_hook_2e2770_l9(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t r = carry_hookc(m, a1, a2, f12);
    sb(m + 0x20, 7);
    return r;
}

// Class 193's hook +0x10: the moby's pvars +0x60 (0 without the moby or its pvars).
// orig: L9:0x002e27a0
uint32_t gas_hook_2e27a0_l9(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x60 : 0;
}

}  // namespace rac1
