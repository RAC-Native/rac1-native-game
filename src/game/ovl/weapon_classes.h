// The shared (Veldin-address) moby classes and pieces the weapons, gadgets and the Vendor spawn or use, which no level ports
// on its own: class 1143 (the Vendor Orb), 479 (the Drone Device's drones, drone.cpp), 270 (the Morph-o-Ray's chicken) with
// 428 (its debris, morph_chicken.cpp), 804 (on the Morph-o-Ray's hand moby, morph.cpp) and 166, the Morph-o-Ray's HUD element
// (morph_hud.cpp) and particle types 5's and 0x37's updates. Every level's own overlay maps its copy to these
// (weapon_classes_register()).
#ifndef RAC1_GAME_OVL_WEAPON_CLASSES_H
#define RAC1_GAME_OVL_WEAPON_CLASSES_H

#include <cstdint>

namespace rac1 {

// --- weapon_classes.cpp ---
void vendor_orb_update(uint32_t m);    // orig: 0x002df730 (class 1143, the Vendor Orb)
void class166_update(uint32_t m);      // orig: 0x002b3458 (class 166)
void particle_05_update(uint32_t p);   // orig: 0x00269c70 (particle type 5, the table 0x1b1e80)
// Registers the updates and callbacks at Veldin's addresses (ovl_register_moby_fn; every level's own overlay maps its copy to them).
void weapon_classes_register();        // orig: (none)

// --- drone.cpp: the Drone Device's drones ---
void drone_update(uint32_t m);                       // orig: 0x002ce358 (class 479)
uint32_t drone_target_find(uint32_t from, uint32_t taken);  // orig: 0x002cdf28
void drone_trails_attach(uint32_t m);                // orig: 0x002cde78
uint32_t particle_37_spawn(uint32_t m, uint32_t id, uint32_t color, uint32_t color2, float size, float shrink);  // orig: 0x002725d8
void particle_37_update(uint32_t p);                 // orig: 0x002726d8 (particle type 0x37: the drones' trails)

// --- morph_chicken.cpp: the Morph-o-Ray's chicken ---
void chicken_update(uint32_t m);       // orig: 0x002c97e0 (class 270)
void chicken_hook_c(uint32_t m);       // orig: 0x002caf10 (class 270's hook +0xc, its class table 0x1ea7d4)
uint32_t chicken_debris_spawn(uint32_t chicken, uint32_t pos, uint32_t vel);  // orig: 0x002cbf68 (EE addresses)
void class428_update(uint32_t m);      // orig: 0x002cc210 (class 428, the chicken's debris)

// --- morph.cpp ---
void class804_update(uint32_t m);      // orig: 0x002d7f88 (class 804, on the Morph-o-Ray's hand moby)

// --- morph_hud.cpp: the Morph-o-Ray's HUD element (slot 4, key 0x7533) ---
void morph_hud_start(uint32_t e);      // orig: 0x00236b10 (a0 = the slot 0x17dc50 + 4 * 0x90)
void morph_hud_update(uint32_t e);     // orig: 0x00236d00 (its update, slot +0x14)
void morph_hud_draw(uint32_t e);       // orig: 0x00236e50 (its draw, slot +0x18)

}  // namespace rac1

#endif
