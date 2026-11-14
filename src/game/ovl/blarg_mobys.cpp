// Blarg's Nebula G34 (level 6) own moby classes: the registration table. The ports are the transliterations in
// blarg_classes_tr_N.cpp (declarations: blarg_mobys.h). Keyed by level 6's own address and by level, so the indirect call (`jalr` through
// moby +0x74) reaches them only while the played level's overlay is level 6's.
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/blarg_hooks.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/blarg_part2.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L6:0x002d8118 (a class hook +0xc: the indirect-call form, the moby only)
void blg_hook_2d8118_m(uint32_t m) { blg_carry_hookc_l6(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L6:0x002ea5e0 (class hook +0)
void blg_hook_2ea5e0_m(uint32_t m) { blg_hook_2ea5e0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L6:0x002ea738 (class hook +8)
void blg_hook_2ea738_m(uint32_t m) { blg_hook_2ea738_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L6:0x002ea7c8 (class hook +0xc)
void blg_hook_2ea7c8_m(uint32_t m) { blg_hook_2ea7c8_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
}  // namespace

// orig: (none) -- level 6's registrations
void blarg_moby_fns_register() {
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002b4770), blg_class55_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002b9678), empty_update);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002bead0), blg_class156_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002d85a0), blg_class302_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002d8c20), blg_class303_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002d9d10), blg_class367_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002e11c8), blg_class604_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002e1518), blg_class605_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002e1940), blg_class615_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002e6270), blg_class803_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002e66e0), blg_class805_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002e8678), blg_class827_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002eaa40), blg_class832_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f0040), blg_class857_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f3ad8), blg_class911_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f4638), blg_class1016_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f4f00), blg_class1021_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f55a0), blg_class1028_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f6470), blg_class1035_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f7288), blg_class1038_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f7930), blg_class1039_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f7ab8), blg_class1040_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f7d78), blg_class1048_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002f9a28), blg_class1051_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fbfb0), blg_class1054_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fc640), blg_class1061_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fd088), blg_class1062_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fda30), blg_class1066_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002fdbd0), blg_class1068_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002ffcb0), blg_class1083_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00300b90), blg_class1085_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00300df0), blg_class1091_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00301070), blg_class1105_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00301b90), blg_class1108_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00302578), blg_class1109_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00304098), blg_class1118_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x003049f8), blg_class1123_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x003053a8), blg_class1134_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00307d30), blg_class1302_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x003083b0), blg_class1348_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00308868), blg_class1504_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00308c68), blg_class1512_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00309348), blg_class1551_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x003094f0), blg_class1633_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00309860), blg_class1655_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00309e08), blg_class1660_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x00309fa8), blg_class1664_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x0030a148), empty_update);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x0030ae88), blg_class1818_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x0030be60), blg_class1898_update_l6);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002d8118), blg_hook_2d8118_m);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002ea5e0), blg_hook_2ea5e0_m);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002ea738), blg_hook_2ea738_m);
    ovl_register_level_moby_fn(BLARG, L6_ADDR(0x002ea7c8), blg_hook_2ea7c8_m);
    blarg_part2_register();  // (the draw callbacks)
}

}  // namespace rac1
