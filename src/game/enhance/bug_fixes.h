// Bug Fixes (main menu > Options > Bug Fixes, beside Advanced; an enhancement; docs/advanced.md "Bug Fixes"): small fixes
// of the original game's own bugs, the ones no later build of the game fixed (those are 1.02 Fixes, fixes102.h). Each
// fix is a row of its own, Off / On, off by default: with every row off the game runs as the original, byte for byte. A
// global setting, kept in the preferences only (advanced_options.txt, one `bugfix_<name> 0|1` line per fix), never with
// a save; changed on the main menu only, so a playthrough sees it from its start.
//
// The page is built as the Mods page (mods_menu.h): a copy of the Camera page with its settings widget in the tall box
// and a copy of the Options help label (each row's help under it). Its EE block BFX (0xf1000..0xf2000) is the Advanced
// block's free second half (advanced.cpp: ADV 0xf0000..0xf1000 used), its text ids 0x7b00.. (below the Exit page's
// 0x7c00..).
//
// The fixes (their numbers for RAC1_BUG_FIXES):
// - 1: Rilgar's hoverboard race: quitting from its results screen (game mode 4, kind 0: cross, screen_update 0x202aa0)
//   brings the planet's music back. The original's code never switches back there (the race organiser, class 918
//   L5:0x316ab8, does it only for the race's other endings, its states 1 and 2), so the race's track kept playing on
//   the planet. The fix makes the call those endings make, the level's music routine L5:0x28f5c8 with (0, 8): the
//   level's track 0 (its music) as the next track, faded over through the track 8.
#ifndef RAC1_GAME_ENHANCE_BUG_FIXES_H
#define RAC1_GAME_ENHANCE_BUG_FIXES_H

#include <cstdint>
#include <string>

namespace rac1 {

enum : int { BUG_FIX_RACE_MUSIC = 1 };

// orig: (none) — enhancement: fix `fix` on (its row, or RAC1_BUG_FIXES over it; never while every enhancement is off)
bool bug_fix_on(int fix);

// orig: (none) — enhancement: a preferences line of ours read (`bugfix_*`); false for the others
bool bug_fixes_pref_read(const char* name, const char* value);
// orig: (none) — enhancement: our preferences lines
std::string bug_fixes_prefs_text();

// orig: (none) — enhancement: the page (re)built, its parent the Options page `parent`; its address
uint32_t bug_fixes_page_install(uint32_t parent);
// orig: (none) — enhancement: the page's address (0 when it is not built: the pause menu, every enhancement off)
uint32_t bug_fixes_page();
// orig: (none) — enhancement: the id of the page's title (its Options entry)
int32_t bug_fixes_title_text();
// orig: (none) — enhancement: a text of the page's (0x7b00..), 0 for others
uint32_t bug_fixes_text(int32_t id);
// orig: (none) — enhancement: the settings widget (0x27ef00) changed `byte` (cross on a row); true when it was ours
bool bug_fixes_setting_changed(uint32_t byte);
// orig: (none) — enhancement: the settings widget `w`, its selected row `e`: the help label's index mirrored; left /
// right step our rows; true when a row was stepped
bool bug_fixes_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed);

// orig: (none) — enhancement: fix 1, called where the results screen's cross quits (screen_update 0x202aa0, kind 0,
// step 2): on Rilgar, the level's music back
void bug_fixes_race_quit();

}  // namespace rac1

#endif
