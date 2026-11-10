// The open engine's sections and portals (docs/open-engine.md, "Sections (stage 6)"; DESIGN section 7): a session (a level
// mod of format 3, session_runtime.h) made of **sections**, each with its own collision, sky, meshes, lights, music, fog and
// background, death height and objects, switched by **portals** (level.txt's `portal` boxes, or the plugin's section_enter)
// with no level load and no reset: the hero, his weapons and ammo, the plugins' state and the session's imported classes stay;
// the switch is made in one frame at the start of a game frame (the collision's pointers, the sky's, the settings, the
// leaving section's objects by its policy, the entering one's, the music, the lights, Ratchet at the entry with the camera cut
// behind him, an optional fade drawn over the HUD). A section other than main ("main": level.txt's lines outside any section
// block, where the ship lands) is **streamed in**: its files read and its gameplay file decompressed on a worker thread, then
// committed on the game's thread within a budget a frame (2 ms by default): copied into the session heap (blocks the section
// owns, "section <name>"), its collision's list and its sky relocated, its objects of the base level's classes placed by the
// game's own rules (section_spawn_placements: gameplay_load's moby part, for one section's gameplay file, into the running
// session), its classes of other levels spawned; until it is entered they are frozen (neither updated nor drawn, out of the
// collision grid). Only the current section's meshes are drawn (host_meshes_sections). A death in a section brings Ratchet
// back at the checkpoint in its section and that section's killed objects back (its respawn policy). Nothing here runs unless
// the current level mod's build has a sections.txt (g_sections_on): formats 1 and 2, and format 3 without sections, are as
// before.
#ifndef RAC1_GAME_ENHANCE_SECTIONS_H
#define RAC1_GAME_ENHANCE_SECTIONS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// (a session with sections is current: every hook's test)
extern bool g_sections_on;

// A section's objects when it is left (level.txt's `section <name> remove|freeze|keep`, main's `leave`; the SDK's section_policy)
constexpr int32_t SECTION_REMOVE = 0;  // taken out of play (killed ones stay so when it is entered again; the others come back as placed)
constexpr int32_t SECTION_FREEZE = 1;  // kept where they are, neither updated nor drawn, out of the collision grid
constexpr int32_t SECTION_KEEP = 2;    // kept running

// orig: (host) a session's level header read (session_rt_level): the build's sections.txt read (its sections, entries,
// checkpoints, portals, settings); "" (or none): off
void sections_level(const std::string& build_dir);
// (stage 10) Bosses: a section's `boss` lines name classes whose objects are its bosses (and the plugin's boss_set names one).
// A death in the section takes a boss still in play out and places it again (the fight from its start); a boss leaving play
// otherwise is beaten: logged, the plugin's boss callback, never placed again in this visit of the session.
// orig: (host) the object `moby` (in play, in the current section) one of its bosses: false when not (no sections, gone)
bool sections_boss_set(uint32_t moby);
// orig: (host) the bosses beaten in section `name` ("" the current one) in this visit of the session; -1 no such section
int sections_boss_beaten(const std::string& name);
// orig: (host) (stage 10) section `index`'s rail table: where the mod's own rails start (a world's section: after its level's; 0
// otherwise)
int32_t sections_rail_first(int index);
// orig: (host) (stage 10) a memory card save's image about to be made: the session's own level state (its level records) put back
// in place of a world section's (sections.cpp: a world's level state); the level whose state was in (-1 none), for
// sections_save_end to put it back after the image
int sections_save_begin();
void sections_save_end(int level);
// orig: (host) the session's arrival (session_rt_loaded): main's collision, sky, settings and music taken as the game set
// them, main's classes of other levels placed, the other sections' streaming started
void sections_arrival();
// orig: (host) once a game frame of the session (session_rt_frame, at the frame's start on the game's thread): the streaming's
// commits within the budget, the portals, the fades, a switch asked by the plugin, the testing switches (RAC1_SECTION_TEST)
void sections_frame(uint64_t frame);
// orig: (host) the HUD's draw (mod_code_hud): a switch's fade over the screen
void sections_hud();

// orig: (host) a session with sections is on
bool sections_any();
// orig: (host) a section switch asked and not over (its streaming, its fade): the SDK's game_save waits for it
bool sections_switching();
// orig: (host) section `index` a game world's (`world <level>`)
bool sections_world(int index);
// orig: (host) the sections' count (main included; 0 none), the current one (0 main; -1 no session with sections), a
// section's name (index) and index (name; -1 none)
int sections_count();
int sections_current();
std::string sections_name(int index);
int sections_index(const std::string& name);
// orig: (host) the level whose sky the current section has drawn (its sky line), -1 when the sky is main's (or no sections)
int sections_sky_level();
// orig: (host) level.txt's start line: the section the session starts in (0 none: main, where the ship lands), its entry
int sections_start();
bool sections_start_entry(float pos[3], float* yaw, std::string* name);
// orig: (host) section `index` made the current one now, with no fade and Ratchet left where he is (a loaded save's
// checkpoint, level.txt's start): false while it is not streamed in yet (its streaming started)
bool sections_switch_now(int index);

// Deaths (session_respawn)
// orig: (host) before the soft reset's hero: the checkpoint's section `index` made the current one at once (no fade)
void sections_respawn_prepare(int index);
// orig: (host) the respawn policy of the current section (its own, else the session's `session_policy`)
int32_t sections_respawn_policy(int32_t session_policy);
// orig: (host) the soft reset of the current section's objects (`policy` SESSION_RESPAWN_RESET: the killed ones placed again
// as at its entry, the game's killed-for-good ones not; main's classes of other levels placed too): their counts
void sections_respawn_objects(int32_t policy, size_t* back, size_t* kept_dead);

// orig: (host) after the soft reset (stage 9): the current section's hero mode (its hero line's) entered again at the next
// frame of play, a new object of it where Ratchet stands
void sections_hero_respawned();

// Saves (session_runtime.cpp's .pc item)
// orig: (host) section `index`'s objects gone (killed since it was entered: not to come back until a death there), by uid
void sections_gone_of(int index, std::vector<int32_t>& uids);
// orig: (host) a loaded save's: section `name`'s objects gone
void sections_gone(const std::string& name, const std::vector<int32_t>& uids);

// The SDK (ABI 10: sdk/rac1_mod.h)
// orig: (host) section `name` streamed in (1: started or already in, 0: no such section, or no session with sections)
int section_load(const std::string& name);
// orig: (host) section `name` streamed in and committed: 1, 0 not yet, -1 no such section
int section_ready(const std::string& name);
// orig: (host) Ratchet to section `name`'s entry `entry` ("": its first) with a fade of `fade` frames (0 none) at the next
// frame's start, once it is streamed in: 1 asked, 0 not (no such section or entry)
int section_enter(const std::string& name, const std::string& entry, int32_t fade);
// orig: (host) section `name` unloaded (its objects out of play, its heap blocks given back): 1 done, 0 not (the current one,
// main, or no such section)
int section_unload(const std::string& name);
// orig: (host) section `name`'s policy when it is left (SECTION_*), -1 only asks: the policy now, -1 no such section
int32_t section_policy(const std::string& name, int32_t policy);

// The equivalence test (testing: RAC1_SECTION_EQUIV=1, with a session's memory: RAC1_SESSION_MEM=1)
extern bool g_section_equiv;
// orig: (host) gameplay_load's mobys and their pvars placed (the decompressed gameplay at `H`): the level's gameplay file read
// again from the disc, placed by section_spawn_placements into the same moby slots and the session heap, every moby record and
// pvar block compared with the native load's (the pvar pointers rebased): `[section] equivalence level N: ...`
void section_equiv_native(uint32_t H);

}  // namespace rac1

#endif
