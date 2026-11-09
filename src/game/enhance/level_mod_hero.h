// The hero on a level mod's level (docs/mods.md, "Base levels"): what a base level's own hero code lacks there.
#ifndef RAC1_GAME_ENHANCE_LEVEL_MOD_HERO_H
#define RAC1_GAME_ENHANCE_LEVEL_MOD_HERO_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: Kalebo III's own hero_surface_update (L16:0x002097a0, kalebo_hero_tr_1.cpp: the generator's hook,
// tools/codegen/hooks.glue) after its surface flags, `kind` the surface under the hero (its +0x12e0 as the frame began). Kalebo's
// build has no lava (kind 3) and no deadly liquid (kind 1): on a level mod's level (a host slot; the original's Kalebo III
// never) they are flagged as the generic build flags them (+0x12e6, +0x12e5) and act with the states Kalebo's hero has (it has
// none of the generic 0x68 / 0x7b / 0x7c / 0x7d; its 0x69 / 0x3c hold him in place when entered without them): coming down into
// either, a hop out (the standing jump, 7) while he has health and has hopped fewer than three times within 3 seconds, else lost
// (hero_lost: the fade and the respawn, as a fall). True when it changed the hero's state (the function returns).
bool level_mod_kalebo_surface(int32_t kind);

// orig: (none) — enhancement: Kalebo III's own hero_ground_update (L16:0x0020ffb0: the generator's hook) after its surface probe:
// true on a level mod's level when the surface under the hero is lava (kind 3), for the generic build's probe through it
// (0x002127b8: the lava's top +0x2f4, the ground under it) that Kalebo's build lacks
bool level_mod_kalebo_lava_ground();

}  // namespace rac1

#endif
