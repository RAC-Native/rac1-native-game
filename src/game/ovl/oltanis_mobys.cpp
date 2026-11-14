// Oltanis (level 14) own moby classes: the registration table. The ports are the transliterations in
// oltanis_classes_tr_N.cpp (declarations: oltanis_mobys.h). Keyed by level 14's own address and by level, so the indirect call (`jalr`)
// through moby +0x74 (or a draw list) reaches them only while the played level's overlay is level 14's.
#include <game/ovl/oltanis_hooks.h>
#include <game/ovl/oltanis_mobys.h>

#include <game/ovl/carry_hooks.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L14:0x002dad60 (class 252's hook +0: the indirect-call form, the moby only)
void olt_hook_2dad60_m(uint32_t m) { enemy10e_hook0(m, 0, 0, 0.0f); }  // (Veldin's 0x2cadc0, the same code)

// orig: (none) -- adapter of L14:0x002dae00 (class 252's hook +8: the indirect-call form, the moby only)
void olt_hook_2dae00_m(uint32_t m) { olt_hook_2dae00_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L14:0x002dae50 (class 252's hook +0xc: the indirect-call form, the moby only)
void olt_hook_2dae50_m(uint32_t m) { olt_hook_2dae50_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
// orig: (none) -- adapter of L14:0x002e81a0 (a moby function: the indirect-call form, the moby only)
void olt_fn_2e81a0_m(uint32_t m) { olt_fn_2e81a0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
}  // namespace

// orig: (none) -- level 14's registrations
void oltanis_moby_fns_register() {
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002aba80), olt_class903_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ac618), olt_class8_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002b17d8), olt_class28_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002b3bf0), olt_class30_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002b46e8), olt_class31_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002bac78), olt_class81_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002d5f40), olt_class211_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002d96e0), olt_class250_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002d98f0), olt_class252_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002db378), olt_class258_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002de1f8), olt_class309_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002de670), olt_class325_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002dee28), olt_class386_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002dfd10), olt_class403_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002e1b20), olt_class455_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002e7190), empty_update);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002e72d8), olt_class545_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002e7bd8), olt_class557_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ea9d0), olt_class604_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ead20), olt_class605_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002eaf88), olt_class610_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ec810), olt_class643_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ece00), olt_class681_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ed280), olt_class684_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ee8d8), olt_class685_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ef840), olt_class686_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002f0538), olt_class712_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002f2cb8), olt_class758_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002f3ef0), empty_update);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002f4190), olt_class805_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002f6338), olt_class832_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002fba20), olt_class851_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002fc0f0), olt_class908_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002fcba8), empty_update);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002fcbb0), olt_class921_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002fdbb8), olt_class922_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002fe2a0), olt_class923_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002fefe0), olt_class924_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ff640), olt_class933_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ffc18), olt_class1134_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00300e00), olt_class1193_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x003015d0), olt_class1224_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00303370), olt_class1260_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x003039e0), olt_class1331_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00305408), olt_class1352_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00305758), olt_class1354_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00305d28), olt_class1395_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00306150), empty_update);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x003061d8), olt_class1397_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00306390), olt_class1400_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00306b78), olt_class1416_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00306ee0), olt_class1417_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00307a80), olt_class1418_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00307ad8), olt_class1511_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00308350), empty_update);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x003087c0), olt_class1559_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x003088b0), olt_class1629_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00308be0), olt_class1633_update_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00309c88), olt_class1818_update_l14);
    // the classes' draw-list callbacks (queued with the draw-list appenders L14:0x20c5d0, 0x20c698, 0x20c760, 0x20c828; run by the frame's
    // draw lists), the cutscene extras' draw (0x2459c0, Veldin's 0x23db30 at level 14's addresses) and the moby function 0x2e81a0 (it puts its own
    // address at Veldin's 0x160770, the pointer the bombs compare a moby's update with)
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002459c0), olt_cb_2459c0_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002aee28), olt_cb_2aee28_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002b2928), olt_cb_2b2928_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002d71f0), olt_cb_2d71f0_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002da678), olt_cb_2da678_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002daf08), olt_cb_2daf08_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002dec40), olt_cb_2dec40_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002df458), olt_cb_2df458_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002eb810), olt_cb_2eb810_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002edc18), olt_cb_2edc18_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002f6128), olt_cb_2f6128_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00301fa8), olt_cb_301fa8_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00303350), olt_cb_303350_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00307a28), olt_cb_307a28_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00308150), olt_cb_308150_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x00309bb0), olt_cb_309bb0_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002ae260), olt_cb_2ae260_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002b3a20), olt_cb_2b3a20_l14);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002e81a0), olt_fn_2e81a0_m);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002dad60), olt_hook_2dad60_m);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002dae00), olt_hook_2dae00_m);
    ovl_register_level_moby_fn(OLTANIS, L14_ADDR(0x002dae50), olt_hook_2dae50_m);
}

}  // namespace rac1
