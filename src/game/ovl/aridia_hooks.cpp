// Aridia's class hooks: the `class +0x2c` hook table's dispatch for the hooks only Aridia's classes have.
// Veldin's class_hook_4 / class_hook_10 (0x2da218 / 0x2db890) stub every hook they do not know, and map Aridia's
// 0x2d6310 (the Sand Shark's hook +0x10: pvar + 0x60) onto Veldin's 0x2d5800 (class 749's: pvar + 0xd0), which the
// matcher takes for the same code (it masks small immediates). Aridia's own copies know the Sand Shark's hooks (class
// 580: its table 0x2d61c0 .. 0x2d6340, transliterated in aridia_classes_tr.cpp).
#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/aridia_hooks.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/variants_l2.h>
#include <game/port_stubs.h>

namespace rac1 {

// The class's hook +4 (class +0x2c table) on moby m: 1 when it has none; Aridia's default hook (0x2e64c0) is
// `move v0,zero`, the Sand Shark's (0x2d6210) is ari_carry_hook4_l2 (carry_hooks_levels.cpp; the other argument registers
// zero, as the transliterated hook had them).
// orig: L2:0x002e4e88
int32_t class_hook_4_l2(uint32_t m) {
    const uint32_t fn = lwu(lwu(lwu(m + 0x24) + 0x2c) + 4);
    if (fn == 0) return 1;
    if (fn == L2_ADDR(0x002e64c0)) return 0;
    if (fn == L2_ADDR(0x002d6210)) return ari_carry_hook4_l2(m, 0, 0, 0.0f);
    return static_cast<int32_t>(TODO_PORT(fn, "class hook +4")(m));
}

// The class's hook +0x10 (class +0x2c table) on moby m: 0 for no moby, a retired one (state 0xfe / 0xfd), one without
// pvars or class, or no hook; the default hook (0x2e64f8) returns 0, the Sand Shark's (0x2d6310) is transliterated.
// orig: L2:0x002e6500
uint32_t class_hook_10_l2(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t s = lbu(m + 0x20);
    if (s == 0xfe) return 0;
    if (s == 0xfd) return 0;
    if (lw(m + 0x78) == 0) return 0;
    const uint32_t cls = lwu(m + 0x24);
    if (cls == 0) return 0;
    const uint32_t fn = lwu(lwu(cls + 0x2c) + 0x10);
    if (fn == 0) return 0;
    if (fn == L2_ADDR(0x002e64f8)) return 0;
    if (fn == L2_ADDR(0x002d6310)) return shark_hook_10_l2(m);
    return static_cast<uint32_t>(TODO_PORT(fn, "class hook +0x10")(m));
}

// The Sand Shark's hooks as the `jalr` of the transliterated code reaches them (the moby dispatcher: a0 = moby, the
// result unused there): one adapter each (the hooks +0 .. +0xc are hand-written: carry_hooks_levels.cpp, and +0xc is
// Veldin's e749_hookc, the same code; the other argument registers zero, as the transliterated hooks had them).
// orig: (none) -- adapter of L2:0x002d61c0
void shark_hook_moby_0(uint32_t m) { ari_carry_hook0_l2(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L2:0x002d6210
void shark_hook_moby_4(uint32_t m) { ari_carry_hook4_l2(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L2:0x002d6290
void shark_hook_moby_8(uint32_t m) { ari_carry_hook8_l2(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L2:0x002d62e0
void shark_hook_moby_c(uint32_t m) { e749_hookc(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L2:0x002d6310
void shark_hook_moby_10(uint32_t m) { shark_hook_10_l2(m); }

// orig: (none) -- adapter of L2:0x002d6340
void shark_hook_moby_14(uint32_t m) { shark_hook_14_l2(m); }

}  // namespace rac1
