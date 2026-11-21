// See carry_hooks_levels.h: the dispatch of the levels' own class hooks by code address. Every row is a
// function of a class's hook table (`class +0x2c`) in the level's overlay section 3 (class, update, hook table) that is
// not Veldin's code at the address the overlay map gives (tools/re README's scan): the level's own port, or
// Veldin's port where the level's function is the same code but the map pairs it with nothing or another hook.
#include <game/ovl/carry_hooks_levels.h>

#include <game/ovl/aridia_enemies.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/gemlik_hooks.h>
#include <game/ovl/oltanis_hooks.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/quartu_mobys.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_class827.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>  // (carry_keep_state_hook: Pokitaru)
#include <game/ovl/veldin_orbit_mobys.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>

namespace rac1 {

// Aridia's Sand Shark (class 0x244): hook +0.
// orig: L2:0x002d61c0
int32_t ari_carry_hook0_l2(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook0(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Aridia's Sand Shark (class 0x244): hook +4.
// orig: L2:0x002d6210
int32_t ari_carry_hook4_l2(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook4(m, pa1, pa2, pf12);
    if (res != 0) {
        const uint32_t pv = lwu(m + 0x78);
        sb(m + 0x20, 9);
        const int32_t spawner_idx = lw(pv + 0x288);
        if (spawner_idx >= 0) {
            const uint32_t spawner = lwu(L2_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(spawner_idx) << 8);
            const uint32_t spv = lwu(spawner + 0x78);
            sw(spv + 0x14c, lwu(spv + 0x14c) - 1u);
        }
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Aridia's Sand Shark (class 0x244): hook +8.
// orig: L2:0x002d6290
int32_t ari_carry_hook8_l2(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook8(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Kerwan's class 0x10e: hook +0xc.
// orig: L3:0x002bcbd0
int32_t ker_carry_hookc_l3(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 5);
    return res;
}

// Eudora's Circular Saw Enemy (class 340, 0x154): hook +0.
// orig: L4:0x002c4030
int32_t eud_carry_hook0_l4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook0(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Eudora's Circular Saw Enemy (class 340, 0x154): hook +4.
// orig: L4:0x002c4080
int32_t eud_carry_hook4_l4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook4(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Eudora's Circular Saw Enemy (class 340, 0x154): hook +8.
// orig: L4:0x002c40d0
int32_t eud_carry_hook8_l4(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const uint32_t r = class_hook_10(m);
    const int32_t res = carry_hook8(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 9);
    } else if (lbu(m + 0x20) == 9) {
        sb(m + 0x20, 1);
    }
    if (lh(r + 0x68) == 6) {
        moby_sound_start(1, 0, m);
    }
    return res;
}

// Rilgar's classes 0x23c / 0x362: hook +0.
// orig: L5:0x00300ad8
int32_t ril_carry_hook0_l5(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
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

// Rilgar's classes 0x23c / 0x362: hook +8.
// orig: L5:0x00300b98
int32_t ril_carry_hook8_l5(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hook8(m, pa1, pa2, pf12);
    if (res != 0) {
        sb(m + 0x20, 0xe);
    } else if (lbu(m + 0x20) == 0xe) {
        sb(m + 0x20, 1);
    }
    return res;
}

// Rilgar's classes 0x23c / 0x362: hook +0xc.
// orig: L5:0x00300be8
int32_t ril_carry_hookc_l5(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 0xe);
    return res;
}

// Blarg's class 0x10e: hook +0xc.
// orig: L6:0x002d8118
int32_t blg_carry_hookc_l6(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 5);
    return res;
}

// Umbris's class 0x10e: hook +0xc.
// orig: L7:0x002f44b0
int32_t umb_carry_hookc_l7(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 5);
    return res;
}

// Batalia's class 0x10e: hook +0xc.
// orig: L8:0x002d8180
int32_t bat_carry_hookc_l8(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 5);
    return res;
}

// Orxon's class 0x10e: hook +0xc.
// orig: L10:0x002bcc60
int32_t orx_carry_hookc_l10(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 5);
    return res;
}

// Drek's fleet's class 1382: hook +0xc (the overlay map pairs it with Veldin's class-749 hook 0x2d57d0, whose state is 9).
// orig: L17:0x002f0040
int32_t vo_carry_hookc_l17(uint32_t m, uint32_t pa1, uint32_t pa2, float pf12) {
    const int32_t res = carry_hookc(m, pa1, pa2, pf12);
    sb(m + 0x20, 0xc);
    return res;
}

namespace {
// a transliterated helper in the uniform ABI (TrRet: every argument register) called as a hook: a0 = the moby, a1 / a2 and
// f12 passed on
int32_t u_call(TrRet (*f)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float,
                          float, float, float, float),
               uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    return static_cast<int32_t>(f(m, a1, a2, 0, 0, 0, 0, 0, f12, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0);
}
}  // namespace

// orig: (host) see carry_hooks_levels.h
bool class_hook_call_level(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret) {
    // (a session: a tag of level A's context is A's hook, whatever overlay is loaded; ovl_reloc.h)
    switch (g_ctx_levels != 0 && ctx_is_tag(fn) ? ctx_tag_level(fn) : ovl_reloc_level()) {
    case 1: {  // Novalis: the Peckbot's and the Large Amoeboid's (novalis_hooks.cpp)
        if (class_hook_call_l1(fn, m, a1, a2, f12, ret)) return true;
        uint32_t r = 0;
        if (!class_hook10_call_l1(fn, m, r)) return false;
        ret = static_cast<int32_t>(r);
        return true;
    }
    case 2:  // Aridia: the Sand Shark (class 0x244, table 0x1fb6dc)
        switch (lvl_key(fn)) {
        case L2_CASE(0x002d61c0): ret = ari_carry_hook0_l2(m, a1, a2, f12); return true;
        case L2_CASE(0x002d6210): ret = ari_carry_hook4_l2(m, a1, a2, f12); return true;
        case L2_CASE(0x002d6290): ret = ari_carry_hook8_l2(m, a1, a2, f12); return true;
        case L2_CASE(0x002d62e0): ret = e749_hookc(m, a1, a2, f12); return true;  // (Veldin's 0x2d57d0, the same code)
        case L2_CASE(0x002d6310): ret = static_cast<int32_t>(shark_hook_10_l2(m)); return true;
        default: return false;
        }
    case 3:  // Kerwan: class 0x10e's hook +0xc (table 0x1e4114)
        if (fn != L3_ADDR(0x002bcbd0)) return false;
        ret = ker_carry_hookc_l3(m, a1, a2, f12);
        return true;
    case 4:  // Eudora: the Circular Saw Enemy (class 340, table 0x1dd11c)
        switch (lvl_key(fn)) {
        case L4_CASE(0x002c4030): ret = eud_carry_hook0_l4(m, a1, a2, f12); return true;
        case L4_CASE(0x002c4080): ret = eud_carry_hook4_l4(m, a1, a2, f12); return true;
        case L4_CASE(0x002c40d0): ret = eud_carry_hook8_l4(m, a1, a2, f12); return true;
        case L4_CASE(0x002c4180): ret = e749_hookc(m, a1, a2, f12); return true;  // (Veldin's 0x2d57d0, the same code)
        case L4_CASE(0x002c41b0): ret = static_cast<int32_t>(eud_hook_10_l4(m)); return true;  // (the saw's carry record, eudora_saw.cpp)
        default: return false;
        }
    case 5:  // Rilgar: classes 0x23c / 0x362 (tables 0x2193e8 / 0x219400)
        switch (lvl_key(fn)) {
        case L5_CASE(0x00300ad8): ret = ril_carry_hook0_l5(m, a1, a2, f12); return true;
        case L5_CASE(0x00300b48): ret = u_call(ril_hook_300b48_u, m, a1, a2, f12); return true;
        case L5_CASE(0x00300b98): ret = ril_carry_hook8_l5(m, a1, a2, f12); return true;
        case L5_CASE(0x00300be8): ret = ril_carry_hookc_l5(m, a1, a2, f12); return true;
        case L5_CASE(0x00300c18): ret = u_call(ril_hook_10_u, m, a1, a2, f12); return true;
        default: return false;
        }
    case 6:  // Blarg: class 0x10e's hook +0xc (table 0x20239c), class 0x33b (table 0x2023b4)
        switch (lvl_key(fn)) {
        case L6_CASE(0x002d8118): ret = blg_carry_hookc_l6(m, a1, a2, f12); return true;
        case L6_CASE(0x002ea5e0): ret = class827_hook_carry0(m, a1, a2, f12); return true;
        case L6_CASE(0x002ea6a8): ret = class827_hook_carry(m, a1, a2, f12, 4); return true;
        case L6_CASE(0x002ea738): ret = class827_hook_carry(m, a1, a2, f12, 8); return true;
        case L6_CASE(0x002ea7c8): ret = class827_hook_carryc(m, a1, a2, f12); return true;
        case L6_CASE(0x002ea800): ret = class_hook_pvars60_l6(m); return true;
        default: return false;
        }
    case 7:  // Umbris: class 0x10e's hook +0xc (table 0x2125dc)
        if (fn != L7_ADDR(0x002f44b0)) return false;
        ret = umb_carry_hookc_l7(m, a1, a2, f12);
        return true;
    case 8:  // Batalia: class 0xfc (table 0x1fd200), class 0x10e's hook +0xc (table 0x1fd218)
        switch (lvl_key(fn)) {
        case L8_CASE(0x002d3f60): ret = enemy10e_hook0(m, a1, a2, f12); return true;  // (Veldin's 0x2cadc0, the same code)
        case L8_CASE(0x002d3fb0): ret = enemy10e_hook4(m, a1, a2, f12); return true;  // (Veldin's 0x2cae10, the same code)
        case L8_CASE(0x002d4000): ret = carry_state_hook(m, a1, a2, f12, 8, 5, 1); return true;  // (shared_helpers.cpp)
        case L8_CASE(0x002d4050): ret = class252_hook_c(m, a1, a2, f12); return true;  // (shared_helpers.cpp)
        case L8_CASE(0x002d40d8): ret = u_call(bat_hook_2d40d8_u, m, a1, a2, f12); return true;
        case L8_CASE(0x002d8180): ret = bat_carry_hookc_l8(m, a1, a2, f12); return true;
        default: return false;
        }
    case 9:  // Gaspar: class 0xc1 (table 0x209b00)
        switch (lvl_key(fn)) {
        case L9_CASE(0x002e2680): ret = carry_state_hook(m, a1, a2, f12, 0, 7, 1); return true;  // (shared_helpers.cpp)
        case L9_CASE(0x002e26d0): ret = carry_state_hook(m, a1, a2, f12, 4, 7, 1); return true;
        case L9_CASE(0x002e2720): ret = carry_state_hook(m, a1, a2, f12, 8, 7, 1); return true;
        case L9_CASE(0x002e2770): ret = gas_hook_2e2770_l9(m, a1, a2, f12); return true;  // (gaspar_carry.cpp)
        case L9_CASE(0x002e27a0): ret = static_cast<int32_t>(gas_hook_2e27a0_l9(m)); return true;
        default: return false;
        }
    case 10:  // Orxon: class 0x10e's hook +0xc (table 0x1dde40), class 0x33b (table 0x1dde58)
        switch (lvl_key(fn)) {
        case L10_CASE(0x002bcc60): ret = orx_carry_hookc_l10(m, a1, a2, f12); return true;
        case L10_CASE(0x002ce618): ret = class827_hook_carry0(m, a1, a2, f12); return true;  // (the shared port, shared_class827.cpp)
        case L10_CASE(0x002ce6e0): ret = class827_hook_carry(m, a1, a2, f12, 4); return true;
        case L10_CASE(0x002ce770): ret = class827_hook_carry(m, a1, a2, f12, 8); return true;
        case L10_CASE(0x002ce800): ret = class827_hook_carryc(m, a1, a2, f12); return true;
        case L10_CASE(0x002ce838): ret = u_call(orx_hook_2ce838_u, m, a1, a2, f12); return true;
        default: return false;
        }
    case 11:  // Pokitaru: classes 0x23c / 0x362 (tables 0x21c008 / 0x21c020), class 0x4de (table 0x21c038)
        switch (lvl_key(fn)) {
        case L11_CASE(0x002fb780): ret = amoeboid_hook0_l1(m, a1, a2, f12); return true;  // (every copy's port: novalis_hooks.cpp)
        case L11_CASE(0x002fb7f0): ret = u_call(pok_hook_2fb7f0_u, m, a1, a2, f12); return true;
        case L11_CASE(0x002fb840): ret = u_call(pok_hook_2fb840_u, m, a1, a2, f12); return true;
        case L11_CASE(0x002fb890): ret = u_call(pok_hook_2fb890_u, m, a1, a2, f12); return true;
        case L11_CASE(0x002fb8c0): ret = u_call(pok_hook_2fb8c0_u, m, a1, a2, f12); return true;
        case L11_CASE(0x00316160): ret = u_call(pok_hook_316160_u, m, a1, a2, f12); return true;
        case L11_CASE(0x00316210): ret = carry_keep_state_hook(m, a1, a2, f12, 4); return true;  // (shared_little.cpp: the carry hook +4)
        case L11_CASE(0x00316270): ret = carry_keep_state_hook(m, a1, a2, f12, 8); return true;  // (the carry hook +8)
        case L11_CASE(0x003162d0): ret = u_call(pok_hook_3162d0_u, m, a1, a2, f12); return true;
        case L11_CASE(0x003162f0): ret = u_call(pok_hook_3162f0_u, m, a1, a2, f12); return true;
        default: return false;
        }
    case 12:  // Hoven: class 0xee (table 0x20d228), class 0x10e's hook +0xc (table 0x20d240: Veldin's 0x2caf10 code, the map pairs it with 0x2d57d0)
        switch (lvl_key(fn)) {
        case L12_CASE(0x002e16b8): ret = hov_hook_2e16b8_l12(m, a1, a2, f12); return true;
        case L12_CASE(0x002e1758): ret = carry_state_hook(m, a1, a2, f12, 4, 8, 1); return true;
        case L12_CASE(0x002e17a8): ret = carry_state_hook(m, a1, a2, f12, 8, 8, 1); return true;
        case L12_CASE(0x002e17f8): ret = hov_hook_2e17f8_l12(m, a1, a2, f12); return true;
        case L12_CASE(0x002e1848): ret = static_cast<int32_t>(hov_hook_2e1848_l12(m)); return true;
        case L12_CASE(0x002e6b00): ret = hov_hook_2e6b00_l12(m, a1, a2, f12); return true;
        default: return false;
        }
    case 13:  // Gemlik Base: class 0x3f (table 0x1f6174)
        return gemlik_class_hook_call(fn, m, a1, a2, f12, ret);
    case 14:  // Oltanis: class 0xfc (table 0x1fd9d0), class 0x10e's hook +0xc (table 0x1fd9e8)
        return oltanis_class_hook_call(fn, m, a1, a2, f12, ret);
    case 15:  // Quartu: the 749-type five-hook table 0x1e3e9c (Veldin's class-749 hooks with other constants: 9 -> 7, 5 -> 1, pvars + 0x60)
        switch (lvl_key(fn)) {
        case L15_CASE(0x002bc4b0): ret = u_call(qua_hook_2bc4b0_u, m, a1, a2, f12); return true;
        case L15_CASE(0x002bc500): ret = u_call(qua_hook_2bc500_u, m, a1, a2, f12); return true;
        case L15_CASE(0x002bc550): ret = u_call(qua_hook_2bc550_u, m, a1, a2, f12); return true;
        case L15_CASE(0x002bc5a0): ret = u_call(qua_hook_2bc5a0_u, m, a1, a2, f12); return true;
        case L15_CASE(0x002bc5d0): ret = u_call(qua_hook_2bc5d0_u, m, a1, a2, f12); return true;
        default: return false;
        }
    case 16:  // Kalebo III: the 749-type five-hook table 0x1e9050 (Veldin's class-749 hooks with other constants: 9 -> 7, 5 -> 1, pvars + 0x60)
        switch (lvl_key(fn)) {
        case L16_CASE(0x002e65d0): ret = carry_state_hook(m, a1, a2, f12, 0, 7, 1); return true;  // (shared_helpers.cpp)
        case L16_CASE(0x002e6620): ret = carry_state_hook(m, a1, a2, f12, 4, 7, 1); return true;
        case L16_CASE(0x002e6670): ret = carry_state_hook(m, a1, a2, f12, 8, 7, 1); return true;
        case L16_CASE(0x002e66c0): ret = kal_hook_2e66c0_l16(m, a1, a2, f12); return true;  // (kalebo_race_organiser.cpp)
        case L16_CASE(0x002e66f0): ret = static_cast<int32_t>(kal_hook_2e66f0_l16(m)); return true;
        default: return false;
        }
    case 17:  // Drek's fleet: class 0x566 (table 0x1e7854; +0xc and +0x10 are Veldin's class-749 hooks with other constants: 12 for 9, pvars + 0x60)
        switch (lvl_key(fn)) {
        case L17_CASE(0x002efea8): ret = carry_drop_hook(m, a1, a2, f12, 0); return true;  // (shared_little.cpp: the carry hook +0)
        case L17_CASE(0x002eff30): ret = carry_drop_hook(m, a1, a2, f12, 4); return true;  // (shared_little.cpp: the carry hook +4)
        case L17_CASE(0x002effb8): ret = carry_drop_hook(m, a1, a2, f12, 8); return true;  // (shared_little.cpp: the carry hook +8)
        case L17_CASE(0x002f0040): ret = vo_carry_hookc_l17(m, a1, a2, f12); return true;
        case L17_CASE(0x002f0070): ret = static_cast<int32_t>(class_hook_pvars60_l6(m)); return true;  // (Blarg's L6:0x2ea800, the same code)
        default: return false;
        }
    case 18:  // Veldin, the finale: classes 0x238 (table 0x1f3498) and 0x772 (table 0x1f34c8); class 749's five hooks, Veldin's
              // 0x2d56e0 .. 0x2d5800, the same code (its 90 mobys: carried and sucked as on Veldin; class_hook_4 / _10 keep
              // Veldin's class-749 hooks to level 0, as the other levels' copies have their own constants)
        switch (lvl_key(fn)) {
        case L18_CASE(0x002e13b8): ret = e749_hook0(m, a1, a2, f12); return true;  // (Veldin's 0x2d56e0)
        case L18_CASE(0x002e1408): ret = e749_hook4(m, a1, a2, f12); return true;  // (Veldin's 0x2d5730)
        case L18_CASE(0x002e1458): ret = e749_hook8(m, a1, a2, f12); return true;  // (Veldin's 0x2d5780)
        case L18_CASE(0x002e14a8): ret = e749_hookc(m, a1, a2, f12); return true;  // (Veldin's 0x2d57d0)
        case L18_CASE(0x002e14d8): ret = static_cast<int32_t>(e749_hook_10(m)); return true;  // (Veldin's 0x2d5800)
        case L18_CASE(0x002d6108): ret = vf_bomb_hook0_l18(m, a1, a2, f12); return true;  // (veldin_finale_walking_bomb.cpp)
        case L18_CASE(0x002d6190): ret = carry_state_hook(m, a1, a2, f12, 4, 4, -1); return true;  // (the shared port, shared_helpers.cpp)
        case L18_CASE(0x002d61e0): ret = carry_state_hook(m, a1, a2, f12, 8, 4, -1); return true;
        case L18_CASE(0x002d6230): ret = vf_bomb_hookc_l18(m, a1, a2, f12); return true;
        case L18_CASE(0x002d6250): ret = static_cast<int32_t>(vf_bomb_hook10_l18(m)); return true;
        case L18_CASE(0x002fcbb0): ret = carry_state_hook(m, a1, a2, f12, 0, 7, 2); return true;
        case L18_CASE(0x002fcc00): ret = carry_state_hook(m, a1, a2, f12, 4, 7, 2); return true;
        case L18_CASE(0x002fcc50): ret = carry_state_hook(m, a1, a2, f12, 8, 7, 2); return true;
        case L18_CASE(0x002fcca0): ret = vf_robot_hookc_l18(m, a1, a2, f12); return true;  // (veldin_finale_robots.cpp)
        case L18_CASE(0x002fccd0): ret = static_cast<int32_t>(e749_hook_10(m)); return true;  // (Veldin's 0x2d5800, the same code)
        default: return false;
        }
    default:
        return false;
    }
}

}  // namespace rac1
