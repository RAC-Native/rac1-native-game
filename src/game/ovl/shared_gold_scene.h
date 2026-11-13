// The gold weapons' scene start of Novalis's and Gemlik Base's cases (shared_gold_scene.cpp: one port for both levels'
// copies). Phase 1 of stage 4: the levels' generated copies still run.
#ifndef RAC1_GAME_OVL_SHARED_GOLD_SCENE_H
#define RAC1_GAME_OVL_SHARED_GOLD_SCENE_H

#include <cstdint>

namespace rac1 {

// the scene of the gold weapon `weapon` bought at the case `m` (L1:0x002aea70, L13:0x002a5390)
void gold_weapon_scene(uint32_t m, uint32_t weapon);

}  // namespace rac1

#endif
