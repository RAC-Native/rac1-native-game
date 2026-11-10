// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/mod_code.cpp.)
// The mods' code (docs/custom-levels.md, "Code"): the native plugins of the mods on whose code is loaded
// (runtime/kernel/mods.h, ModInfo::code_on), loaded at the first level's load, called through the C interface of
// sdk/rac1_mod.h. Every hook here does nothing when no plugin is loaded (no mod with code on: the original, byte for byte).
#ifndef RAC1_HOST_MOD_CODE_H
#define RAC1_HOST_MOD_CODE_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rac1 {

// (true while a plugin is loaded: the hooks' gate, inline)
extern bool g_mod_code_any;
// (true while the current level has placed objects a plugin updates: moby_update_visible's gate)
extern bool g_mod_code_objects;
// (true while a level mod's level has level.txt's update_all line: moby_visible_queue updates every moby, wherever the camera
// is, as a moby whose update distance +0x30 is 0xff)
extern bool g_mod_update_all;
// (true while a session with sections is current: src/game/enhance/sections.h; the HUD's draw then runs mod_code_hud for a
// switch's fade)
extern bool g_sections_on;

// (ABI 5) weapon_allow: true while a plugin limits the weapons that can be chosen (the quick select's ring, the PC's next /
// previous weapon); mod_code_weapon_allowed(item): false for a weapon not in its list (true for every other item, and always
// when no list is set: the original's)
extern bool g_mod_weapons_limited;
bool mod_code_weapon_allowed_slow(uint32_t item);
inline bool mod_code_weapon_allowed(uint32_t item) { return !g_mod_weapons_limited || mod_code_weapon_allowed_slow(item); }

// (ABI 4) the plugins' lights of a frame (light_add) in EE memory: the count at MOD_LIGHTS, then MOD_LIGHT_MAX entries of 0x20
// bytes from MOD_LIGHTS + 0x10 (x y z radius, r g b already times the intensity, 0). The modern lighting reads them from the
// frame's copy (src/game/render/mrp_lighting.cpp); draw_memory_take copies the block while a plugin is loaded
constexpr uint32_t MOD_LIGHTS = 0x000ef400u, MOD_LIGHTS_END = 0x000ef800u, MOD_LIGHT_MAX = 31;

// orig: (none) — enhancement: the game's start, its EE memory made: the plugins loaded (their failures noted on their mods,
// runtime/kernel/mods.h's mods_code_failed: Options > Mods shows them)
void mod_code_start();

// orig: (none) — enhancement: a level's gameplay load starts (gameplay_load): the plugins loaded (the first time), the level
// before ended, the next one's start at its first frame
void mod_code_level_load();

// orig: (none) — enhancement: a level's gameplay load has ended (its mobys spawned, their pvars in place): a level mod's spawn
// pools taken out of play (base_spawn's)
void mod_code_level_loaded();

// orig: (none) — enhancement: a frame of play (gameplay_frame, past its menus): the level's start (its first frame), the
// plugins' frame and test callbacks
void mod_code_frame();

// orig: (none) — enhancement: pad_read_packet (the game's pad read, every game mode): before it, the real buttons put back
// where input_hold masked them (the edges come from the pad); after it, the frame's real buttons kept for pad_held /
// pad_pressed and, in play while a plugin holds the input (input_hold), the game's fields masked (Start and Select left)
void mod_code_pad_before(uint32_t pad);
void mod_code_pad_after(uint32_t pad);

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

// orig: (none) — enhancement: level.txt's path `name` of the current level mod (its level.sdk): its points, x y z each; false none
bool mod_code_path_points(const std::string& name, std::vector<float>& xyz);
// orig: (none) — enhancement: a session's soft reset done (session_runtime.h): the plugins' respawn callback (ABI 9), Ratchet
// back at the checkpoint `checkpoint` (pos, yaw), the session's `count`th respawn
void mod_code_respawn(uint32_t count, const char* checkpoint, const float pos[3], float yaw);
// orig: (none) — enhancement: a session's section switch (src/game/enhance/sections.h): the plugins' section_leave (`from`) and
// section_enter (`to`, at its entry `entry`, "" none) callbacks (ABI 10)
void mod_code_section_switch(const char* from, const char* to, const char* entry);
// orig: (none) — enhancement: a section's boss (src/game/enhance/sections.h): the plugins' boss callback (ABI 13), `event`
// RAC1_BOSS_BEATEN (1) or RAC1_BOSS_AGAIN (2)
void mod_code_boss(const char* section, int32_t cls, uint32_t obj, int32_t event);
// orig: (none) — enhancement: a save made in a session: each plugin's session bytes (session_save, ABI 9) by its mod's folder
void mod_code_session_save(std::vector<std::pair<std::string, std::vector<uint8_t>>>& out);
// orig: (none) — enhancement: a loaded save's session bytes of the plugin of `folder` (session_load, ABI 9): given after the
// level's level_start
void mod_code_session_load(const std::string& folder, const uint8_t* data, uint32_t size);

}  // namespace rac1

#endif
