// The open engine's session runtime (docs/open-engine.md, stage 5; DESIGN sections 2 and 8): a level mod of format 3 is a
// **session**. The game loads its level once, at the arrival, and never again until the player leaves (the ship, a save of
// another level loaded): a death does not run the game's restart (level_setup(0, 1): the gameplay file read again, every
// moby of the level rebuilt, the plugins' level started again) but a **soft reset**: Ratchet back at the session's current
// checkpoint with his health, the camera, the HUD and the music as at a start, the level's placed objects reset by the
// session's policy (default: those killed since the arrival come back as placed, unless the game's own records keep them
// killed for good), and nothing else touched: the weapons, the ammo, the bolts, the session's imports, the plugins' state and
// their spawned objects (the plugin's respawn callback decides about those). Checkpoints are level.txt's `checkpoint` lines,
// the plugin's (SDK ABI 9) and the game's own checkpoint saves; a save made in a session carries an item of the session in
// save<N>.pc (its level, the checkpoint, the placed objects gone, the plugins' session bytes): loading it brings the player
// back into the session at that checkpoint. Formats 1 and 2 are untouched: nothing here runs unless a format 3 level is
// current (g_session_rt, every hook's one test).
#ifndef RAC1_GAME_ENHANCE_SESSION_RUNTIME_H
#define RAC1_GAME_ENHANCE_SESSION_RUNTIME_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// (a format 3 level mod's session is current: the hooks' one test on every other level)
extern bool g_session_rt;

// The respawn policies of the level's placed objects (level.txt `respawn reset|keep`, the SDK's respawn_policy)
constexpr int32_t SESSION_RESPAWN_RESET = 0;  // those killed since the arrival come back as placed (not the game's killed for good)
constexpr int32_t SESSION_RESPAWN_KEEP = 1;   // they stay as they are

// orig: (host) a level's header read (level_slots.cpp): the session of the slot's build (its session.txt: a format 3 level
// mod) on, or off ("": a game level, or a level mod of format 1 / 2)
void session_rt_level(const std::string& build_dir, const std::string& level_id);
// orig: (host) the level's gameplay loaded (gameplay_load's end, after a level mod's spawn pools): in a session, the arrival:
// the hero's and the placed objects' records and pvars kept for the soft resets, the checkpoint the session's start (or a
// loaded save's)
void session_rt_loaded();
// orig: (host) the hero died in a session (overlay_entry.cpp, in place of level_setup(0, 1)): the soft reset
void session_respawn();
// orig: (host) once a game frame (session_frame): a loaded save's checkpoint taken once the arrival is over; the testing
// deaths (RAC1_SESSION_DIE)
void session_rt_frame(uint64_t frame);
// orig: (host) testing, once a game frame (session_frame): RAC1_SESSION_SAVE_AT / RAC1_SESSION_LOAD_AT=<frame>:<slot>, a save
// and a load of the card's slot as the pause menu's pages make them
void session_rt_test_frame(uint64_t frame);
// orig: (host) the game's checkpoint save (0x284e50: an imported class's own checkpoint) at `pos` turned `yaw`: the
// session's checkpoint
void session_checkpoint_game(const float pos[3], float yaw);

// The SDK (ABI 9)
// orig: (host) level.txt's checkpoint `name` the current one: false when the session has none of that name
bool session_checkpoint_set(const std::string& name);
// orig: (host) a checkpoint at `pos` turned `yaw` (named `name`) the current one: false outside a session
bool session_checkpoint_at(const float pos[3], float yaw, const std::string& name);
// orig: (host) the current checkpoint's name ("" outside a session), its place and yaw
std::string session_checkpoint(float pos[3], float* yaw);
// orig: (host) the placed objects' policy at a respawn (SESSION_RESPAWN_*); -1 asks it. Returns the policy now (-1: no session)
int32_t session_respawn_policy(int32_t policy);
// orig: (host) the session's respawns so far (0 outside a session)
uint32_t session_respawns();

// save<N>.pc's item of the session (advanced.cpp's file, level_slots.cpp's items)
// orig: (host) the session's item appended to `d` (a save made in a session; a loaded one not yet entered carried over):
// its count
uint32_t session_rt_pc_items(std::vector<uint8_t>& d);
// orig: (host) save<N>.pc's item `id` read: true when it is the session's
bool session_rt_pc_item(uint32_t id, const uint8_t* p, uint32_t size);
// orig: (host) a .pc about to be read: the session item of the save before forgotten
void session_rt_pc_reset();

}  // namespace rac1

#endif
