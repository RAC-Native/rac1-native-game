// Pokitaru (level 11) own moby classes: the registration table. The ports are the transliterations in
// pokitaru_classes_tr_N.cpp (declarations: pokitaru_mobys.h) and, for the classes several levels carry a copy of, the shared
// hand-written ports (shared_*.cpp, novalis_*.cpp). Keyed by level 11's own address and by level, so the indirect call (`jalr`) through
// moby +0x74) reaches them only while the played level's overlay is level 11's.
#include <game/ovl/pokitaru_hooks.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/shared_class1111.h>
#include <game/ovl/shared_class1179.h>
#include <game/ovl/shared_class1319.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class295.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_tiles.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L11:0x002fb780 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only; the Amoeboid's
// hook +0, every copy's port: novalis_hooks.cpp; the other argument registers zero, as the transliterated hook had them)
void pok_hook_2fb780_m(uint32_t m) { amoeboid_hook0_l1(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L11:0x002fb840 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void pok_hook_2fb840_m(uint32_t m) { pok_hook_2fb840_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L11:0x002fb890 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void pok_hook_2fb890_m(uint32_t m) { pok_hook_2fb890_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L11:0x00316160 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void pok_hook_316160_m(uint32_t m) { pok_hook_316160_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L11:0x00316270 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only; its port:
// carry_keep_state_hook on the carry hook +8, shared_little.cpp; the other argument registers zero, as the transliterated hook had them)
void pok_hook_316270_m(uint32_t m) { carry_keep_state_hook(m, 0, 0, 0.0f, 8); }

// orig: (none) -- adapter of L11:0x003162d0 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void pok_hook_3162d0_m(uint32_t m) { pok_hook_3162d0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

}  // namespace

// orig: (none) -- level 11's registrations
void pokitaru_moby_fns_register() {
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002cb668), pok_class23_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002cb790), raritanium_pickup_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002cb810), pok_class65_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002d0710), pok_class90_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002d0fa8), pok_class114_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002f0e40), pok_class298_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002f2518), pok_class318_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002f30b0), class341_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002f3350), pok_class361_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002f9998), amoeboid_update_l1);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002fc630), class604_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002fc980), zone_marker_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002fcda8), class615_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00300010), swingshot_target_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00300578), spawn_point_update_l1);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00302510), class819_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00302ae0), class830_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00303070), class832_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x003089d0), class1009_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00309098), class1017_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x003094f8), class1034_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00309ac0), pok_class1075_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030b358), class1111_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030b730), gold_bolt_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030c788), pok_class1156_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030d800), pok_class1157_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030e088), pok_class1158_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030e978), pok_class1159_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030eb90), pok_class1178_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030ee00), class1179_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030f2a8), pok_class1180_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030f728), pok_class1218_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00310028), pok_class1219_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00310180), pok_class1231_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00313290), pok_class1242_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00314318), pok_class1246_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00316320), pok_class1248_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x003172c0), pok_class1264_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x003181f0), pok_class1265_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00318b30), pok_class1297_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00319838), pok_class1319_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031aa80), pok_class1350_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031ab08), pok_class1524_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031b758), class1633_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031c800), sandmouse_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031d7d8), class1844_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031d8d0), empty_update);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031e2f0), pok_class1903_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031f150), pok_class1919_update_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002fb780), pok_hook_2fb780_m);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002fb840), pok_hook_2fb840_m);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002fb890), pok_hook_2fb890_m);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00316160), pok_hook_316160_m);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00316270), pok_hook_316270_m);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x003162d0), pok_hook_3162d0_m);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002f2d58), pok_cb_2f2d58_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x002fd6c0), class615_puzzle_draw);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00302e60), class832_draw);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030a908), class1111_draw);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0030e708), nov_water_751_draw_l1);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x00311d50), pok_cb_311d50_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x003192c8), class1319_trails_draw);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031c728), sandmouse_glow);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031db50), pok_cb_31db50_l11);
    ovl_register_level_moby_fn(POKITARU, L11_ADDR(0x0031e930), pok_cb_31e930_l11);
}

}  // namespace rac1
