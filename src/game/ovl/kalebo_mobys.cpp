// Kalebo III (level 16) own moby classes: the registration table. The ports are the transliterations in
// kalebo_classes_tr_N.cpp (declarations: kalebo_mobys.h). Keyed by level 16's own address and by level, so the indirect call (`jalr`) through
// moby +0x74 (or a callback installed by a class) reaches them only while the played level's overlay is level 16's.
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_class1111.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1356.h>
#include <game/ovl/shared_class1443.h>
#include <game/ovl/shared_class1952.h>
#include <game/ovl/shared_class235.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class52.h>
#include <game/ovl/shared_class638.h>
#include <game/ovl/shared_class933.h>
#include <game/ovl/shared_envmap.h>
#include <game/ovl/shared_envmap_meshes.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_race_hud.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/kalebo_hooks.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/shared_helpers.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L16:0x002e65d0 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void kal_hook_2e65d0_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 0, 7, 1); }

// orig: (none) -- adapter of L16:0x002e6670 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void kal_hook_2e6670_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 8, 7, 1); }

// orig: (none) -- adapter of L16:0x002e66c0 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void kal_hook_2e66c0_m(uint32_t m) { kal_hook_2e66c0_l16(m, 0, 0, 0.0f); }

}  // namespace

// Class 482: retires itself on its first update (the level places none; nothing it could do is left in its code).
// orig: L16:0x002cb600
void kal_class482_update_l16(uint32_t m) { moby_retire(m); }

// orig: (none) -- level 16's registrations
void kalebo_moby_fns_register() {
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a05e8), class49_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a0a98), class50_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a1088), class52_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a3f38), kal_class1475_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c3be8), class235_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c3d38), kal_class257_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c5eb0), kal_class281_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c7788), class439_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c9480), kal_class470_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c9878), kal_class471_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cb600), kal_class482_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cddb8), kal_class541_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cf198), kal_class546_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cf4a8), kal_class552_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d04a0), kal_class556_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d2a80), zone_marker_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d2cf0), class638_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d59e0), kal_class647_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d5cb8), kal_class650_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d5ef8), kal_class654_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d6378), swingshot_target_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d6740), kal_class800_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d69e0), spawn_point_update_l1);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002dddd8), empty_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002ddde0), class933_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002dee10), class1111_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002df1e8), gold_bolt_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e0120), teleporter_update_l1);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e0f78), class1139_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e1080), empty_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e22d8), class1356_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e3190), kal_class1377_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e36e8), kal_class1387_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e37a0), kal_class1401_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e43e0), kal_class1410_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e5010), kal_class1439_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e5258), kal_class1441_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e54a0), kal_class1442_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e5708), class1443_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e5e08), kal_class1445_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e6808), kal_class1455_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e7208), kal_class1561_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e76b8), kal_class1667_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e7e00), kal_class1812_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e7f80), kal_class1826_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e88d0), kal_class1891_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e8970), kal_class1923_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e8e80), kal_class1943_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e93b0), kal_class1947_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e98e0), envmap_1948_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9df0), class1952_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002ea128), envmap_1953_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002351f8), race_hud_start);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00235268), race_meter_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00235660), race_meter_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002388b0), race_hud_reset);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002388f8), race_hud_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00238a30), race_bar_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00238e20), race_hud_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c9cd0), kal_cb_2c9cd0_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cef60), kal_cb_2cef60_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cf7a8), kal_cb_2cf7a8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002de3c0), class1111_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e0a38), class1135_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e58d8), class1443_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e84e8), kal_cb_2e84e8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e8d40), envmap_1943_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9270), kal_cb_2e9270_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e97a0), envmap_1948_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9ce0), class1952_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9fe8), envmap_1953_draw);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e65d0), kal_hook_2e65d0_m);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e6670), kal_hook_2e6670_m);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e66c0), kal_hook_2e66c0_m);
}

}  // namespace rac1
