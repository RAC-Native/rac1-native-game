// The voice language, apart from the text language (an enhancement: Options > Advanced > Voice, `voice_language` in the
// preferences, RAC1_VOICE_LANGUAGE). Not a port.
//
// The game has one language, 0x15ed88 (0 English, 2 French, 3 German, 4 Spanish, 5 Italian; 1 is in no menu), read by
// its texts and its voices alike. The host keeps a voice language beside it: 0x15ed88 stays the text language (the
// game's own Options > Language row), and the places that pick a voice by the language read the voice language
// instead. Those places (every one ported or host code):
//   - the scenes' and the level's dialogue (dialog_play below 10000: the level header's table 0x13a664, 0x250 bytes a
//     line, a word per language);
//   - the Vendor's lines (dialog_play_10000: the table of contents' rows 0x1a0.., 6 lines of 6 slots, the slot
//     max(language - 1, 0), chosen by the Vendor's code);
//   - the 30000 lines (the table of contents' 0x1ab8.., 0x258 bytes a language) and the 50000 lines (0xf00.., 0x18
//     bytes a line, a word per language);
//   - the movies' sound track (one per language in each PSS);
//   - Ratchet's own sounds in each level's sound bank (only the Japanese disc's differ: voice_level_bank).
// The North American disc (and the PAL one, the same) carries all five European voices: any of them can go with any
// text. Japanese comes from the Japanese disc given to the setup as a data disc (runtime/kernel/disc.h): its tables'
// language 0 is Japanese, read through its sectors' window; a line it lacks is the text language's.
// With the voice the same as the text (the default, and every headless run without RAC1_VOICE_LANGUAGE) nothing is
// read or changed: the game is the original.
#ifndef RAC1_GAME_ENHANCE_VOICE_LANGUAGE_H
#define RAC1_GAME_ENHANCE_VOICE_LANGUAGE_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// The choice: -1 the same as the text, else a game language code (0 English, 1 Japanese, 2 French, 3 German, 4 Spanish,
// 5 Italian). RAC1_VOICE_LANGUAGE (same, en, ja, fr, de, es, it) over the setting.
// orig: (none)
int32_t voice_choice();
// orig: (none) — the setting (Options > Advanced > Voice, the preferences)
void voice_choice_set(int32_t choice);
// orig: (none) — the setting's name in the preferences ("same", "en", "ja"...) and back (-2: not a name)
const char* voice_choice_name(int32_t choice);
int32_t voice_choice_from_name(const std::string& name);
// orig: (none) — the choices offered, in the Voice row's order: the same as the text, the five European voices, and
// Japanese when the Japanese disc is there (a data disc)
std::vector<int32_t> voice_choices();
// orig: (none) — the Japanese voices can be played: the Japanese disc is a data disc
bool voice_japanese_available();
// orig: (none) — the voice language now (a game language code): the choice, or the text language 0x15ed88 for "the same"
// (and for Japanese without the Japanese disc)
uint32_t voice_language();

// The voice sites: the sector the game would play (`text_lsn`, the text language's) and what the voice language plays
// instead (`text_lsn` itself when the voice is the text's, or the voice has no such line).
// orig: (none) — dialog_play's level lines (`id` below 10000)
uint32_t voice_level_line(int32_t id, uint32_t text_lsn);
// orig: (none) — dialog_play_10000's Vendor lines (`index` = id - 10000; below 36 they are 6 lines of 6 languages)
uint32_t voice_vendor_line(int32_t index, uint32_t text_lsn);
// orig: (none) — dialog_play_50000 (`index` = id - 50000)
uint32_t voice_line_50000(int32_t index, uint32_t text_lsn);
// orig: (none) — dialog_play_30000 (`index` = id - 30000)
uint32_t voice_line_30000(int32_t index, uint32_t text_lsn);
// orig: (none) — a movie (`lsn`, `bytes`, the text language's track `track`): the track to play, and the PSS to take the
// sound from when it is another disc's (`audio_bytes` 0: the movie's own)
void voice_movie(uint32_t lsn, uint32_t bytes, int32_t& track, uint32_t& audio_lsn, uint32_t& audio_bytes);
// orig: (none) — a sound bank the game hands the IOP (`bank`): a level's bank replaced by the Japanese disc's bank of the
// same level when the voice is Japanese (Ratchet's own sounds; the sounds' numbers are the same)
void voice_level_bank(std::vector<uint8_t>& bank);

}  // namespace rac1

#endif
