// Rilgar's (level 5) moby classes and camera types: the declarations of their hand-written ports (the unit
// rilgar_classes_tr_N.cpp is retired since H3).
#ifndef RAC1_GAME_OVL_RILGAR_MOBYS_H
#define RAC1_GAME_OVL_RILGAR_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

// orig: (none) -- Rilgar's registrations (rilgar_mobys.cpp)
void rilgar_moby_fns_register();

// ---- the classes ported by hand (their helpers: rilgar_own.h)
void ril_class35_update_l5(uint32_t m);   // L5:0x002d1688 the water monster (rilgar_water_monster.cpp)
void ril_class79_update_l5(uint32_t m);   // L5:0x002d7140 the air traffic (rilgar_air_traffic.cpp)
void ril_class133_update_l5(uint32_t m);  // L5:0x002dac80 the race bots (rilgar_race_bots.cpp)
void ril_class623_update_l5(uint32_t m);  // L5:0x00301f48 the spitter (rilgar_spitter.cpp)
void ril_class625_update_l5(uint32_t m);  // L5:0x00304320 the flame tank (rilgar_flame_tank.cpp)
void ril_class717_update_l5(uint32_t m);  // L5:0x00307910 the race rivals (rilgar_race_rivals.cpp)
void ril_class810_update_l5(uint32_t m);  // L5:0x0030bdd8 the rocking floats (rilgar_props.cpp)
void ril_class812_update_l5(uint32_t m);  // L5:0x0030bf98 the props riding another moby (rilgar_props.cpp)
void ril_class831_update_l5(uint32_t m);  // L5:0x0030ca80 the water (rilgar_water.cpp)
void ril_class838_update_l5(uint32_t m);  // L5:0x0030dc68 the shared hum (rilgar_props.cpp)
void ril_class841_update_l5(uint32_t m);  // L5:0x0030e1f8 the fog and light zones (rilgar_props.cpp)
void ril_class843_update_l5(uint32_t m);  // L5:0x0030e508 the sliding, sinking gates (rilgar_props.cpp)
void ril_class879_update_l5(uint32_t m);   // L5:0x00316110 the open sea (rilgar_ocean.cpp)
void ril_class895_update_l5(uint32_t m);   // L5:0x003166a0 the turning gates (rilgar_props.cpp)
void ril_class982_update_l5(uint32_t m);   // L5:0x00318a68 the bay's water disc (rilgar_ocean.cpp)
void ril_class984_update_l5(uint32_t m);   // L5:0x00318b98 the bobbing floats (rilgar_props.cpp)
void ril_class1099_update_l5(uint32_t m);  // L5:0x00319c28 the props hidden in scenes (rilgar_props.cpp)
void ril_class1550_update_l5(uint32_t m);  // L5:0x0031d5c8 the scenes' smoke (rilgar_props.cpp)
void ril_class846_update_l5(uint32_t m);   // L5:0x0030f5c8 the pods that burst (rilgar_props.cpp)
void ril_class852_update_l5(uint32_t m);   // L5:0x00314eb0 the gates let down into their slots (rilgar_props.cpp)
void ril_class855_update_l5(uint32_t m);   // L5:0x003150f0 the waterfalls (rilgar_ocean.cpp)
void ril_class877_update_l5(uint32_t m);   // L5:0x003156d0 the lifts (rilgar_props.cpp)
void ril_class893_update_l5(uint32_t m);   // L5:0x00316258 the hatches over the lifts (rilgar_props.cpp)
void ril_class919_update_l5(uint32_t m);   // L5:0x00317470 the pilot (rilgar_people.cpp)
void ril_class925_update_l5(uint32_t m);   // L5:0x003180a0 the man with the item 0x17 (rilgar_people.cpp)
void ril_class998_update_l5(uint32_t m);   // L5:0x00318c78 the raft (rilgar_ocean.cpp)
void ril_class844_update_l5(uint32_t m);   // L5:0x0030e7f8 the float that leaps (rilgar_class844.cpp)
void ril_class918_update_l5(uint32_t m);   // L5:0x00316ab8 the race organiser (rilgar_race_organiser.cpp)
void ril_class1347_update_l5(uint32_t m);  // L5:0x0031bf40 the hint giver (rilgar_help.cpp)

}  // namespace rac1

#endif
