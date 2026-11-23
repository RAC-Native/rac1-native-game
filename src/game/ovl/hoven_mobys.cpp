// Hoven (level 12) own moby classes: the registration table. The ports are the level's own, written by hand (declarations:
// hoven_mobys.h). Keyed by level 12's own address and by level, so the indirect call (`jalr` through moby +0x74) reaches them only
// while the played level's overlay is level 12's.
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class1371.h>
#include <game/ovl/shared_class1400.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class886.h>
#include <game/ovl/shared_env_objects.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_homing_missile.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_tiles.h>
#include <game/ovl/hoven_hooks.h>
#include <game/ovl/hoven_mobys.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L12:0x002e16b8 (a class hook +0 of class 238: the indirect-call form, the moby only)
void hov_hook_2e16b8_m(uint32_t m) { hov_hook_2e16b8_l12(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L12:0x002e17a8 (a class hook +8 of class 238: the indirect-call form, the moby only; the shared
// carry_state_hook with the copy's own constants: hook 8, carried state 8, released 1)
void hov_hook_2e17a8_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 8, 8, 1); }

// orig: (none) -- adapter of L12:0x002e17f8 (a class hook +0xc of class 238: the indirect-call form, the moby only)
void hov_hook_2e17f8_m(uint32_t m) { hov_hook_2e17f8_l12(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L12:0x002e6b00 (a class hook +0xc of class 270, the Morph-o-Ray chicken: Veldin's 0x2caf10 code (state 5), which the overlay map pairs with 0x2d57d0 (state 9): the indirect-call form, the moby only)
void hov_hook_2e6b00_m(uint32_t m) { hov_hook_2e6b00_l12(m, 0, 0, 0.0f); }

}  // namespace

// orig: (none) -- level 12's registrations: the class updates, the class hooks +0 / +8 / +0xc, the draw-list callbacks
void hoven_moby_fns_register() {
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002bf140), hov_class19_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002bf9b8), raritanium_pickup_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002cb1e8), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002d30f8), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002d4378), shot_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e1be0), hov_class238_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e3ed8), hov_class240_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e4cd8), class258_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e6c48), hov_class282_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e7208), hov_class293_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e72c0), hov_class294_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e8c10), escalator_update_l2);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e9f68), hov_class326_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002eb570), hov_class328_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ebea8), hov_class336_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ec1d0), hov_class339_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ec478), class341_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ec718), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ec720), hov_class384_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ece00), hov_class409_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ed280), hov_class422_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ef648), homing_missile_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002f11b0), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002f51f0), class604_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002f5540), zone_marker_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002f7b28), swingshot_target_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002f8090), spawn_point_update_l1);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002fa238), class832_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ff838), class886_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ffdb0), gold_bolt_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00300ce8), teleporter_update_l1);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00301d08), class1221_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00301df0), class1221_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00302f30), hov_class1259_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00303540), hov_class1267_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00304e00), hov_class1269_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003069d0), hov_class1274_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003076d0), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003076d8), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00307840), hov_class1281_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003081a8), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003081b0), hov_class1345_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00308918), class1371_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00308be0), class1400_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003093c8), hov_class1404_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003094a0), hov_class1557_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00309648), class1633_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x0030a6f0), sandmouse_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x0030b6c8), class1844_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x0030b7c0), empty_update);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x0030bff0), hov_class1901_update_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e16b8), hov_hook_2e16b8_m);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e17a8), hov_hook_2e17a8_m);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e17f8), hov_hook_2e17f8_m);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e6b00), hov_hook_2e6b00_m);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002bf748), nov_water_751_draw_l1);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002e71b0), env_objects_callback);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002ecac0), hov_cb_2ecac0_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x002fa028), class832_draw);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00301600), class1135_draw);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x003028c8), hov_cb_3028c8_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00304218), hov_cb_304218_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00304d98), hov_cb_304d98_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x00308350), hov_cb_308350_l12);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x0030a618), sandmouse_glow);
    ovl_register_level_moby_fn(HOVEN, L12_ADDR(0x0030be68), hov_cb_30be68_l12);
}

}  // namespace rac1
