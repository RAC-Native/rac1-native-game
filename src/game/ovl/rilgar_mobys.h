// Rilgar's (level 5) transliterated moby classes and camera types: the declarations of rilgar_classes_tr_N.cpp
// (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper in the uniform ABI).
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

// ---- the entries (class updates, camera type functions) and the uniform-ABI entries
void ril_class844_update_l5(uint32_t a0);  // L5:0x0030e7f8
void ril_class918_update_l5(uint32_t a0);  // L5:0x00316ab8
void ril_class1347_update_l5(uint32_t a0);  // L5:0x0031bf40
void ril_cb_262f50_l5(uint32_t a0);  // L5:0x00262f50
void ril_cb_266320_l5(uint32_t a0);  // L5:0x00266320

// ---- the uniform-ABI entries and the level-only helpers
TrRet ril_29bbe0_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x0029bbe0
TrRet ril_30f218_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x0030f218
TrRet ril_30f408_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x0030f408
TrRet ril_3274b8_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x003274b8
TrRet ril_316810_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x00316810
TrRet ril_316990_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x00316990
TrRet ril_hook_300b48_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x00300b48
TrRet ril_hook_10_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x00300c18
TrRet ril_28d960_l5(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L5:0x0028d960  (hero_walk_to_l1's copy: kept until the PAL 2.00 walk-to fork is merged)

}  // namespace rac1

#endif
