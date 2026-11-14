// Orxon (level 10) own moby classes: the registration table. The ports are the transliterations in
// orxon_classes_tr_N.cpp (declarations: orxon_mobys.h). Keyed by level 10's own address and by level, so the indirect call (`jalr`) through
// moby +0x74) reaches them only while the played level's overlay is level 10's.
#include <game/ovl/orxon_hooks.h>
#include <game/ovl/orxon_mobys.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L10:0x002ce618 (class 827's hook +0: the indirect-call form, the moby only)
void orx_hook_2ce618_m(uint32_t m) { orx_hook_2ce618_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L10:0x002ce770 (class 827's hook +8: the indirect-call form, the moby only)
void orx_hook_2ce770_m(uint32_t m) { orx_hook_2ce770_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L10:0x002ce800 (class 827's hook +0xc: the indirect-call form, the moby only)
void orx_hook_2ce800_m(uint32_t m) { orx_hook_2ce800_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

}  // namespace

// orig: (none) -- level 10's registrations
void orxon_moby_fns_register() {
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00295a38), orx_class1033_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00295c20), orx_class1117_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00298668), orx_class18_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00298b68), orx_class22_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x0029e1c8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bae38), orx_class258_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bd0e8), orx_class302_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bd768), orx_class303_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002be858), orx_class351_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002befe8), orx_class353_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c4bc8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c6470), orx_class605_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c74b8), orx_class648_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c7a20), orx_class702_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c7cf8), orx_class737_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c9590), orx_class758_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c9c70), orx_class794_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ca220), orx_class805_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002cc2e8), orx_class819_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002cc8b8), orx_class827_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002cea78), orx_class832_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d4078), orx_class857_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d7b80), orx_class859_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d7fe8), orx_class915_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d85c8), orx_class938_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d8c00), orx_class939_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d8d30), orx_class947_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d90a8), orx_class1015_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d92b8), orx_class1031_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d9550), orx_class1038_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d9bf8), orx_class1040_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d9eb8), orx_class1047_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002da2c8), orx_class1067_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dabe8), orx_class1068_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dcc58), orx_class1073_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd010), orx_class1085_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd3d8), orx_class1090_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd650), orx_class1100_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd8d8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd8e0), orx_class1134_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002de860), orx_class1141_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002df270), orx_class1196_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e01a8), orx_class1199_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e1d38), orx_class1202_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e4a88), orx_class1229_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e5be0), orx_class1240_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e6a78), orx_class1258_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e7cd8), orx_class1302_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e8358), orx_class1326_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e85b8), orx_class1344_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e8ab0), orx_class1346_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e9028), orx_class1378_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e9648), orx_class1421_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e9990), orx_class1424_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ea1f0), orx_class1544_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eacd8), orx_class1555_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb1b0), orx_class1806_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb6f0), orx_class1855_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb7e0), orx_class1856_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb8b8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb950), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ebb08), orx_class1973_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce618), orx_hook_2ce618_m);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce770), orx_hook_2ce770_m);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce800), orx_hook_2ce800_m);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bd280), orx_cb_2bd280_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce868), orx_cb_2ce868_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d7808), orx_cb_2d7808_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002da690), orx_cb_2da690_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dc698), orx_cb_2dc698_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e7520), orx_cb_2e7520_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e91b8), orx_cb_2e91b8_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb958), orx_cb_2eb958_l10);
}

}  // namespace rac1
