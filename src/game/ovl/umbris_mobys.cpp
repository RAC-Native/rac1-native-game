// Umbris (level 7) own moby classes: the registration table. The ports are the level's own, written by hand (declarations:
// umbris_mobys.h) and, for the classes several levels carry a copy of, the shared hand-written ports (shared_*.cpp, novalis_*.cpp).
// Keyed by level 7's own address and by level, so the indirect call (`jalr` through moby +0x74) reaches them only while the played
// level's overlay is level 7's.
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/umbris_hooks.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/umbris_part2.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_breakables.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class631.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_class886.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_movers.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_swingshot_target.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L7:0x002f44b0 (a class hook +0xc: the indirect-call form, the moby only)
void umb_hook_2f44b0_m(uint32_t m) { umb_carry_hookc_l7(m, 0, 0, 0.0f); }
}  // namespace

// orig: (none) -- level 7's registrations
void umbris_moby_fns_register() {
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002cd2b0), umb_class38_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002cdb28), umb_class1474_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002f2688), class258_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002f5508), class341_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002f5ba0), umb_class436_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002f61c0), class447_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002f8058), sea_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002fbdb8), umb_class529_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002fbdf0), empty_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002fd698), zone_marker_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002fd908), class631_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00300338), rock_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00300970), scripted_camera_trigger_update_l1);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00302238), swingshot_target_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x003027a0), spawn_point_update_l1);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00304738), class823_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00304e50), class832_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00304ff0), class848_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030b000), umb_class871_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030b6a8), umb_class880_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030bbc8), umb_class882_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030bf90), class886_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030c508), umb_class902_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030cf90), umb_class1013_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030d440), umb_class1018_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030d4d8), umb_class1041_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030e1f8), umb_class1046_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030e458), umb_class1049_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030f5f0), umb_class1059_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00310df0), mover_door_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x003112c8), umb_class1069_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00311bc8), umb_class1080_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00311eb0), empty_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00314150), umb_class1106_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00319040), umb_class1110_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x003196e0), umb_class1112_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00319f48), umb_class1113_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031a250), umb_class1126_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031a9e8), umb_class1128_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031aee0), mover_platform_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031b3f0), umb_class1133_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031b5f0), gold_bolt_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031c528), teleporter_update_l1);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031d2e0), umb_class1142_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031d9f8), class1221_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031dae0), class1221_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031eba8), umb_class1552_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031f3d8), class1650_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031f5a8), empty_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031f5b0), class1663_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031f780), umb_class1686_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031f900), umb_class1789_update_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002f44b0), umb_hook_2f44b0_m);
    umbris_part2_register();  // (the classes' draw callbacks, particle type 0x50)
}

}  // namespace rac1
