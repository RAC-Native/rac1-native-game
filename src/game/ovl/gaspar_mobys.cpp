// Gaspar (level 9) own moby classes: the registration table. The ports are the level's own, written by hand (declarations: gaspar_mobys.h).
// Keyed by level 9's own address and by level, so the indirect call (`jalr`) through moby +0x74) reaches them only while the played level's
// overlay is level 9's.
#include <game/ovl/aridia_ocean.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_class1023.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class1885.h>
#include <game/ovl/shared_class1886.h>
#include <game/ovl/shared_class193.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class52.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_tracer.h>
#include <game/ovl/gaspar_hooks.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/shared_helpers.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L9:0x002e2680 (class 193's hook +0: the indirect-call form, the moby only)
void gas_hook_2e2680_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 0, 7, 1); }

// orig: (none) -- adapter of L9:0x002e2720 (class 193's hook +8: the indirect-call form, the moby only)
void gas_hook_2e2720_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 8, 7, 1); }

// orig: (none) -- adapter of L9:0x002e2770 (class 193's hook +0xc: the indirect-call form, the moby only)
void gas_hook_2e2770_m(uint32_t m) { gas_hook_2e2770_l9(m, 0, 0, 0.0f); }

}  // namespace

// orig: (none) -- level 9's registrations
void gaspar_moby_fns_register() {
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002c26c8), gas_class1182_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002c5990), class52_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002d0d88), class156_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e27d8), class193_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ea338), class258_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002eb970), gas_class263_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002edc30), gas_class276_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ee110), gas_class279_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ee428), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef430), gas_class313_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef7f8), sea_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef9b0), gas_class320_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002efe40), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f0040), gas_class417_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f6678), class567_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f7658), zone_marker_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f86a0), gas_class664_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f8c70), swingshot_target_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002f90e0), spawn_point_update_l1);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002fb288), class832_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00300888), gas_class1000_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003009d0), class1023_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003022f0), gold_bolt_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003033a0), gas_class1150_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00303d10), gas_class1172_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00304360), gas_class1181_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00304c80), gas_class1201_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00305a28), gas_class1206_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00305dc0), bg_ship1_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00306720), bg_ship2_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00307d68), shell_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003082d8), gas_class1285_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00308f08), tracer_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x003091b0), gas_class1293_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00309828), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00309c08), gas_class1766_update_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030a678), class1844_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030a770), empty_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030ab80), class1885_update);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x0030b3b0), class1886_update);
    // (task 0418) the classes' draw-list callbacks (queued with draw_list_add / L9:0x218748, 0x218810, 0x218950; run by the frame's draw lists)
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002ef750), gas_class317_draw_l9);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002fb078), class832_draw);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00305c10), ari_bg_ship1_glow_draw_l2);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x00306570), ari_bg_ship2_glow_draw_l2);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e2680), gas_hook_2e2680_m);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e2720), gas_hook_2e2720_m);
    ovl_register_level_moby_fn(GASPAR, L9_ADDR(0x002e2770), gas_hook_2e2770_m);
}

}  // namespace rac1
