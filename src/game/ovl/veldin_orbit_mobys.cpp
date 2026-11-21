// Drek's fleet (level 17) own moby classes: the registration table. The ports are the transliterations in
// veldin_orbit_classes_tr_N.cpp (declarations: veldin_orbit_mobys.h) and, for the classes several levels carry a copy of, the
// shared hand-written ports (shared_*.cpp, novalis_*.cpp). Keyed by level 17's own address and by level, so the indirect call (`jalr`)
// through moby +0x74 (and the draw lists' callbacks) reaches them only while the played level's overlay is level 17's.
#include <game/ovl/veldin_orbit_hooks.h>
#include <game/ovl/veldin_orbit_mobys.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_class1021.h>
#include <game/ovl/shared_class1038.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1224.h>
#include <game/ovl/shared_class123_draw.h>
#include <game/ovl/shared_class123.h>
#include <game/ovl/shared_class1319.h>
#include <game/ovl/shared_class1378.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class1952.h>
#include <game/ovl/shared_class196.h>
#include <game/ovl/shared_class224.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class295.h>
#include <game/ovl/shared_class408.h>
#include <game/ovl/shared_class408_tint.h>
#include <game/ovl/shared_class44.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class655_draw.h>
#include <game/ovl/shared_class655.h>
#include <game/ovl/shared_class77.h>
#include <game/ovl/shared_class78_draw.h>
#include <game/ovl/shared_class78.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class934.h>
#include <game/ovl/shared_envmap_meshes.h>
#include <game/ovl/shared_floor_switch.h>
#include <game/ovl/shared_gadget_pickups.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_ripples.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_three_glows.h>
#include <game/ovl/shared_waves.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/hud_slot7_tr.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L17:0x002efea8 (class 1382's hook +0: the indirect-call form, the moby only)
// (its port: carry_drop_hook on the carry hook +0, shared_little.cpp; the other argument registers zero, as the transliterated hook had them)
void vo_hook_2efea8_m(uint32_t m) { carry_drop_hook(m, 0, 0, 0.0f, 0); }

// orig: (none) -- adapter of L17:0x002effb8 (class 1382's hook +8: the indirect-call form, the moby only)
// (its port: carry_drop_hook on the carry hook +8)
void vo_hook_2effb8_m(uint32_t m) { carry_drop_hook(m, 0, 0, 0.0f, 8); }

// orig: (none) -- adapter of L17:0x002f0040 (class 1382's hook +0xc: the indirect-call form, the moby only)
// (its port: vo_carry_hookc_l17, carry_hooks_levels.cpp; the other argument registers zero, as the transliterated hook had them)
void vo_hook_2f0040_m(uint32_t m) { vo_carry_hookc_l17(m, 0, 0, 0.0f); }
}  // namespace

// orig: (none) -- level 17's registrations
void veldin_orbit_moby_fns_register() {
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x0029f8d8), class44_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a2b30), class49_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a7b58), class77_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a82c0), class78_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a8da0), vo_class99_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002ac740), class123_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002af608), class156_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c0f00), class196_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c53d8), class224_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c7cb0), class258_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c9ce8), class295_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002cb310), vo_class347_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002cbe80), class408_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d2dc0), class567_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d3da0), zone_marker_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d4e88), class655_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d77f0), vo_class669_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d95a8), swingshot_target_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d9b10), spawn_point_update_l1);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002dbaa8), class819_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002dc078), class830_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002dc3f8), vo_class835_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e1c20), class934_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e22b0), class1009_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e2978), class1017_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e2c58), class1021_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e30c8), class1034_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e3690), class1038_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e3d38), class1040_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e3ff8), gold_bolt_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e4f30), teleporter_update_l1);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e5f50), floor_switch_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e6468), class1224_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e87d8), vo_class1359_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e8e08), vo_class1368_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e9608), class1378_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002ed018), vo_class1379_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002ee2b0), vo_class1380_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002eeb68), vo_class1382_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f08e0), class1405_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0ca0), class1406_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1060), class1407_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1418), class1426_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1790), class1428_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1940), vo_class1448_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f26d0), vo_class1470_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f2e78), vo_class1562_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f3848), vo_class1772_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f40d8), vo_class1843_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5260), vo_class1873_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5388), empty_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5390), vo_class1876_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f54d0), empty_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f57a0), class1907_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5ab8), class1908_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5dd0), class1910_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f60e8), class1911_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f6400), class1913_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f6718), class1918_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002efea8), vo_hook_2efea8_m);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002effb8), vo_hook_2effb8_m);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0040), vo_hook_2f0040_m);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a2aa8), class44_glows_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a8088), class77_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a8808), class78_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a95b8), vo_cb_2a95b8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002acf00), class123_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002cc740), class408_tint);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d5e38), class1108_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d6d88), class655_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d7cf0), vo_cb_2d7cf0_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e5848), class1135_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e6e40), class1224_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e9798), class1378_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002eb9a8), vo_cb_2eb9a8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0448), class1382_glows_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f04d0), ripple_rings_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0948), fleet_waves_draw_a);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0d08), fleet_waves_draw_b);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f10c8), ripple_pool_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f32f0), vo_cb_2f32f0_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f3b68), class1319_trails_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5690), class1907_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f59a8), class1908_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5cc0), class1910_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5fd8), class1911_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f62f0), class1913_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f6608), class1918_draw);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002355b8), hud_slot7_draw_tr);  // (HUD slot 7's draw: Veldin's port, the level's copy is the same code; its start / update are Veldin's too)
}

}  // namespace rac1
