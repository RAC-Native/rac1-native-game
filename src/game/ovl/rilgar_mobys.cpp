// Rilgar's own moby classes: the registration table. The classes several levels share run their hand-written ports
// (shared_*.cpp, novalis_*.cpp); the level's own are the transliterations in rilgar_classes_tr_N.cpp (declarations:
// rilgar_mobys.h); classes whose update is a bare `jr ra` are empty here.
// Keyed by Rilgar's own address and by level, so the indirect call (`jalr` through moby +0x74) reaches them only
// while the played level's overlay is Rilgar's (RAC1_OWN_OVERLAY=1 RAC1_LEVEL=5).
#include <game/ovl/rilgar_mobys.h>

#include <initializer_list>

#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_part2.h>
#include <game/ovl/shared_class1511.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class679.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/shared_particles.h>
#include <game/ovl/shared_race_hud.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_swingshot_target.h>

namespace rac1 {

namespace {
// orig: L5:0x0031ace8 (also 0x0031d158: classes 1140, 1514) -- a bare `jr ra`
void ril_empty_update_l5(uint32_t) {}

// orig: (none) -- adapter of L5:0x00300b98 (class hook +8: the indirect-call form, the moby only)
void ril_hook_300b98_m(uint32_t m) { ril_carry_hook8_l5(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L5:0x00300be8 (class hook +0xc: the indirect-call form, the moby only)
void ril_hook_300be8_m(uint32_t m) { ril_carry_hookc_l5(m, 0, 0, 0.0f); }
}  // namespace

// orig: (none) -- Rilgar's registrations
void rilgar_moby_fns_register() {
    for (const uint32_t a : {L5_ADDR(0x0031ace8u), L5_ADDR(0x0031d158u)}) ovl_register_level_moby_fn(RILGAR, a, ril_empty_update_l5);
    // the classes several levels share, ported once (shared_*.cpp, novalis_*.cpp; each proved against Rilgar's own copy)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002f5200), class258_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002f8080), class341_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002f87a8), class439_update);  // (the race's hoverboard)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002fecf0), amoeboid_update_l1);  // (class 572)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00301988), class604_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00301cd8), zone_marker_update);  // (class 605)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003067b8), class679_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003098d8), swingshot_target_update);  // (class 758)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00309e40), spawn_point_update_l1);  // (class 805)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030c0a8), class823_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030c5b0), class830_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030dac8), class832_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00317aa0), class447_update);  // (class 920)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003186f8), class344_update);  // (class 980)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00319c60), gold_bolt_update);  // (class 1134)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031abe0), class1139_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031c8e0), class1511_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031d760), class1633_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031e808), sandmouse_update);  // (class 1818)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030d678), nov_water_751_draw_l1);  // (class 831's draw-list callback)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030d8b8), class832_draw);  // (its draw-list callback)
    // the race's HUD elements (shared with Kalebo III): start, reset, update, draw of the lap / time element, the trick meter's
    // update and draw, the bottom bar. Both PAL builds changed the meter's draw and the bar (their texts from the level's text
    // table: the port's region fork) and they carry 1.02 Fixes' fix 19 (shared_race_hud.cpp; task H3)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00262ae8), race_hud_start);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00262b58), race_meter_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00262f50), race_meter_draw);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00266320), race_bar_draw);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002661a0), race_hud_reset);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002661e8), race_hud_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00266710), race_hud_draw);
    // particle updates (the same code as Veldin's / the shared kinds)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0029bda0), bullet_particle_update);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002a00f0), beam_particle_update);
    // the transliterated classes (rilgar_classes_tr_N.cpp, the translator)
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002d1688), ril_class35_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002d7140), ril_class79_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002dac80), ril_class133_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00301f48), ril_class623_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00304320), ril_class625_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00307910), ril_class717_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030bdd8), ril_class810_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030bf98), ril_class812_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030ca80), ril_class831_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030dc68), ril_class838_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030e1f8), ril_class841_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030e508), ril_class843_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030e7f8), ril_class844_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030f5c8), ril_class846_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00314eb0), ril_class852_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003150f0), ril_class855_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003156d0), ril_class877_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00316110), ril_class879_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00316258), ril_class893_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003166a0), ril_class895_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00316ab8), ril_class918_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00317470), ril_class919_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003180a0), ril_class925_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00318a68), ril_class982_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00318b98), ril_class984_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00318c78), ril_class998_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00319c28), ril_class1099_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031bf40), ril_class1347_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031d5c8), ril_class1550_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00300b98), ril_hook_300b98_m);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00300be8), ril_hook_300be8_m);
    rilgar_part2_register();  // (the classes' draw callbacks)
}

}  // namespace rac1
