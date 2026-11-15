// The open engine's stage 7 (docs/open-engine.md, "Rails and set pieces (stage 7)"; DESIGN stage 7, STUDY 5.1 to 5.5): a level
// mod's grind rails (level.txt's `rail` / `rails` lines: the composer writes them into the gameplay file's rail block, main's
// and each section's; the game's own rail code rides them), the landing gate of Kalebo III's, Oltanis's and Blarg's hero code
// skipped on a level mod with rails of its own, the items given for the visit (level.txt's `items`, the SDK's item_visit: never
// in a save), and the set-piece kit's services: a screen fade, a camera of the plugin's (fixed, looking at, along a path,
// following), hazards (volumes that hurt on a timer). The rails' services and callbacks (rail_find, hero_rail, rail_point,
// rail_enable, hero_rail_attach, rail_speed; the rail callback's enter / end / leave) are the SDK's ABI 7 room (game ABI 11).
// Nothing here runs on the game's own levels: everything starts from a level mod's level.sdk or a plugin's call.
#ifndef RAC1_GAME_ENHANCE_SETPIECES_H
#define RAC1_GAME_ENHANCE_SETPIECES_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "../../../sdk/rac1_mod.h"

namespace rac1 {

// What the mods' code gives this module (mod_code.cpp): the game's hurt, the paths (path_find / path_create handles), an
// object's moby, the plugins' rail callback
struct SetpiecesHost {
    int (*hero_damage)(int32_t amount, uint32_t from) = nullptr;
    bool (*path)(int32_t handle, std::vector<std::array<float, 3>>& out) = nullptr;
    uint32_t (*object_moby)(uint32_t obj) = nullptr;
    void (*rail_event)(int32_t rail, const char* name, int32_t event) = nullptr;
    void (*hero_place)(const float pos[3], float yaw) = nullptr;
};
// orig: (host) the host's services (once, at the plugins' load)
void setpieces_host(const SetpiecesHost& h);

// orig: (host) hero_rail_land_check's level gate (Kalebo III's, Oltanis's, Blarg's rail spots) skipped: a level mod's level with
// rails of its own (level.txt's) is the current one
bool setpieces_rails_anywhere();

// orig: (host) a level loads (mod_code_level_load): the items given for the last one's visit taken back, the camera, fade and
// hazards dropped; level `slot`'s level.sdk (`sdk`, "" none) read for its rails and items
void setpieces_level_load(int slot, const std::string& sdk);
// orig: (host) the level's first frame (mod_code's level start): its items given, its rails' states applied
void setpieces_level_start();
// orig: (host) a frame of play, after the hero's and the objects' updates, before the plugins' frame callbacks: the rails'
// events, a rail's speed, the hazards, the fade's time
void setpieces_frame();
// orig: (host) after the game's camera update (mod_code_camera): the plugin's camera
void setpieces_camera();
// orig: (host) the HUD's draw: the fade
void setpieces_hud();
// orig: (host) the card's save image written (save_blocks_write): the items of the visit out of it, then back
void setpieces_save_begin();
void setpieces_save_end();
// orig: (host) sections.cpp: section `index`'s rail table is the game's now (a switch, the arrival): the rails' states applied
void setpieces_section(int index);

// The SDK's services (sdk/rac1_mod.h, ABI 7's room)
int32_t sp_rail_find(const char* name);
int sp_hero_rail(rac1_rail_info* out);
int sp_rail_point(int32_t rail, float distance, float out[3], float dir[3]);
int sp_rail_enable(int32_t rail, int on);
int sp_hero_rail_attach(int32_t rail, float distance, int32_t direction);
int sp_rail_speed(int32_t rail, float scale);
int sp_item_visit(int32_t item, int on);
int sp_fade(int32_t out_frames, int32_t hold_frames, int32_t in_frames, uint32_t rgb);
int sp_camera_set(const rac1_camera* cam);
int sp_camera_release(int32_t blend);
int32_t sp_hazard_add(const rac1_hazard* h);
int sp_hazard_set(int32_t hazard, int on);
int sp_rail_info(int32_t rail, char* name, int32_t cap, float* length, int32_t* section);
int sp_fade_active();

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_SETPIECES_H
