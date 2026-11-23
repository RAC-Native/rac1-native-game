// Orxon (level 10) own moby classes: the registration table. The ports are the hand-written classes of orxon_classes.h,
// the level's hand-written ports (declarations: orxon_mobys.h) and, for the classes other levels share,
// the shared hand-written
// ports (shared_*.cpp, novalis_*.cpp, kerwan_props.cpp, aridia_ocean.cpp: each proved against level 10's own copy by its
// family batch). Keyed by level 10's own address and by level, so the indirect call (`jalr`) through
// moby +0x74) reaches them only while the played level's overlay is level 10's.
#include <game/ovl/orxon_hooks.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/aridia_ocean.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_class1038.h>
#include <game/ovl/shared_class1068.h>
#include <game/ovl/shared_class1302.h>
#include <game/ovl/shared_class1378.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class827.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class857.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_puffer.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_swingshot_target.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L10:0x002ce618 (class 827's hook +0, the shared port: the indirect-call form, the moby only)
void orx_hook_2ce618_m(uint32_t m) { class827_hook_carry0(m, 0, 0, 0.0f); }

// orig: (none) -- adapter of L10:0x002ce770 (class 827's hook +8, the shared port: the indirect-call form, the moby only)
void orx_hook_2ce770_m(uint32_t m) { class827_hook_carry(m, 0, 0, 0.0f, 8); }

// orig: (none) -- adapter of L10:0x002ce800 (class 827's hook +0xc, the shared port: the indirect-call form, the moby only)
void orx_hook_2ce800_m(uint32_t m) { class827_hook_carryc(m, 0, 0, 0.0f); }

}  // namespace

// orig: (none) -- level 10's registrations
void orxon_moby_fns_register() {
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00295a38), orx_class1033_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00295c20), orx_class1117_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00298668), orx_class18_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x00298b68), orx_class22_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x0029e1c8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bae38), class258_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bd0e8), class302_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bd768), class303_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002be858), orx_class351_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002befe8), orx_class353_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c4bc8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c6470), zone_marker_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c74b8), puffer_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c7a20), orx_class702_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c7cf8), scripted_camera_trigger_update_l1);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c9590), swingshot_target_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002c9c70), orx_class794_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ca220), spawn_point_update_l1);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002cc2e8), class819_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002cc8b8), class827_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002cea78), class832_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d4078), class857_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d7b80), orx_class859_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d7fe8), fred_update_l3);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d85c8), orx_class938_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d8c00), orx_class939_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d8d30), orx_class947_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d90a8), orx_class1015_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d92b8), orx_class1031_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d9550), class1038_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d9bf8), class1040_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d9eb8), orx_class1047_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002da2c8), orx_class1067_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dabe8), class1068_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dcc58), orx_class1073_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd010), beam_shard_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd3d8), orx_class1090_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd650), orx_class1100_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd8d8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dd8e0), gold_bolt_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002de860), lift_update_l1);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002df270), orx_class1196_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e01a8), orx_class1199_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e1d38), orx_class1202_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e4a88), orx_class1229_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e5be0), orx_class1240_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e6a78), shell_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e7cd8), class1302_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e8358), orx_class1326_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e85b8), orx_class1344_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e8ab0), orx_class1346_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e9028), class1378_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e9648), orx_class1421_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e9990), orx_class1424_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ea1f0), orx_class1544_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eacd8), orx_class1555_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb1b0), rock_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb6f0), orx_class1855_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb7e0), orx_class1856_update_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb8b8), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb950), empty_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ebb08), bg_ship2_update);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce618), orx_hook_2ce618_m);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce770), orx_hook_2ce770_m);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce800), orx_hook_2ce800_m);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002bd280), class302_draw);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002ce868), class832_draw);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002d7808), class857_draw);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002da690), orx_cb_2da690_l10);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002dc698), beam_draw);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e7520), class1302_draw);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002e91b8), class1378_draw);
    ovl_register_level_moby_fn(ORXON, L10_ADDR(0x002eb958), ari_bg_ship2_glow_draw_l2);
}

}  // namespace rac1
