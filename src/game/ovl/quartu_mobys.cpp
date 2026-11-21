// Quartu (level 15) own moby classes: the registration table. The ports are the transliterations in
// quartu_classes_tr_N.cpp (declarations: quartu_mobys.h) and, for the classes other levels share, the shared hand-written
// ports (shared_*.cpp, novalis_*.cpp: each proved against level 15's own copy by its family batch; Giant Clank's pad, class
// 1451, with the PAL 2.00 disc's changes as 1.02 Gameplay fix 23). Keyed by level 15's own address and by level, so the indirect call (`jalr`) through
// moby +0x74 (or a callback installed by a class) reaches them only while the played level's overlay is level 15's.
#include <game/ovl/quartu_hooks.h>
#include <game/ovl/quartu_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1179.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class1224.h>
#include <game/ovl/shared_class123.h>
#include <game/ovl/shared_class123_draw.h>
#include <game/ovl/shared_class1331.h>
#include <game/ovl/shared_class1371.h>
#include <game/ovl/shared_class1523.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class193.h>
#include <game/ovl/shared_class196.h>
#include <game/ovl/shared_class250.h>
#include <game/ovl/shared_class256.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class28.h>
#include <game/ovl/shared_class28_draw.h>
#include <game/ovl/shared_class325.h>
#include <game/ovl/shared_class403.h>
#include <game/ovl/shared_class408.h>
#include <game/ovl/shared_class408_tint.h>
#include <game/ovl/shared_class44.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class655.h>
#include <game/ovl/shared_class655_draw.h>
#include <game/ovl/shared_class679.h>
#include <game/ovl/shared_class77.h>
#include <game/ovl/shared_class78.h>
#include <game/ovl/shared_class78_draw.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_class934.h>
#include <game/ovl/shared_clank_pad.h>
#include <game/ovl/shared_floor_switch.h>
#include <game/ovl/shared_gadget_pickups.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_ripples.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_shockwave.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_three_glows.h>
#include <game/ovl/shared_waves.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L15:0x002bc4b0 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void qua_hook_2bc4b0_m(uint32_t m) { qua_hook_2bc4b0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L15:0x002bc550 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void qua_hook_2bc550_m(uint32_t m) { qua_hook_2bc550_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L15:0x002bc5a0 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void qua_hook_2bc5a0_m(uint32_t m) { qua_hook_2bc5a0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

}  // namespace

// orig: (none) -- level 15's registrations
void quartu_moby_fns_register() {
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002955c0), class28_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002979d8), class44_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029ac30), class49_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029aff0), qua_class67_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029d6c0), class256_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029ead0), shockwave_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029edb0), class1523_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a2488), class77_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a2bf0), class78_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a36d0), qua_class92_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a3ba8), qua_class93_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a6e68), class123_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a7880), qua_class148_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002aa280), qua_class154_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002aabb8), class156_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc608), class193_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bddb0), class196_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002be298), rock_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c2938), qua_class221_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c5630), qua_class233_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c6f08), class250_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c73b8), class258_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c9240), empty_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ca240), class325_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ca9f8), class403_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cb4c8), class408_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cbe58), empty_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cf3a8), qua_class491_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d3bd8), empty_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d4748), class567_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d5728), zone_marker_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d6810), class655_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d9100), class1179_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d95e8), class679_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002db100), swingshot_target_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002db668), spawn_point_update_l1);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002dd730), class819_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ddd00), class830_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002de290), class832_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002de430), class848_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e4380), class934_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e46b0), qua_class937_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e4770), gold_bolt_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e5958), floor_switch_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e5e70), class1221_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e5f58), class1221_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e6038), class1224_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e73c0), qua_class1250_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e7f68), qua_class1257_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e8910), class1331_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ea5e0), class1371_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ea748), qua_class1388_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eabd8), qua_class1394_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb0a0), class1408_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb458), class1409_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb4c0), qua_class1419_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb928), qua_class1425_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ebd78), class1428_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ec030), qua_class1430_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ec760), qua_class1446_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ed068), clank_pad_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ed398), qua_class1469_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edb20), qua_class1560_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edf58), class1565_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ee338), class1567_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x00296710), class28_draw_glows);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x00297808), class28_draw_spot);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029aba8), class44_glows_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a29b8), class77_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a3138), class78_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a7628), class123_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ca810), class325_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cbd88), class408_tint);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d77c0), class1108_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d8710), class655_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002de080), class832_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e6a10), class1224_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eac90), ripple_rings_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb108), ripple_pool_draw);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb6b8), qua_cb_2eb6b8_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edbe0), quartu_waves_draw_a);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edfc0), quartu_waves_draw_b);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc4b0), qua_hook_2bc4b0_m);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc550), qua_hook_2bc550_m);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc5a0), qua_hook_2bc5a0_m);
}

}  // namespace rac1
