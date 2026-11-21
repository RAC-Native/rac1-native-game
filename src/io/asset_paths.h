#pragma once
// The folder the game's run-time data is in, and the paths in it (docs/setup.md, "Where the data goes").
//
// The setup makes the run-time data from the player's disc: the disc's files (disc/), the boot executable (dumps/), the
// overlay maps (generated/overlay_map/), the converted models and worlds (generated/mrp/), the asset pool, the mods'
// builds, the movie subtitles. Every file the game reads there is named through asset_path(), relative to one root:
//   1. RAC1_ASSETS, when set (a relative path is taken against the current directory, once, at the first use);
//   2. else the first line of assets.txt next to the executable (`rac1-native --setup --output-dir <dir> --remember`
//      writes it; a relative path in it is taken against the executable's folder);
//   3. else "assets", relative to the current directory: what every run did before there was a choice, string for
//      string (a run from the repository's root reads assets/... as it always did).
// The setup's --output-dir / --assets sets the root of its own process (asset_root_set) before anything reads a path.
// (The generated sources, assets/generated/src/, are a build input, not run-time data: they stay in the repository.)
//
// The root is decided once and never moves: callers may keep what asset_path() returns. A lookup builds one string;
// call it where a file is opened, not per frame.

#include <string>

namespace rac1::io {

// The root, without a trailing separator ("assets" by default)
const std::string& asset_root();
// asset_root() + "/" + rel ("assets/generated/overlay_map/level3.txt" by default)
std::string asset_path(const char* rel);
std::string asset_path(const std::string& rel);
// Where the root came from: "default", "RAC1_ASSETS", "--output-dir", or the pointer file's path
const std::string& asset_root_source();
// The setup's --output-dir: this process's root. Only before the first asset_root() / asset_path(): later the root
// stays as it was decided, and this returns false (said on stderr).
bool asset_root_set(const std::string& dir, const char* source = "--output-dir");
// The pointer file: assets.txt in the executable's folder ("" when that folder cannot be found)
std::string asset_root_pointer_file();
// Writes `dir` (made absolute) into the pointer file; false with `why` when it cannot
bool asset_root_remember(const std::string& dir, std::string& why);
// Removes the pointer file (true when there is none afterwards)
bool asset_root_forget(std::string& why);

}  // namespace rac1::io
