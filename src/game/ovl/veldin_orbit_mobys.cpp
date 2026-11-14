// Drek's fleet (level 17) own moby classes: the registration table. The ports are the transliterations in
// veldin_orbit_classes_tr_N.cpp (declarations: veldin_orbit_mobys.h). Keyed by level 17's own address and by level, so the indirect call (`jalr`)
// through moby +0x74 (and the draw lists' callbacks) reaches them only while the played level's overlay is level 17's.
#include <game/ovl/veldin_orbit_hooks.h>
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/hud_slot7_tr.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L17:0x002efea8 (class 1382's hook +0: the indirect-call form, the moby only)
void vo_hook_2efea8_m(uint32_t m) { vo_hook_2efea8_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L17:0x002effb8 (class 1382's hook +8: the indirect-call form, the moby only)
void vo_hook_2effb8_m(uint32_t m) { vo_hook_2effb8_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L17:0x002f0040 (class 1382's hook +0xc: the indirect-call form, the moby only)
void vo_hook_2f0040_m(uint32_t m) { vo_hook_2f0040_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
}  // namespace

// orig: (none) -- level 17's registrations
void veldin_orbit_moby_fns_register() {
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x0029f8d8), vo_class44_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a2b30), vo_class49_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a7b58), vo_class77_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a82c0), vo_class78_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a8da0), vo_class99_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002ac740), vo_class123_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002af608), vo_class156_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c0f00), vo_class197_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c53d8), vo_class224_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c7cb0), vo_class258_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002c9ce8), vo_class295_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002cb310), vo_class347_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002cbe80), vo_class408_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d2dc0), vo_class567_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d3da0), vo_class605_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d4e88), vo_class655_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d77f0), vo_class669_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d95a8), vo_class758_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d9b10), vo_class805_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002dbaa8), vo_class819_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002dc078), vo_class830_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002dc3f8), vo_class835_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e1c20), vo_class934_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e22b0), vo_class1009_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e2978), vo_class1017_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e2c58), vo_class1021_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e30c8), vo_class1034_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e3690), vo_class1038_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e3d38), vo_class1040_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e3ff8), vo_class1134_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e4f30), vo_class1135_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e5f50), vo_class1209_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e6468), vo_class1224_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e87d8), vo_class1359_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e8e08), vo_class1368_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e9608), vo_class1378_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002ed018), vo_class1379_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002ee2b0), vo_class1380_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002eeb68), vo_class1382_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f08e0), vo_class1405_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0ca0), vo_class1406_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1060), vo_class1407_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1418), vo_class1426_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1790), vo_class1428_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f1940), vo_class1448_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f26d0), vo_class1470_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f2e78), vo_class1562_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f3848), vo_class1772_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f40d8), vo_class1843_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5260), vo_class1873_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5388), empty_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5390), vo_class1876_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f54d0), empty_update);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f57a0), vo_class1907_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5ab8), vo_class1908_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5dd0), vo_class1910_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f60e8), vo_class1911_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f6400), vo_class1913_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f6718), vo_class1918_update_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002efea8), vo_hook_2efea8_m);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002effb8), vo_hook_2effb8_m);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0040), vo_hook_2f0040_m);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a2aa8), vo_cb_2a2aa8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a8088), vo_cb_2a8088_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a8808), vo_cb_2a8808_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002a95b8), vo_cb_2a95b8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002acf00), vo_cb_2acf00_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002cc740), vo_cb_2cc740_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d5e38), vo_cb_2d5e38_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d6d88), vo_cb_2d6d88_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002d7cf0), vo_cb_2d7cf0_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e5848), vo_cb_2e5848_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e6e40), vo_cb_2e6e40_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002e9798), vo_cb_2e9798_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002eb9a8), vo_cb_2eb9a8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0448), vo_cb_2f0448_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f04d0), vo_cb_2f04d0_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0948), vo_cb_2f0948_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f0d08), vo_cb_2f0d08_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f10c8), vo_cb_2f10c8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f32f0), vo_cb_2f32f0_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f3b68), vo_cb_2f3b68_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5690), vo_cb_2f5690_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f59a8), vo_cb_2f59a8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5cc0), vo_cb_2f5cc0_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f5fd8), vo_cb_2f5fd8_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f62f0), vo_cb_2f62f0_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002f6608), vo_cb_2f6608_l17);
    ovl_register_level_moby_fn(VELDIN_ORBIT, L17_ADDR(0x002355b8), hud_slot7_draw_tr);  // (HUD slot 7's draw: Veldin's port, the level's copy is the same code; its start / update are Veldin's too)
}

}  // namespace rac1
