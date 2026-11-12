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

// The board (Kalebo III's and Rilgar's builds: the generator's hooks, tools/codegen/hooks.glue) outside a race, on a level mod's
// level (hero_modes.h's rules; the original's levels: as transliterated, every hook a test that fails):
// orig: (none) — enhancement: the level number the board's race code compares (its fall test, its race's start line): the
// planet whose race it rides (`world rilgar` on Kalebo III's base: 5), else `level` as read
uint32_t level_mod_board_planet(uint32_t level);
// orig: (none) — enhancement: true when the board rides free (no race: its fall test level_mod_board_fell's, no race HUD)
bool level_mod_board_free();
// orig: (none) — enhancement: a free board's fall test (the race's z below 76 on Kalebo III, 61.4 on Rilgar): true when he is
// below the fall height (its +0x8a0 set), the race's fall state 0x6e then entered
bool level_mod_board_fell(float z);
// orig: (none) — enhancement: the race's "back on the track" place (L16:0x225ac0, L5:0x254358: the nearest point of the board's
// race path) for a free board: a fall's place back (the plugin's, else where he got on), a wipe-out's where he is, into
// `pos` / `rot` (the game's vectors); true when it did
bool level_mod_board_back(uint32_t pos, uint32_t rot);
// orig: (none) — enhancement: true when the board's race HUD (lap, time, place: its requests 5 and 7) is not asked for
bool level_mod_board_hud_off();
// orig: (none) — enhancement: the level number the board's race camera compares (camera type 8's floor under its target: Rilgar's
// 62.4 / 67.2, Kalebo III's 77.5): on a level mod's level the current section's world (`world rilgar`: 5), none (-1: no floor) in
// main and the mod's own sections; `level` as read on the original's levels
uint32_t level_mod_camera_planet(uint32_t level);

}  // namespace rac1

#endif
