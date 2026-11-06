// Mods (docs/mods.md): the mods/ folder next to assets/, one subfolder per mod with its manifest (mod.json: name, author,
// version, description, priority) and its files:
//   mods/<folder>/mod.json
//   mods/<folder>/disc/...                files laid out as the setup's extracted disc files (assets/disc/, its layout.txt's
//                                         paths): each one read instead of the disc's (disc.cpp), the same size only
//   mods/<folder>/textures/<hash>.png     a texture replaced by its content hash (game/render/texture_mods.h)
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
};

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

}  // namespace rac1
