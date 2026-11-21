// The water meshes that ripple from their centre (Quartu's two, Drek's fleet's two: two builds), drawn by their draw
// callbacks; shared within each level (shared_waves.cpp): each copy is the same code on its own mesh and words.
#ifndef RAC1_GAME_OVL_SHARED_WAVES_H
#define RAC1_GAME_OVL_SHARED_WAVES_H

#include <cstdint>

namespace rac1 {

// Quartu's ripple meshes (46 and 88 points).
void quartu_waves_draw_a(uint32_t m);  // L15:0x002edbe0
void quartu_waves_draw_b(uint32_t m);  // L15:0x002edfc0
// Drek's fleet's (the second build: the count its prim list's, the mesh not scaled).
void fleet_waves_draw_a(uint32_t m);  // L17:0x002f0948
void fleet_waves_draw_b(uint32_t m);  // L17:0x002f0d08

}  // namespace rac1

#endif
