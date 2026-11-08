// The mods' plugins (docs/custom-levels.md, "Code"): a mod's plugin/ folder (a CMakeLists.txt including
// sdk/rac1_mod.cmake, and its sources) built on the player's machine, from the source, with CMake and the compilers the
// game was built with, into assets/generated/mods/<folder>/plugin/ (rac1_mod.so / .dll and plugin.json, the stamp of the
// sources). A plugin/prebuilt/<kPluginFile> with no CMakeLists.txt is copied there instead (optional, not recommended:
// the player cannot see what it is built from). `rac1-native --setup --mod-plugins` builds every one whose build is
// missing or old (also part of --level-mods, the setup's last step), `--mod-plugin <folder>` one, always.
#ifndef RAC1_SETUP_MOD_PLUGINS_H
#define RAC1_SETUP_MOD_PLUGINS_H

#include <string>

namespace rac1::setup {

// orig: (host) mods/<folder>'s plugin built (`force`: even when current); false (said on stderr) when it cannot be
bool build_mod_plugin(const std::string& folder, bool force);

// orig: (host) every mod's plugin in mods/ built whose build is missing or old (`force`: all); false when one failed
bool build_mod_plugins(bool force);

}  // namespace rac1::setup

#endif
