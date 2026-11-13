// Novalis's class hooks for the Suck Cannon: the Peckbot's (class 577, 0x241) and the Large Amoeboid's
// (class 572, 0x23c), and their dispatch by code address.
#include <game/ovl/novalis_hooks.h>

#include <game/ovl/carry_hooks.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Peckbot (class 0x241): hook +0.
// orig: L1:0x002f1c78
int32_t peckbot_hook0_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook0(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 7);
    } else if (lbu(m + 0x20) == 7) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Peckbot (class 0x241): hook +4.
// orig: L1:0x002f1cc8
int32_t peckbot_hook4_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook4(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 7);
    } else if (lbu(m + 0x20) == 7) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Peckbot (class 0x241): hook +8. Gemlik's hook 0x2b7a20 is the same code (Veldin's functions only): this port serves it.
// orig: L1:0x002f1d18 L13:0x002b7a20
int32_t peckbot_hook8_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const uint32_t r = class_hook_10(m);
    const int32_t res = carry_hook8(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 7);
    } else if (lbu(m + 0x20) == 7) {
        sb(m + 0x20, 1);
    }
    if (r != 0 && lh(r + 0x68) == 6) {
        moby_sound_start(2, 0, m);
    }
    return res;
}

// Peckbot (class 0x241): hook +0xc.
// orig: L1:0x002f1dc8
int32_t peckbot_hookc_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 7);
    return res;
}

// Peckbot (class 0x241): hook +0x10 (its carry record at pvars +0x60).
// orig: L1:0x002f1df8
uint32_t peckbot_hook10_l1(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x60u : 0u;
}

// Large Amoeboid (class 0x23c / Small Amoeboid 0x362): hook +0. Pokitaru's hook 0x2fb780 is the same code (Veldin's
// functions only): this port serves it (Rilgar's own port of it, L5:0x00300ad8, is ril_carry_hook0_l5 in
// carry_hooks_levels.cpp).
// orig: L1:0x002efa88 L11:0x002fb780
int32_t amoeboid_hook0_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook0(m, pa1, pa2, pf12);
    if (res != 0) {
        if (lh(m + 0xa6) != 0x362) return 0;
        if (lbu(m + 0x20) == 8) return 0;
        sb(m + 0x20, 0xe);
        return res;
    }
    if (lbu(m + 0x20) == 0xe) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Large Amoeboid (class 0x23c / Small Amoeboid 0x362): hook +4.
// orig: L1:0x002efaf8
int32_t amoeboid_hook4_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook4(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 0xe);
    } else if (lbu(m + 0x20) == 0xe) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Large Amoeboid (class 0x23c / Small Amoeboid 0x362): hook +8.
// orig: L1:0x002efb48
int32_t amoeboid_hook8_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook8(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 0xe);
    } else if (lbu(m + 0x20) == 0xe) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Large Amoeboid (class 0x23c / Small Amoeboid 0x362): hook +0xc.
// orig: L1:0x002efb98
int32_t amoeboid_hookc_l1(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 0xe);
    return res;
}

// Large Amoeboid (class 0x23c / Small Amoeboid 0x362): hook +0x10 (its carry record at pvars +0xc0).
// orig: L1:0x002efbc8
uint32_t amoeboid_hook10_l1(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0xc0u : 0u;
}

// orig: (host) see novalis_hooks.h
bool class_hook_call_l1(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret) {
    if (ovl_reloc_level() != 1) return false;
    switch (fn) {
    case 0x002f1c78: ret = peckbot_hook0_l1(m, a1, a2, f12); return true;
    case 0x002f1cc8: ret = peckbot_hook4_l1(m, a1, a2, f12); return true;
    case 0x002f1d18: ret = peckbot_hook8_l1(m, a1, a2, f12); return true;
    case 0x002f1dc8: ret = peckbot_hookc_l1(m, a1, a2, f12); return true;
    case 0x002efa88: ret = amoeboid_hook0_l1(m, a1, a2, f12); return true;
    case 0x002efaf8: ret = amoeboid_hook4_l1(m, a1, a2, f12); return true;
    case 0x002efb48: ret = amoeboid_hook8_l1(m, a1, a2, f12); return true;
    case 0x002efb98: ret = amoeboid_hookc_l1(m, a1, a2, f12); return true;
    default: return false;
    }
}

// orig: (host) see novalis_hooks.h
bool class_hook10_call_l1(uint32_t fn, uint32_t m, uint32_t& ret) {
    if (ovl_reloc_level() != 1) return false;
    switch (fn) {
    case 0x002f1df8: ret = peckbot_hook10_l1(m); return true;
    case 0x002efbc8: ret = amoeboid_hook10_l1(m); return true;
    default: return false;
    }
}

namespace {
// (the indirect calls of the original, `jalr` through moby +0xc of a class's hook table, pass only the moby)
// orig: L1:0x002f1c78 (the indirect-call form: the moby only)
void peckbot_hook0_m(uint32_t m) { peckbot_hook0_l1(m, 0, 0, 0.0f); }
// orig: L1:0x002f1cc8 (the indirect-call form: the moby only)
void peckbot_hook4_m(uint32_t m) { peckbot_hook4_l1(m, 0, 0, 0.0f); }
// orig: L1:0x002f1d18 (the indirect-call form: the moby only)
void peckbot_hook8_m(uint32_t m) { peckbot_hook8_l1(m, 0, 0, 0.0f); }
// orig: L1:0x002f1dc8 (the indirect-call form: the moby only)
void peckbot_hookc_m(uint32_t m) { peckbot_hookc_l1(m, 0, 0, 0.0f); }
// orig: L1:0x002efa88 (the indirect-call form: the moby only)
void amoeboid_hook0_m(uint32_t m) { amoeboid_hook0_l1(m, 0, 0, 0.0f); }
// orig: L1:0x002efaf8 (the indirect-call form: the moby only)
void amoeboid_hook4_m(uint32_t m) { amoeboid_hook4_l1(m, 0, 0, 0.0f); }
// orig: L1:0x002efb48 (the indirect-call form: the moby only)
void amoeboid_hook8_m(uint32_t m) { amoeboid_hook8_l1(m, 0, 0, 0.0f); }
// orig: L1:0x002efb98 (the indirect-call form: the moby only)
void amoeboid_hookc_m(uint32_t m) { amoeboid_hookc_l1(m, 0, 0, 0.0f); }
}  // namespace

// orig: (none) -- Novalis's hooks for the indirect calls (ovl_call_moby_fn)
void novalis_hooks_fns_register() {
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f1c78), peckbot_hook0_m);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f1cc8), peckbot_hook4_m);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f1d18), peckbot_hook8_m);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002f1dc8), peckbot_hookc_m);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002efa88), amoeboid_hook0_m);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002efaf8), amoeboid_hook4_m);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002efb48), amoeboid_hook8_m);
    ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002efb98), amoeboid_hookc_m);
}

}  // namespace rac1
