// Movie subtitles of our own (an enhancement: Options > Language > Movie Subtitles, `movie_subtitles` in the preferences,
// RAC1_MOVIE_SUBTITLES; off by default). Not a port: the original never shows text over its movies on any disc (its
// movie player draws none, the PSS files carry no subtitle stream).
//
// The text comes from the player's own machine: tools/movie_subtitles.py (the setup's --movie-subtitles) turns each
// movie's sound track into a SubRip file with Whisper, assets/movie-subtitles/<movie>.<lang>.srt (<movie> the movie's file
// in the extracted disc, row_XXXX; <lang> en, fr, de, es, it). The player may edit them. When a movie opens, the file of
// the text language (0x15ed88, the language the game's own subtitles follow) is read; a missing, empty or unreadable file
// shows nothing (a log line), a cue that cannot be read is skipped (a log line).
//
// A cue is drawn as the game draws a scene's subtitle (cutscene_subtitle_draw, 0x1f9a20): the small font (0x1c3150,
// effect texture 1, read from EE RAM as the game uploads it), the text wrapped into balanced lines in the 432-pixel box
// (text_box, 0x1fbf48), centred near the bottom, grey 0x80b0b0b0, over the dark rounded box of alpha 0x60. The host
// composes it into the movie's picture (the game's 512-pixel-wide screen scaled onto it), after the decode: nothing of
// the game's memory is written, its state is the same with them on or off.
#ifndef RAC1_GAME_ENHANCE_MOVIE_SUBTITLES_H
#define RAC1_GAME_ENHANCE_MOVIE_SUBTITLES_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// orig: (none) — enhancement: the setting (Options > Language > Movie Subtitles); RAC1_MOVIE_SUBTITLES (on / off) over
// it, also in the headless runs. Off while every enhancement is off unless RAC1_MOVIE_SUBTITLES says on.
bool movie_subtitles_on();
// orig: (none) — enhancement: the row's choice, kept in the preferences
bool movie_subtitles_choice();
void movie_subtitles_set(bool on);

// orig: (none) — enhancement: a movie opened (the player's open, 0x2ee130): its file at sector `lsn` named, the
// subtitles of the text language read (nothing when off or none)
void movie_subtitles_open(uint32_t lsn);
// orig: (none) — enhancement: the movie closed
void movie_subtitles_close();
// orig: (none) — enhancement: the picture to show at `seconds` of the movie: `rgba` itself (no cue then), or a copy with
// the cue drawn over it
const std::vector<uint8_t>& movie_subtitles_picture(const std::vector<uint8_t>& rgba, int w, int h, double seconds);

// --- the files (also for the tests) ---
struct MovieCue {
    double start = 0.0, end = 0.0;
    std::string text;  // UTF-8, '\n' between the lines the file gives
};
// orig: (none) — enhancement: a SubRip file's text parsed; each cue that cannot be read skipped with a line in `log`
// (`name` the file's, for the lines)
std::vector<MovieCue> movie_subtitles_parse(const std::string& srt, const std::string& name, std::vector<std::string>& log);
// orig: (none) — enhancement: UTF-8 text as the game's characters (its font's codes: ASCII, the accented letters
// 0x80..0xa7, 1 a line break; a byte that is not UTF-8 taken as Latin-1); characters the font does not have dropped
// (counted in `dropped`)
std::vector<uint8_t> movie_subtitles_to_game(const std::string& utf8, int& dropped);

}  // namespace rac1

#endif
