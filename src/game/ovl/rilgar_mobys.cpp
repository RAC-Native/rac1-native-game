// Rilgar's own moby classes: the registration table. The ports are the transliterations in
// rilgar_classes_tr_N.cpp (declarations: rilgar_mobys.h); classes whose update is a bare `jr ra` are empty here.
// Keyed by Rilgar's own address and by level, so the indirect call (`jalr` through moby +0x74) reaches them only
// while the played level's overlay is Rilgar's (RAC1_OWN_OVERLAY=1 RAC1_LEVEL=5).
#include <game/ovl/rilgar_mobys.h>

#include <initializer_list>

#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_part2.h>

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
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002d1688), ril_class35_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002d7140), ril_class79_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002dac80), ril_class133_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002f5200), ril_class258_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002f8080), ril_class341_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002f87a8), ril_class439_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002fecf0), ril_class572_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00301988), ril_class604_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00301cd8), ril_class605_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00301f48), ril_class623_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00304320), ril_class625_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003067b8), ril_class679_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00307910), ril_class717_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003098d8), ril_class758_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00309e40), ril_class805_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030bdd8), ril_class810_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030bf98), ril_class812_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030c0a8), ril_class823_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030c5b0), ril_class830_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030ca80), ril_class831_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030dac8), ril_class832_update_l5);
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
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00317aa0), ril_class920_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003180a0), ril_class925_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x003186f8), ril_class980_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00318a68), ril_class982_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00318b98), ril_class984_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00318c78), ril_class998_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00319c28), ril_class1099_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00319c60), ril_class1134_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031abe0), ril_class1139_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031bf40), ril_class1347_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031c8e0), ril_class1511_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031d5c8), ril_class1550_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031d760), ril_class1633_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0031e808), ril_class1818_update_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00300b98), ril_hook_300b98_m);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00300be8), ril_hook_300be8_m);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030d678), ril_cb_30d678_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0030d8b8), ril_cb_30d8b8_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00262ae8), ril_cb_262ae8_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00262b58), ril_cb_262b58_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00262f50), ril_cb_262f50_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002661a0), ril_cb_2661a0_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002661e8), ril_cb_2661e8_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00266320), ril_cb_266320_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x00266710), ril_cb_266710_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x0029bda0), ril_cb_29bda0_l5);
    ovl_register_level_moby_fn(RILGAR, L5_ADDR(0x002a00f0), ril_cb_2a00f0_l5);
    rilgar_part2_register();  // (the classes' draw callbacks)
}

}  // namespace rac1
