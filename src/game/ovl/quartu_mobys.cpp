// Quartu (level 15) own moby classes: the registration table. The ports are the transliterations in
// quartu_classes_tr_N.cpp (declarations: quartu_mobys.h). Keyed by level 15's own address and by level, so the indirect call (`jalr`) through
// moby +0x74 (or a callback installed by a class) reaches them only while the played level's overlay is level 15's.
#include <game/ovl/quartu_hooks.h>
#include <game/ovl/quartu_mobys.h>

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
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002955c0), qua_class28_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002979d8), qua_class44_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029ac30), qua_class49_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029aff0), qua_class67_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029d6c0), qua_class256_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029ead0), qua_class1427_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029edb0), qua_class1523_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a2488), qua_class77_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a2bf0), qua_class78_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a36d0), qua_class92_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a3ba8), qua_class93_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a6e68), qua_class123_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a7880), qua_class148_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002aa280), qua_class154_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002aabb8), qua_class156_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc608), qua_class193_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bddb0), qua_class196_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002be298), qua_class200_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c2938), qua_class221_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c5630), qua_class233_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c6f08), qua_class250_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c73b8), qua_class258_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002c9240), empty_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ca240), qua_class325_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ca9f8), qua_class403_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cb4c8), qua_class408_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cbe58), empty_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cf3a8), qua_class491_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d3bd8), empty_update);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d4748), qua_class567_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d5728), qua_class605_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d6810), qua_class655_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d9100), qua_class1179_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d95e8), qua_class679_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002db100), qua_class758_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002db668), qua_class805_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002dd730), qua_class819_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ddd00), qua_class830_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002de290), qua_class832_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002de430), qua_class848_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e4380), qua_class934_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e46b0), qua_class937_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e4770), qua_class1134_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e5958), qua_class1209_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e5e70), qua_class1221_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e5f58), qua_class1222_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e6038), qua_class1224_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e73c0), qua_class1250_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e7f68), qua_class1257_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e8910), qua_class1331_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ea5e0), qua_class1371_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ea748), qua_class1388_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eabd8), qua_class1394_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb0a0), qua_class1408_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb458), qua_class1409_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb4c0), qua_class1419_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb928), qua_class1425_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ebd78), qua_class1428_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ec030), qua_class1430_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ec760), qua_class1446_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ed068), qua_class1451_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ed398), qua_class1469_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edb20), qua_class1560_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edf58), qua_class1565_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ee338), qua_class1567_update_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x00296710), qua_cb_296710_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x00297808), qua_cb_297808_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x0029aba8), qua_cb_29aba8_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a29b8), qua_cb_2a29b8_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a3138), qua_cb_2a3138_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002a7628), qua_cb_2a7628_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002ca810), qua_cb_2ca810_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002cbd88), qua_cb_2cbd88_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d77c0), qua_cb_2d77c0_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002d8710), qua_cb_2d8710_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002de080), qua_cb_2de080_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002e6a10), qua_cb_2e6a10_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eac90), qua_cb_2eac90_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb108), qua_cb_2eb108_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002eb6b8), qua_cb_2eb6b8_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edbe0), qua_cb_2edbe0_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002edfc0), qua_cb_2edfc0_l15);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc4b0), qua_hook_2bc4b0_m);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc550), qua_hook_2bc550_m);
    ovl_register_level_moby_fn(QUARTU, L15_ADDR(0x002bc5a0), qua_hook_2bc5a0_m);
}

}  // namespace rac1
