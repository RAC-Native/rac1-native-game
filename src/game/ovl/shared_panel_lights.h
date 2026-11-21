// A row of four pulsing lights before a panel (the Trespasser's on Aridia, Blarg's class 1016, Quartu's and Drek's fleet's
// class 1428; Orxon's class 18 and Quartu's class 1388 in a second build), shared: one port of the copies
// (shared_panel_lights.cpp).
#ifndef RAC1_GAME_OVL_SHARED_PANEL_LIGHTS_H
#define RAC1_GAME_OVL_SHARED_PANEL_LIGHTS_H

#include <cstdint>

namespace rac1 {

// Four pulsing lights (particle 0x3b) in a row from the moby away from the camera, their spins turning; their fields at
// the pvars' `base` (0 in the first build, 0x10 in the second).
void panel_lights(uint32_t m, uint32_t base);  // base 0: L2:0x002ea8b0 L6:0x002f4cd8 L15:0x002ebb50 L17:0x002f1568; base 0x10: L10:0x00298940 L15:0x002ea9b0

}  // namespace rac1

#endif
