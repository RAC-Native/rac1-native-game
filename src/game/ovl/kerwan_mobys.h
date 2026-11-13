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
void empty_update_l3(uint32_t m);            // L3:0x002e2a40  class 1828
void zone_marker_update_l3(uint32_t m);       // (novalis_props.cpp)  L3:0x002cbc38  class 605

}  // namespace rac1

#endif
