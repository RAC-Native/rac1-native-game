// Gaspar (level 9) own moby classes: the registration table. The ports are the transliterations in
// gaspar_classes_tr_N.cpp (declarations: gaspar_mobys.h). Keyed by level 9's own address and by level, so the indirect call (`jalr`) through
// moby +0x74) reaches them only while the played level's overlay is level 9's.
#include <game/ovl/gaspar_hooks.h>
#include <game/ovl/gaspar_mobys.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L9:0x002e2680 (class 193's hook +0: the indirect-call form, the moby only)
void gas_hook_2e2680_m(uint32_t m) { gas_hook_2e2680_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L9:0x002e2720 (class 193's hook +8: the indirect-call form, the moby only)
void gas_hook_2e2720_m(uint32_t m) { gas_hook_2e2720_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L9:0x002e2770 (class 193's hook +0xc: the indirect-call form, the moby only)
void gas_hook_2e2770_m(uint32_t m) { gas_hook_2e2770_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

}  // namespace

// orig: (none) -- level 9's registrations
void gaspar_moby_fns_register() {
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002c26c8), gas_class1182_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002c5990), gas_class52_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002d0d88), gas_class156_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e27d8), gas_class193_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ea338), gas_class258_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002eb970), gas_class263_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002edc30), gas_class276_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ee110), gas_class279_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ee428), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef430), gas_class313_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef7f8), gas_class317_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef9b0), gas_class320_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002efe40), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f0040), gas_class417_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f6678), gas_class567_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f7658), gas_class605_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f86a0), gas_class664_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f8c70), gas_class803_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f90e0), gas_class805_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002fb288), gas_class832_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00300888), gas_class1000_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003009d0), gas_class1023_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003022f0), gas_class1134_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003033a0), gas_class1150_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00303d10), gas_class1172_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00304360), gas_class1181_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00304c80), gas_class1201_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00305a28), gas_class1206_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00305dc0), gas_class1212_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00306720), gas_class1213_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00307d68), gas_class1258_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003082d8), gas_class1285_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00308f08), gas_class1292_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003091b0), gas_class1293_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00309828), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00309c08), gas_class1766_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030a678), gas_class1849_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030a770), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030ab80), gas_class1885_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030b3b0), gas_class1886_update_l9);
    // (task 0418) the classes' draw-list callbacks (queued with draw_list_add / L9:0x218748, 0x218810, 0x218950; run by the frame's draw lists)
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef750), gas_class317_draw_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002fb078), gas_class832_draw_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00305c10), gas_class1212_draw_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00306570), gas_class1213_draw_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e2680), gas_hook_2e2680_m);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e2720), gas_hook_2e2720_m);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e2770), gas_hook_2e2770_m);
}

}  // namespace rac1
