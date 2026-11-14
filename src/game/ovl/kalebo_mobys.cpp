// Kalebo III (level 16) own moby classes: the registration table. The ports are the transliterations in
// kalebo_classes_tr_N.cpp (declarations: kalebo_mobys.h). Keyed by level 16's own address and by level, so the indirect call (`jalr`) through
// moby +0x74 (or a callback installed by a class) reaches them only while the played level's overlay is level 16's.
#include <game/ovl/kalebo_hooks.h>
#include <game/ovl/kalebo_mobys.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L16:0x002e65d0 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void kal_hook_2e65d0_m(uint32_t m) { kal_hook_2e65d0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L16:0x002e6670 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void kal_hook_2e6670_m(uint32_t m) { kal_hook_2e6670_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L16:0x002e66c0 (a class hook +0 / +8 / +0xc: the indirect-call form, the moby only)
void kal_hook_2e66c0_m(uint32_t m) { kal_hook_2e66c0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

}  // namespace

// orig: (none) -- level 16's registrations
void kalebo_moby_fns_register() {
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a05e8), kal_class49_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a0a98), kal_class50_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a1088), kal_class52_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002a3f38), kal_class1475_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c3be8), kal_class235_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c3d38), kal_class257_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c5eb0), kal_class281_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c7788), kal_class439_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c9480), kal_class470_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c9878), kal_class471_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cb600), kal_class482_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cddb8), kal_class541_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cf198), kal_class546_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cf4a8), kal_class552_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d04a0), kal_class556_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d2a80), kal_class605_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d2cf0), kal_class638_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d59e0), kal_class647_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d5cb8), kal_class650_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d5ef8), kal_class654_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d6378), kal_class758_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d6740), kal_class800_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002d69e0), kal_class805_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002dddd8), empty_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002ddde0), kal_class933_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002dee10), kal_class1111_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002df1e8), kal_class1134_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e0120), kal_class1135_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e0f78), kal_class1139_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e1080), empty_update);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e22d8), kal_class1356_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e3190), kal_class1377_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e36e8), kal_class1387_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e37a0), kal_class1401_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e43e0), kal_class1410_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e5010), kal_class1439_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e5258), kal_class1441_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e54a0), kal_class1442_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e5708), kal_class1443_update_l16);
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
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e98e0), kal_class1948_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9df0), kal_class1952_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002ea128), kal_class1953_update_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002351f8), kal_cb_2351f8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00235268), kal_cb_235268_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00235660), kal_cb_235660_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002388b0), kal_cb_2388b0_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002388f8), kal_cb_2388f8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00238a30), kal_cb_238a30_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x00238e20), kal_cb_238e20_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002c9cd0), kal_cb_2c9cd0_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cef60), kal_cb_2cef60_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002cf7a8), kal_cb_2cf7a8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002de3c0), kal_cb_2de3c0_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e0a38), kal_cb_2e0a38_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e58d8), kal_cb_2e58d8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e84e8), kal_cb_2e84e8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e8d40), kal_cb_2e8d40_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9270), kal_cb_2e9270_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e97a0), kal_cb_2e97a0_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9ce0), kal_cb_2e9ce0_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e9fe8), kal_cb_2e9fe8_l16);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e65d0), kal_hook_2e65d0_m);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e6670), kal_hook_2e6670_m);
    ovl_register_level_moby_fn(KALEBO, L16_ADDR(0x002e66c0), kal_hook_2e66c0_m);
}

}  // namespace rac1
