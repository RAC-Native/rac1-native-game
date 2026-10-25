// The player's progress for the RAC Native launcher (github.com/RAC-Native/rac-native-launcher). Host code:
// progress.json in the data folder (platform::data_dir()), rewritten when the game goes to another level and
// once a minute in a level. The launcher shows the current planet, the planets reached and the time played.
//
//   {"planet": "novalis", "planet_name": "Novalis", "level": 1, "visited": ["veldin", "novalis"], "percent": 11,
//    "playtime_s": 1234, "updated": "2026-10-25T10:00:00Z"}
//
// The planet is the level being played (0x15ed84). The planets reached and the time played are counted here, not
// read from the memory card: they follow the last game played, and start again with a new game (the title going
// to Veldin, level 0: a loaded save goes to its own planet). Never in a headless run unless RAC1_PROGRESS is set.
#ifndef RAC1_HOST_PROGRESS_H
#define RAC1_HOST_PROGRESS_H

#include <string>

namespace rac1::progress {

// orig: (none) — host: turns the progress on, kept in `path` (the game's own: data_dir()/progress.json;
// testing: RAC1_PROGRESS=<file> elsewhere, RAC1_PROGRESS=0 none)
void start(bool headless);

// orig: (none) — host: once a host frame, on the game's thread: the level played, whether the title (the boot
// module) runs, whether a level exit (its loading screen) runs
void frame(int level, bool title, bool exiting);

}  // namespace rac1::progress

#endif
