// Veldin, the finale (level 18) own moby classes: the registration table. The ports are the transliterations in
// veldin_finale_classes_tr_N.cpp (declarations: veldin_finale_mobys.h). Keyed by level 18's own address and by level, so the indirect call (`jalr`)
// through moby +0x74 (and the draw lists' callbacks) reaches them only while the played level's overlay is level 18's.
#include <game/ovl/veldin_finale_hooks.h>
#include <game/ovl/veldin_finale_mobys.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/hud_slot7_tr.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L18:0x002d6108 (class 568's hook +0: the indirect-call form, the moby only)
void vf_hook_2d6108_m(uint32_t m) { vf_hook_2d6108_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L18:0x002d61e0 (class 568's hook +8: the indirect-call form, the moby only)
void vf_hook_2d61e0_m(uint32_t m) { vf_hook_2d61e0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L18:0x002d6230 (class 568's hook +0xc: the indirect-call form, the moby only)
void vf_hook_2d6230_m(uint32_t m) { vf_hook_2d6230_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L18:0x002fcbb0 (class 1906's hook +0: the indirect-call form, the moby only)
void vf_hook_2fcbb0_m(uint32_t m) { vf_hook_2fcbb0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L18:0x002fcc50 (class 1906's hook +8: the indirect-call form, the moby only)
void vf_hook_2fcc50_m(uint32_t m) { vf_hook_2fcc50_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L18:0x002fcca0 (class 1906's hook +0xc: the indirect-call form, the moby only)
void vf_hook_2fcca0_m(uint32_t m) { vf_hook_2fcca0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
}  // namespace

// orig: (none) -- level 18's registrations
void veldin_finale_moby_fns_register() {
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002a7220), vf_class41_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002a76e0), vf_class49_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002a7b90), vf_class50_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002aa3b0), vf_class256_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ab7c0), vf_class1427_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002abaa0), vf_class1523_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ce7d0), vf_class331_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ce800), vf_class341_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ceaa0), empty_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d5050), vf_class564_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d5918), vf_class568_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d62e8), vf_class582_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6600), vf_class583_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d7b20), vf_class586_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d82c0), vf_class587_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d8ba8), vf_class605_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d8fd0), vf_class615_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002db460), vf_class624_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dc260), vf_class628_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dc918), vf_class638_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002df608), vf_class644_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dfda8), vf_class696_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e1508), vf_class830_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e19d8), vf_class758_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e1f40), vf_class805_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e4168), vf_class832_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e9768), vf_class885_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e9e70), vf_class983_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ec3e8), vf_class1021_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ec858), vf_class1039_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ecc40), vf_class1134_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002edb78), vf_class1135_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002efb88), vf_class1355_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f0920), vf_class1356_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f16f0), vf_class1381_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f1e68), vf_class1392_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f2310), vf_class1402_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f2bf0), vf_class1422_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f7ab0), vf_class1432_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f7ad8), vf_class1434_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f7c40), vf_class1454_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f88e8), vf_class1563_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fa640), vf_class1568_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fa728), vf_class1584_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fad08), vf_class1750_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fad28), vf_class1799_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fae48), vf_class1890_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb548), vf_class1898_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb868), vf_class1899_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fbb98), vf_class1906_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6108), vf_hook_2d6108_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d61e0), vf_hook_2d61e0_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6230), vf_hook_2d6230_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcbb0), vf_hook_2fcbb0_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcc50), vf_hook_2fcc50_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcca0), vf_hook_2fcca0_m);
    // (the classes' hook +0x14: the carried moby's reset after its explosion, explode_and_reset 0x2dc938, through the moby dispatch)
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6280), vf_hook_2d6280_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcd00), vf_hook_2fcd00_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d5768), vf_cb_2d5768_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d8098), vf_cb_2d8098_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d98e8), vf_cb_2d98e8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dba20), vf_cb_2dba20_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dc4b8), vf_cb_2dc4b8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e3f58), vf_cb_2e3f58_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002eb9f8), vf_cb_2eb9f8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ee490), vf_cb_2ee490_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f0390), vf_cb_2f0390_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f06c0), vf_cb_2f06c0_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f1fd8), vf_cb_2f1fd8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f2620), vf_cb_2f2620_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f27c8), vf_cb_2f27c8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f2970), vf_cb_2f2970_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f7880), vf_cb_2f7880_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9780), vf_cb_2f9780_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9a98), vf_cb_2f9a98_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9c20), vf_cb_2f9c20_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9eb8), vf_cb_2f9eb8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb018), vf_cb_2fb018_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb690), vf_cb_2fb690_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fc860), vf_cb_2fc860_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fca98), vf_cb_2fca98_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x0023d1a8), hud_slot7_draw_tr);  // (HUD slot 7's draw: Veldin's port, the level's copy is the same code; its start / update are Veldin's too)
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x0023cd60), vf_hud_boss_draw_l18);  // (the draw function of the HUD element the boss 1422 adds: its health bar)
}

}  // namespace rac1
