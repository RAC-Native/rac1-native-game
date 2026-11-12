// Mods (docs/mods.md): the mods/ folder next to assets/, one subfolder per mod with its manifest (mod.json: name, author,
// version, description, priority) and its files:
//   mods/<folder>/mod.json
//   mods/<folder>/disc/...                files laid out as the setup's extracted disc files (assets/disc/, its layout.txt's
//                                         paths): each one read instead of the disc's (disc.cpp), the same size only
//   mods/<folder>/textures/<hash>.png     a texture replaced by its content hash (game/render/texture_mods.h)
//   mods/<folder>/level/...               a level mod's level (the manifest's "kind": "level"): the setup composes it
//                                         with the player's disc into assets/generated/mods/<folder>/ (its build), and
//                                         the game runs it on a host level slot (src/host/level_slots.h)
// Which mods are on is the player's choice (main menu > Options > Mods), kept in the preferences (mods.txt in the config
// folder: "<folder> on|off" lines; a mod never chosen is on); a change applies at the next start. When two mods give the
// same file, the higher priority wins (ties: the folder name first in alphabetical order), and the conflict is logged.
//
// Nothing here runs unless the game's start calls mods_start: the setup's own reads (its --compare-disc) never see a mod.
// A headless run (the tests, the reference checks) is vanilla: no mod is looked at unless RAC1_MODS=1 (then every mod
// found is on, the player's preferences not read, unless RAC1_MODS_FILE names a preferences file). RAC1_MODS=0 turns
// every mod off in a window too. RAC1_MODS_DIR: another mods folder (testing).
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rac1 {

struct ModInfo {
    std::string folder;             // its folder's name in mods/ (its key in the preferences)
    std::filesystem::path dir;      // mods/<folder>
    std::string name, author, version, description;  // the manifest's (name: the folder's when it has none)
    int priority = 0;               // the manifest's; higher wins a conflict
    bool enabled = true;            // the player's choice (applies at the next start)
    bool active = false;            // on in this run (its files used)
    size_t disc_files = 0;          // the files under disc/
    size_t textures = 0;            // the PNGs under textures/
    std::string error;              // its manifest could not be read: never active
    // a level mod (the manifest's "kind": "level"; docs/mods.md, "Level mods"): a new level the ship flies to
    bool level = false;
    std::string level_id;           // the manifest's level.id: the level's stable key (its progress in save<N>.pc,
                                    // RAC1_GOTO=<id>@F), never the folder's name
    int level_format = 0;           // the level folder's format (level.format: 1)
    int level_base = 0;             // the game's level it runs on (level.base: its code, its address map; 0 Veldin)
    std::string level_wait;         // why it is not on although chosen ("" when it is): no build, an old build, ...
    int level_slot = -1;            // the host's level slot (19, 20, ...) while it is on in this run, else -1
    // its build's choices (build.json), read when it is on: the music (kLevelMusicBase the base level's, kLevelMusicNone
    // none, 0..18 that level's tracks, kLevelMusicOwn the build's music files: level_music_files, the intro then the loop)
    int level_music = -2;
    std::vector<std::string> level_music_files;
    // its other music (level.txt's music_track lines, build.json's "music_tracks"): each a name and its files (the intro, then
    // the loop), after the music's files in the level's window; the plugins' music_track plays one
    struct LevelTrack {
        std::string name;
        std::vector<std::string> files;
    };
    std::vector<LevelTrack> level_tracks;
    int level_sky = -1;             // the level whose sky the build has (level.txt's sky line: 0..18), -1 the base level's
    int level_objects = 0;          // the placed objects (level.txt's moby lines): uids 1..level_objects (the hero's skipped)
    // the spawn pools (level.txt's spawn lines): per class, the uids of the objects the composer placed for the plugins'
    // base_spawn (taken out of play when the level loads, each one's moby and pvars kept to start again)
    struct LevelPool {
        int cls = 0;
        std::vector<int> uids;
        int pvar_size = 0;
    };
    std::vector<LevelPool> level_pools;
    // its code (docs/custom-levels.md, "Code"): a plugin/ folder (a native plugin the setup builds from its source into
    // assets/generated/mods/<folder>/plugin/). A mod with code is off until the player turns it on and confirms it (Options
    // > Mods); a headless run loads code only with RAC1_MOD_CODE=1 (RAC1_MODS=1 too)
    bool code = false;
    bool code_on = false;           // its plugin is to be loaded in this run (game/enhance/mod_code.cpp)
    std::string code_wait;          // why its code cannot be loaded although chosen ("" when it can): no build, an old one
    std::filesystem::path plugin;   // its built plugin (the library)
    std::string code_error;         // its plugin could not be loaded or started (mods_code_failed): the rest of it runs
};

constexpr int kLevelMusicBase = -2, kLevelMusicNone = -1, kLevelMusicOwn = 100;

// The plugins' file name in their build folder (sdk/rac1_mod.cmake: a CMake MODULE named rac1_mod)
#if defined(_WIN32)
constexpr const char* kPluginFile = "rac1_mod.dll";
#else
constexpr const char* kPluginFile = "rac1_mod.so";  // (CMake's MODULE suffix on macOS too)
#endif

// The level composer's version (src/setup/level_compose.cpp): a build made by another version is made again by the
// setup, and the game does not use it ("needs setup")
constexpr int kLevelComposerVersion = 9;
// The level folder formats this build reads (level.format): 1 (on Veldin, level 0), 2 (on a base level of the allow-list), 3 (a
// session: loaded once, a death a soft reset; on Veldin by default, or a base level of the allow-list: docs/open-engine.md)
constexpr int kLevelFormat = 3;
// The base levels a level mod may run on (level.base, format 2; format 1: 0 only), each tested: Veldin (0), Kerwan (3),
// Eudora (4), Kalebo III (16). docs/mods.md, "Base levels"
constexpr int kLevelBases[] = {0, 3, 4, 16};

// orig: (none) — enhancement: `base` is one of kLevelBases
bool mods_level_base_allowed(int base);
// The first host level slot (the game's levels are 0..18)
constexpr int kFirstLevelSlot = 19;
// How many host level slots there are (19 .. 34: the disc layer's level windows, the ship's list's rows permitting)
constexpr int kLevelSlots = 16;

// orig: (none) — enhancement: the mods looked for (the game's start, before the disc's first read): `headless` a test run
void mods_start(bool headless);
// orig: (none) — enhancement: at least one mod is on in this run
bool mods_on();
// orig: (none) — enhancement: the mods found (empty before mods_start, or in a vanilla run), in their load order: the
// highest priority first
const std::vector<ModInfo>& mods_list();
// orig: (none) — enhancement: the mods on in this run, in their load order (the first wins a conflict)
std::vector<const ModInfo*> mods_active();
// orig: (none) — enhancement: the player's choice for mod `i` of mods_list(), written to the preferences (applies at the
// next start)
void mods_set_enabled(size_t i, bool on);
// orig: (none) — enhancement: some mod's choice differs from what this run uses (a restart applies it)
bool mods_restart_pending();
// orig: (none) — enhancement: the mods folder ("mods", or $RAC1_MODS_DIR)
std::filesystem::path mods_dir();

// orig: (none) — enhancement: for the disc layer (disc.cpp): the file of the disc's layout path `rel` ("levels/level03/
// data.wad") from the mods on, `size` bytes as the layout has it; empty when no mod gives it (or none of the right size:
// logged, the disc's own read)
std::filesystem::path mods_disc_file(const std::string& rel, uint64_t size);
// orig: (none) — enhancement: every file under `sub` ("textures") of the mods on, by its path relative to there, the
// winner of each name (a conflict logged)
std::vector<std::pair<std::string, std::filesystem::path>> mods_files(const char* sub);

// ---- Level mods (docs/mods.md, "Level mods") ----
// orig: (none) — enhancement: mods/<folder>/mod.json read into `m` (its folder and dir set by the caller); false (m.error
// said) when it cannot be. The setup's level composer reads the manifests with it (it runs without mods_start).
bool mods_manifest_read(ModInfo& m);
// orig: (none) — enhancement: where a level mod's build is (the setup's level composer writes it there, from the player's
// disc: never in the mod): assets/generated/mods/<folder>
std::filesystem::path mods_level_build_dir(const std::string& folder);
// orig: (none) — enhancement: the stamp of a level mod's own files (mod.json and everything under level/: their paths,
// sizes and bytes), which its build records (build.json): another stamp means the build is old
uint64_t mods_level_inputs_stamp(const std::filesystem::path& mod_dir);
// orig: (none) — enhancement: why the build of level mod `m` cannot be used ("" when it can): none, made by another
// composer version, or from other files than the mod's
std::string mods_level_build_check(const ModInfo& m);
// orig: (none) — enhancement: the stamp of a mod's plugin sources (everything under plugin/), which its plugin's build records
// (plugin.json): another stamp means the build is old
uint64_t mods_plugin_inputs_stamp(const std::filesystem::path& mod_dir);
// orig: (none) — enhancement: why the plugin of mod `m` cannot be loaded ("" when it can): none built, built from other
// sources, by another SDK
std::string mods_plugin_build_check(const ModInfo& m);
// orig: (none) — enhancement: mod `m`'s plugin failed to load or start (`why`: Options > Mods shows it)
void mods_code_failed(const ModInfo* m, const std::string& why);
// orig: (none) — enhancement: the mods on in this run whose code is loaded (mod_code.cpp loads them)
std::vector<const ModInfo*> mods_code();
// orig: (none) — enhancement: the level mods on in this run (each with its host slot), in their load order
std::vector<const ModInfo*> mods_levels();
// orig: (none) — enhancement: the level mod on host slot `slot`, else null
const ModInfo* mods_level_at_slot(int slot);
// orig: (none) — enhancement: the level mod on whose level.id is `id`, else null
const ModInfo* mods_level_by_id(const std::string& id);

}  // namespace rac1
