// Kerwan's own moby classes: the registration table. The ports are in kerwan_*.cpp.
#include <game/enhance/cut_content.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/kerwan_part2_tr.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class235.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class631.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_class994.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_spawner.h>
#include <game/ovl/shared_swingshot_target.h>

#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: (none) -- Kerwan's registrations
void kerwan_moby_fns_register() {
    // the same code as Novalis's (class 805 spawn point: OV globals only; 726 the lift; 737 the scripted camera trigger,
    // 845 the aircraft base, below with the shared ports)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d1140), spawn_point_update_l1);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x00291dd0), lift_update_l1);
    // class 605 the zone marker: the same code, its counter at gp - 0x5270
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002cbc38), zone_marker_update_l3);
    // the props, citizens and mechanisms (kerwan_props.cpp)
    // (Fred: through the Cut Content option's choice, cut_content.h; with it off the faithful fred_update_l3 runs)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dc310), cut_content_fred_dispatch_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002e2948), lightpost_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d3cd8), statue_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d3e58), robo_shack_sign_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dca30), garage_door_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002953f8), train_carriage_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002e2a40), empty_update);  // (class 1828: a bare `jr ra`, static scenery)
    // the classes several levels share, ported once (shared_*.cpp, novalis_*.cpp; each proved against Kerwan's own copy)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002bae48), class235_update);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002bef68), class455_update);  // (the Spawner)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002c4718), class545_update);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002cb8e8), class604_update);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002cca00), class631_update);  // (the Blarg helicopter)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002cf340), scripted_camera_trigger_update_l1);  // (class 737)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d0bd8), swingshot_target_update);  // (class 758)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d40e8), class832_update);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d3ed8), class832_draw);  // (its draw-list callback)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x00293c78), jet_update_base_l1);  // (class 845, the aircraft base)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d47c0), class848_update);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dc478), class447_update);  // (class 920)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dc9b0), class994_update);  // (the sea's controller)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dd188), gold_bolt_update);  // (class 1134)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002de370), class1221_update);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002de458), class1221_update);  // (class 1222: the same code)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002e08c8), class1633_update);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002e1970), sandmouse_update);  // (class 1818)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002e1898), sandmouse_glow);  // (the Sandmouse's draw-list callback)
    // the train (kerwan_train.cpp), the shuttles 868 / 905 / 928 (kerwan_shuttles.cpp), the air traffic 75, 115 .. 120, 132
    // (kerwan_traffic.cpp), class 573 (kerwan_walkers.cpp)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x00292e98), kerwan_class822_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x00294c08), kerwan_class868_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x0029dba8), kerwan_class75_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002c5bb8), kerwan_class573_update_l3);
    // the Blarg trooper 574 (kerwan_troopers.cpp), the mine layer 578 (kerwan_mine_layer.cpp)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002c6fd0), kerwan_class574_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002c9eb8), kerwan_class578_update_l3);
    // the transliterated classes (kerwan_classes_tr.cpp, the translator)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002cbea8), kerwan_class627_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d3198), kerwan_class816_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002d43c8), kerwan_class833_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002da870), kerwan_class890_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002db020), kerwan_class898_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002db280), kerwan_class899_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002db558), kerwan_class909_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dbd90), kerwan_class914_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dccc0), kerwan_class1012_update_l3);
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002df520), kerwan_class1342_update_l3);
    // part 2 (kerwan_part2_tr.cpp): class 994's draw callback (queued by its update through 0x1f93e0). It stays transliterated
    // with the ocean's set-up and texture upload it reaches: the texture upload carries the palette-blend hook
    // (tools/codegen/hooks.glue, RAC1_PERF_PALETTE_BLEND) that the shared port (sea_draw, ari_ocean_tex_upload_l2) lacks.
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002dc990), kerwan_class994_draw_l3);
    // class 1548: a lightning effect of the scenes (game mode 2; 0335's hand port took 0x15f5c4 for the level number)
    ovl_register_level_moby_fn(KERWAN, L3_ADDR(0x002e01d0), kerwan_class1548_update_l3);
}

}  // namespace rac1
