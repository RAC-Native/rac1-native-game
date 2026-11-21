// Veldin, the finale (level 18) own moby classes: the registration table. The ports are the transliterations in
// veldin_finale_classes_tr_N.cpp (declarations: veldin_finale_mobys.h), the level's own hand-written classes (veldin_finale_shots.cpp,
// _walking_bomb.cpp, _rail_switch.cpp, _gate.cpp, _platforms.cpp: classes 41, 564, 568, 582, 583, 586, 587, 624, 628 and class
// 568's carry hooks; the files named in veldin_finale_mobys.h for the others) and, for the classes other levels share, the shared
// hand-written ports (shared_*.cpp, novalis_*.cpp, aridia_props.cpp: each proved against level 18's own copy by its family
// batch; the adapters below pass a copy's own constants; Giant Clank's pad, class 1899, with the PAL 2.00 disc's changes as
// 1.02 Gameplay fix 23). Keyed by level 18's own address and by level, so the indirect call (`jalr`)
// through moby +0x74 (and the draw lists' callbacks) reaches them only while the played level's overlay is level 18's.
#include <game/ovl/veldin_finale_hooks.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_boss_bar.h>
#include <game/ovl/shared_class1021.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1356.h>
#include <game/ovl/shared_class1443.h>
#include <game/ovl/shared_class1523.h>
#include <game/ovl/shared_class256.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class638.h>
#include <game/ovl/shared_class655_draw.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_clank_pad.h>
#include <game/ovl/shared_finale.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_shockwave.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_zones.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/hud_slot7_tr.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L18:0x002d6108 (class 568's hook +0, veldin_finale_walking_bomb.cpp: the indirect-call form, the moby only)
void vf_hook_2d6108_m(uint32_t m) { vf_bomb_hook0_l18(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L18:0x002d61e0 (class 568's hook +8, the shared port: the indirect-call form, the moby only)
void vf_hook_2d61e0_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 8, 4, -1); }

// orig: (none) -- adapter of L18:0x002d6230 (class 568's hook +0xc, veldin_finale_walking_bomb.cpp: the indirect-call form, the moby only)
void vf_hook_2d6230_m(uint32_t m) { vf_bomb_hookc_l18(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L18:0x002fcbb0 (class 1906's hook +0, the shared port: the indirect-call form, the moby only)
void vf_hook_2fcbb0_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 0, 7, 2); }

// orig: (none) -- adapter of L18:0x002fcc50 (class 1906's hook +8, the shared port: the indirect-call form, the moby only)
void vf_hook_2fcc50_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 8, 7, 2); }

// orig: (none) -- adapter of L18:0x002fcca0 (class 1906's hook +0xc, veldin_finale_robots.cpp: the indirect-call form, the moby only)
void vf_hook_2fcca0_m(uint32_t m) { vf_robot_hookc_l18(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L18:0x002d6280 (class 568's hook +0x14, the shared port with the copy's state 5)
void vf_finale_stop_hook_5_m(uint32_t m) { finale_stop_hook(m, 5); }

// orig: (none) -- adapter of L18:0x002fcd00 (class 1906's hook +0x14, the shared port with the copy's state 8)
void vf_finale_stop_hook_8_m(uint32_t m) { finale_stop_hook(m, 8); }

// orig: (none) -- adapters of L18:0x002f2620 / 0x002f27c8 / 0x002f2970 (class 1402's draw callbacks: the shared port with
// the copies' quad sets 0, 1, 2)
void vf_finale_quads_0_m(uint32_t m) { finale_quads_draw(0, m); }
void vf_finale_quads_1_m(uint32_t m) { finale_quads_draw(1, m); }
void vf_finale_quads_2_m(uint32_t m) { finale_quads_draw(2, m); }
}  // namespace

// orig: (none) -- level 18's registrations
void veldin_finale_moby_fns_register() {
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002a7220), vf_class41_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002a76e0), class49_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002a7b90), class50_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002aa3b0), class256_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ab7c0), shockwave_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002abaa0), class1523_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ce7d0), trackdrive_left_update_l2);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ce800), class341_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ceaa0), empty_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d5050), vf_class564_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d5918), vf_class568_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d62e8), vf_class582_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6600), vf_class583_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d7b20), vf_class586_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d82c0), vf_class587_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d8ba8), zone_marker_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d8fd0), class615_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002db460), vf_class624_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dc260), vf_class628_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dc918), class638_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002df608), vf_class644_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dfda8), rock_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e1508), class830_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e19d8), swingshot_target_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e1f40), spawn_point_update_l1);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e4168), class832_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e9768), vf_class885_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e9e70), vf_class983_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ec3e8), class1021_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ec858), deadly_zones_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ecc40), gold_bolt_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002edb78), teleporter_update_l1);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002efb88), vf_class1355_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f0920), class1356_update);
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
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fae48), class1443_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb548), flash_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb868), clank_pad_update);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fbb98), vf_class1906_update_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6108), vf_hook_2d6108_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d61e0), vf_hook_2d61e0_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6230), vf_hook_2d6230_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcbb0), vf_hook_2fcbb0_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcc50), vf_hook_2fcc50_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcca0), vf_hook_2fcca0_m);
    // (the classes' hook +0x14: the carried moby's reset after its explosion, explode_and_reset 0x2dc938, through the moby dispatch)
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d6280), vf_finale_stop_hook_5_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fcd00), vf_finale_stop_hook_8_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d5768), vf_cb_2d5768_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d8098), class1108_draw);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002d98e8), class615_puzzle_draw);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dba20), vf_cb_2dba20_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002dc4b8), vf_cb_2dc4b8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002e3f58), class832_draw);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ea1f8), vf_cb_2ea1f8_l18);  // (class 983's glow: queued by its update, served by nothing before P18.5)
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002eb9f8), class655_draw);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002ee490), class1135_draw);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f0390), vf_cb_2f0390_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f06c0), vf_cb_2f06c0_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f1fd8), vf_cb_2f1fd8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f2620), vf_finale_quads_0_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f27c8), vf_finale_quads_1_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f2970), vf_finale_quads_2_m);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f7880), vf_cb_2f7880_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9780), vf_cb_2f9780_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9a98), vf_cb_2f9a98_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9c20), vf_cb_2f9c20_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002f9eb8), vf_cb_2f9eb8_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb018), class1443_draw);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fb690), flash_draw_quad);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fc860), vf_cb_2fc860_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x002fca98), vf_cb_2fca98_l18);
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x0023d1a8), hud_slot7_draw_tr);  // (HUD slot 7's draw: Veldin's port, the level's copy is the same code; its start / update are Veldin's too)
    ovl_register_level_moby_fn(VELDIN_FINALE, L18_ADDR(0x0023cd60), boss_health_bar_draw);  // (the draw function of the HUD element the boss 1422 adds: its health bar)
}

}  // namespace rac1
