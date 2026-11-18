// Hoven's (level 12) class hooks +0 / +0xc / +0x10 of class 238 (snow critters, table 0x20d228) and +0xc of class 270
// (Morph-o-Ray chicken, table 0x20d240). Dispatched by carry_hooks_levels.cpp and registered in hoven_mobys.cpp.
#include <game/ovl/carry_hooks.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// Class 238's hook +0: refuses carry in states 9, 11 and 13; when carried, alerts its group (`m + 0x21`) and sets state 8,
// else returns from state 8 to state 1.
// orig: L12:0x002e16b8
int32_t hov_hook_2e16b8_l12(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 9 || st == 11 || st == 13) return 0;
    const int32_t res = carry_hook0(m, a1, a2, f12);
    if (res != 0) {
        const uint32_t grp = lbu(m + 0x21);
        if (grp != 0xff) moby_group_set_bc_l1(grp, 1);
        sb(m + 0x20, 8);
    } else if (lbu(m + 0x20) == 8) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Class 238's hook +0xc: runs carry_hookc, returning 0 without changing state in states 9 and 11, else setting state 8.
// orig: L12:0x002e17f8
int32_t hov_hook_2e17f8_l12(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t res = carry_hookc(m, a1, a2, f12);
    const uint32_t st = lbu(m + 0x20);
    if (st == 9 || st == 11) return 0;
    sb(m + 0x20, 8);
    return res;
}

// Class 238's hook +0x10: the moby's pvars +0x60 (0 without the moby or its pvars).
// orig: L12:0x002e1848
uint32_t hov_hook_2e1848_l12(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x60 : 0;
}

// Class 270's hook +0xc: the carry hook +0xc, placing the moby in state 5.
// orig: L12:0x002e6b00
int32_t hov_hook_2e6b00_l12(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const int32_t res = carry_hookc(m, a1, a2, f12);
    sb(m + 0x20, 5);
    return res;
}

}  // namespace rac1
