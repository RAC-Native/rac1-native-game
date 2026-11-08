// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/mod_code.cpp.)
// The mods' code (docs/custom-levels.md, "Code"): the native plugins of the mods on whose code is loaded
// (runtime/kernel/mods.h, ModInfo::code_on), loaded at the first level's load, called through the C interface of
// sdk/rac1_mod.h. Every hook here does nothing when no plugin is loaded (no mod with code on: the original, byte for byte).
#ifndef RAC1_HOST_MOD_CODE_H
#define RAC1_HOST_MOD_CODE_H

#include <cstdint>
#include <vector>

namespace rac1 {

// (true while a plugin is loaded: the hooks' gate, inline)
extern bool g_mod_code_any;
// (true while the current level has placed objects a plugin updates: moby_update_visible's gate)
extern bool g_mod_code_objects;

// orig: (none) — enhancement: the game's start, its EE memory made: the plugins loaded (their failures noted on their mods,
// runtime/kernel/mods.h's mods_code_failed: Options > Mods shows them)
void mod_code_start();

// orig: (none) — enhancement: a level's gameplay load starts (gameplay_load): the plugins loaded (the first time), the level
// before ended, the next one's start at its first frame
void mod_code_level_load();

// orig: (none) — enhancement: a frame of play (gameplay_frame, past its menus): the level's start (its first frame), the
// plugins' frame and test callbacks
void mod_code_frame();

// orig: (none) — enhancement: after the game's camera update
void mod_code_camera();

// orig: (none) — enhancement: the HUD's draw (hud_draw)
void mod_code_hud();

// orig: (none) — enhancement: moby `m`'s update (moby_update_visible): true when a plugin did it instead of its class
bool mod_code_object_update(uint32_t m);

// orig: (none) — enhancement: a new game (new_game_state)
void mod_code_new_game();

// orig: (none) — enhancement: save<N>.pc (advanced.cpp): the plugins' items appended to `d` (their count returned), an item read
// (true: ours), before and after a save's items are read
uint32_t mod_code_pc_items(std::vector<uint8_t>& d);
bool mod_code_pc_item(uint32_t id, const uint8_t* p, uint32_t size);
void mod_code_pc_reset();
void mod_code_save_loaded();

}  // namespace rac1

#endif
