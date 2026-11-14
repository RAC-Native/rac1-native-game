// Kerwan's (level 3, Metropolis) own moby classes: the update functions of the classes only
// Kerwan registers, ported at Kerwan's own addresses
// (`// orig: L3:0x00xxxxxx`). They are registered per level (ovl_register_level_moby_fn): the indirect
// call of the original (`jalr` through moby +0x74) reaches them only while the played level's overlay is
// Kerwan's own (RAC1_OWN_OVERLAY=1 RAC1_LEVEL=3), keyed by Kerwan's address. Where Kerwan's build of a class
// is the same code as Novalis's (805 spawn point, 605 zone marker, 726 lift) the Novalis port is registered
// at Kerwan's address too (its level-own data address passed in).
#ifndef RAC1_GAME_OVL_KERWAN_MOBYS_H
#define RAC1_GAME_OVL_KERWAN_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/variants_l3.h>

namespace rac1 {

// orig: (none) -- Kerwan's registrations
void kerwan_moby_fns_register();

// ---- the props, citizens and mechanisms (kerwan_props.cpp)
void fred_update_l3(uint32_t m);              // L3:0x002dc310 L10:0x002d7fe8  classes 915, 916, 917 (cut content: retire at once)
void lightpost_update_l3(uint32_t m);         // L3:0x002e2948  class 1827
void statue_update_l3(uint32_t m);            // L3:0x002d3cd8  class 817
void robo_shack_sign_update_l3(uint32_t m);   // L3:0x002d3e58  class 825
void garage_door_update_l3(uint32_t m);       // L3:0x002dca30  class 997
void train_carriage_update_l3(uint32_t m);    // L3:0x002953f8  class 1210
void zone_marker_update_l3(uint32_t m);       // (novalis_props.cpp)  L3:0x002cbc38  class 605

// ---- the train of the ride, class 822 (kerwan_train.cpp)
void kerwan_class822_update_l3(uint32_t m);   // L3:0x00292e98  class 822
void train_place_l3(uint32_t m, uint32_t point, float heading);  // L3:0x00292578
void train_drive_l3(uint32_t m);              // L3:0x00292890
void train_hold_l3(uint32_t m);               // L3:0x00292d10
// ---- the shuttles, classes 868 / 905 / 928 (kerwan_shuttles.cpp)
void kerwan_class868_update_l3(uint32_t m);   // L3:0x00294c08  classes 868, 905, 928
// ---- the air traffic, classes 75, 115 .. 120, 132 (kerwan_traffic.cpp)
void kerwan_class75_update_l3(uint32_t m);    // L3:0x0029dba8  classes 75, 115 .. 120, 132
int32_t kerwan_level_sound_start_l3(int32_t idx, uint32_t flags, uint32_t moby);  // L3:0x0027a618
void traffic_engine_sound_l3(uint32_t m);     // L3:0x0029c9d0
uint32_t traffic_puff_spawn_l3(uint32_t m, uint32_t colour, uint32_t colour_end, int32_t life, float rise, float scale);  // L3:0x002bad40
void traffic_exhaust_l3(uint32_t m, uint32_t move, uint32_t off_screen);  // L3:0x0029caa8
void traffic_attitude_l3(uint32_t m);         // L3:0x0029cff0
void traffic_horn_l3(uint32_t m);             // L3:0x0029d1b8
void traffic_hit_l3(uint32_t m, uint32_t move, uint32_t off_screen);  // L3:0x0029d2e0
// ---- class 573, a walking robot that follows a class 574 (kerwan_walkers.cpp)
void kerwan_class573_update_l3(uint32_t m);   // L3:0x002c5bb8  class 573
uint32_t walker_leader_calls_l3(uint32_t l);  // L3:0x002c6ca0
uint32_t walker_group_busy_l3(uint32_t group);  // L3:0x002c6d98 (class 574's update, still generated, is glued to it)
void walker_alert_and_hits_l3(uint32_t m);    // L3:0x002c6a20
// ---- class 574, the Blarg trooper (kerwan_troopers.cpp)
void kerwan_class574_update_l3(uint32_t m);   // L3:0x002c6fd0  class 574
void trooper_floor_tilt_l3(uint32_t normal, uint32_t angles, float yaw);  // L3:0x0024fba8
uint32_t trooper_turn_to_l3(uint32_t m, uint32_t target, float heading);  // L3:0x002c6e18
void trooper_hits_l3(uint32_t m);             // L3:0x002c95a8
void trooper_pick_point_l3(uint32_t m);       // L3:0x002c9ca0
uint32_t trooper_bolt_spawn_l3(uint32_t dir, uint32_t at, uint32_t owner, int32_t life, float spin, float speed, float size);  // L3:0x002d4288
// ---- class 578, the mine layer (kerwan_mine_layer.cpp)
void kerwan_class578_update_l3(uint32_t m);   // L3:0x002c9eb8  class 578
void mine_layer_hits_l3(uint32_t m);          // L3:0x002ca808
uint32_t mine_layer_drop_l3(uint32_t layer, uint32_t at, uint32_t vel);  // L3:0x002cc888

}  // namespace rac1

#endif
