// Aridia's own moby classes: the registration table. The ports are in aridia_*.cpp, and the classes several levels
// carry a copy of in shared_*.cpp (and novalis_mobys.cpp).
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/aridia_fx.h>
#include <game/ovl/aridia_hooks.h>
#include <game/ovl/aridia_hud.h>
#include <game/ovl/aridia_ocean.h>
#include <game/ovl/aridia_ship.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_class437.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gadget_pickups.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_swingshot_target.h>

#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: (none) -- Aridia's registrations
void aridia_moby_fns_register() {
    // the same code as Novalis's (class 805 spawn point: the checkpoint; OV globals only)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e28a0), spawn_point_update_l1);
    // class 605 the zone marker: the same code, its counter at gp-0x51a0
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d74e0), zone_marker_update_l2);
    // the props, platforms and mechanisms (aridia_props.cpp)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002cb978), trackdrive_left_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dd4d0), empty_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e2220), empty_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002f2058), empty_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002cbd98), empty_update_l2);  // (classes 433 / 618 / 1289, items 5 / 0x22 / 6)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002daf58), empty_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ee2a0), empty_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ddc00), platform_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002df8c0), platform_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dca10), bulldozer_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dd370), grab_crane_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002f1f60), white_flower_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ea198), ocean_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dffc8), trespasser_door1_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e0138), trespasser_door2_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002df948), rocket_door_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002df1a8), crane_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dc6b0), escalator_update_l2);
    // part 2 (hand-written: aridia_enemies.h)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d3e50), sand_shark_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d7748), constructorbot_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dcb38), nest_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ddc88), transport_pod_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002df3d8), rocket_launcher_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e1950), skid_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e0dc0), skid_agent_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e2228), liftcar_platform_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dbd38), elevator_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ee890), class1324_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ef020), class1479_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e0720), rocks_update_l2);
    // the Sand Shark class hooks (jalr through the class +0x2c table)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d61c0), shark_hook_moby_0);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d6210), shark_hook_moby_4);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d6290), shark_hook_moby_8);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d62e0), shark_hook_moby_c);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d6310), shark_hook_moby_10);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d6340), shark_hook_moby_14);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e0280), shark_egg_update_l2);          // class 755
    // the classes several levels carry a copy of: one hand-written port each (shared_*.cpp, novalis_mobys.cpp)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e0450), swingshot_target_update);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d8ad0), class615_update);            // the Trespasser's pad
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d93e8), class615_puzzle_draw);       // (the pad's draw-list callback)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ec4b8), bg_ship1_update);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ece18), bg_ship2_update);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ead38), gold_bolt_update);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002f0f88), sandmouse_update);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002f0eb0), sandmouse_glow);             // (the Sandmouse's draw-list callback)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002d7190), class604_update);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e4a48), class832_update);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002e4838), class832_draw);              // (its draw-list callback)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dfcd0), scripted_camera_trigger_update_l1);  // class 737
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ea210), gadget_pickup_update);       // class 1005
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002efee0), class1633_update);           // class 1633
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ccc08), class437_update);            // class 437
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dd6c0), rock_update);                // classes 696..698
    // Skid's HUD element (the HUD slot callbacks of his request, aridia_hud_asm.cpp)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x0023c990), ari_skid_hud_start_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x0023ccc8), ari_skid_hud_update_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x0023cda8), ari_skid_hud_draw_l2);
    // the ship's glows (Aridia's copies of Veldin's unported 0x28e670 / 0x28efc8, aridia_ship_asm.cpp)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x0028dde0), ari_ship_glow_draw_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x0028e738), ari_ship_engine_glow_draw_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x0028e9a8), ari_ship_takeoff_draw_l2);
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002dc2c8), ari_elevator_draw_l2);  // the Elevator's draw callback (aridia_fx_asm.cpp)
    ovl_register_level_moby_fn(ARIDIA, L2_ADDR(0x002ebcb8), ari_vendor_orb_update_l2);  // class 1143, the Vendor Orb (Veldin's 0x2df730 unported)
    aridia_ocean_fns_register();  // the ocean's and the glows' draws (aridia_ocean.cpp)
}

}  // namespace rac1
