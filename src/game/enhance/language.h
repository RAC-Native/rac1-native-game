// The "Language" options page: the text language, the voice language and the subtitles together, as left / right rows
// (an enhancement). Not a port: built from the game's own widgets as the Advanced page (advanced.h).
//
// The game itself has one language, 0x15ed88 (0 English, 2 French, 3 German, 4 Spanish, 5 Italian), chosen on the main
// menu's Options > Language page (a list of the five, a tick by the current one; the list's action 9 writes the
// entry's value there) and nowhere in the pause menu: the level load copies the texts of that language (0x241940), so
// it cannot change during a level. Its subtitles are a switch of the pause menu's Options > Subtitles page (0x15ee40,
// one of the game's options, in its save), absent from the main menu.
//
// The page: Options > Language in the main menu (the game's own entry pointed at it) and in the pause menu (added):
//   - Text: the game's language 0x15ed88, as the game's own list writes it (main menu only, as the game's own: in the
//     pause menu the level's texts are already loaded);
//   - Voice: the voice language (voice_language.h; `voice_language` in the preferences, RAC1_VOICE_LANGUAGE);
//   - Subtitles: Off / On, kept in the preferences (`subtitles`) and put in the game's switch 0x15ee40 whenever the game
//     reads a state (a new game, a loaded save, the title's): what the player chose last, on this page or on the game's
//     own Subtitles page, is what every game gets. Without the key (never chosen) the game's own value stays.
// With every enhancement off (the headless runs, the oracle) nothing here runs.
#ifndef RAC1_GAME_ENHANCE_LANGUAGE_H
#define RAC1_GAME_ENHANCE_LANGUAGE_H

#include <cstdint>
#include <string>

namespace rac1 {

// --- the menu's side (advanced.cpp, graphics.cpp, the settings widget's hooks) ---
// orig: (none) — enhancement: the page (re)built, its parent the Options page `parent`; `front`: the main menu's (the
// Text row shown); returns the page's address
uint32_t language_page_install(uint32_t parent, bool front);
// orig: (none) — enhancement: the page's address (0 before it is built)
uint32_t language_page();
// orig: (none) — enhancement: the ids of the page's title and of its Options help text
int32_t language_title_text();
int32_t language_help_text();
// orig: (none) — enhancement: a text of the page's, 0 for others
uint32_t language_text(int32_t id);
// orig: (none) — enhancement: the settings widget (0x27ef00) changed `byte` (cross on a row: +1); true when it was ours
bool language_setting_changed(uint32_t byte);
// orig: (none) — enhancement: the settings widget `w`, its selected row `e`: left / right step our rows; true when done
bool language_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed);
// orig: (none) — enhancement: the main menu's Options entries (`entries`, `n` of them, the game's copied): the game's
// Language entry (the one opening the page of the language list) pointed at our page
void language_main_entries(uint32_t entries, uint32_t n);

// --- the settings ---
// orig: (none) — enhancement: a line of the preferences (advanced_options.txt); true when it was ours
bool language_pref_read(const char* name, const char* value);
// orig: (none) — enhancement: our lines of the preferences
std::string language_prefs_text();
// orig: (none) — enhancement: the game read a state (the disc's default save: a new game, the title; or a save): the
// Subtitles choice put in the game's switch
void language_game_state_read();
// orig: (none) — enhancement: the game's own switches page (0x27ea70) switched the byte `byte`: the game's Subtitles
// switch kept as the choice
void language_switch_changed(uint32_t byte);

}  // namespace rac1

#endif
