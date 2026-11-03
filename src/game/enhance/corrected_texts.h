// Corrected texts: the North American disc's European texts as the later discs carry them, fixed (main menu > Options >
// Advanced > Corrected Texts, off by default; a global setting, kept in the preferences only). Not a port: the game reads
// its texts as before, from the North American disc; only the records of the strings a later disc corrects point
// elsewhere, at that disc's string.
//
// Where the corrections come from: a data disc that gives "the corrected texts" (tools/setup.py's GIVES), in this order:
// the PAL disc 2.00 (the "Platinum"), the PAL disc 1.00, the Japanese disc (whose European tables are the PAL disc 1.00's).
// Their levels' gameplay files (levels/levelNN/gameplay_pal.bin in the data disc's folder; the ntsc ones hold the same
// texts) are compared with the North American disc's (assets/disc/), table by table (language 0 English, 2 French, 3
// German, 4 Spanish, 5 Italian), string by string, by id. The PAL disc 2.00 numbers some ids differently (five credits
// lines added: ids 20695 and 20743..20746 are new, the ids after each of them one higher): they are taken back to the
// North American ids, the new ones left out, and its credits (ids 20649..20853) are not taken (they need the 1.02 build's
// credits code); a later disc in the order gives them, when there is one. A table whose ids differ from the North
// American one is not used. Spanish 20172 is taken from the PAL disc 2.00 only (the PAL disc 1.00, and the Japanese
// disc with it, made it a copy of 20171; 2.00 put it back).
//
// The strings that differ (about 75, 6 KB) are written to the EE block CTX (0xfe000..0xffc00, in the PS2 kernel's
// first megabyte, past the Advanced page's texts: nothing of the game or the runtime lives there) and, after the game
// has relocated a text table (title_load: the title's eight tables; gameplay_load: the level's table; the pause menu's
// help log: the title's table again), the records of those ids point at them. With the option off nothing is read,
// written or pointed elsewhere: the game is byte for byte the same.
//
// Only with the North American disc as the game's disc (the PAL discs carry these texts already). The oracle and the
// headless runs have it off (g_advanced_disabled), unless a headless run keeps the settings (RAC1_ADVANCED=1).
#ifndef RAC1_GAME_ENHANCE_CORRECTED_TEXTS_H
#define RAC1_GAME_ENHANCE_CORRECTED_TEXTS_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: a data disc gives the corrected texts and the game's disc is the North American one (the
// Corrected Texts row is shown only then)
bool corrected_texts_available();

// orig: (none) — enhancement: the corrected texts are used now: RAC1_CORRECTED_TEXTS=1 / 0 (testing) over the Corrected
// Texts row; false without a data disc that gives them, and while every enhancement is off
bool corrected_texts_on();

// orig: (none) — enhancement: the text table of language `lang` just relocated by the game (`count` records of 16 bytes at
// `table`: +0 the string, +4 the id) has its corrected strings pointed at; nothing with the option off. For a level's
// table (gameplay_load) and the help log's (help_log_text_update)
void corrected_texts_patch(uint32_t table, int32_t count, int32_t lang);
// orig: (none) — enhancement: the same for the title's tables (title_load, languages 0..7 in order), kept in mind until a
// level's load so that the main menu's row acts on them at once
void corrected_texts_title_patch(uint32_t table, int32_t count, int32_t lang);
// orig: (none) — enhancement: the Corrected Texts row changed (on the main menu): the title's tables patched, or their
// original strings put back
void corrected_texts_row_changed();

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_CORRECTED_TEXTS_H
