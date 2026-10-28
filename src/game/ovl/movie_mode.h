// Game mode 1: a movie playing. The mode is entered by 0x2995d0 (movie_start, from 0x299460 movie_play, 0x299560, 0x2994f8)
// and its update 0x2999a8 runs the whole
// movie (the player 0x2edd28) in one call, then 0x2997c8 puts the game back (mode 0, the music, the
// talk that started the movie continued). The host runs the player a frame at a time, as for
// the story's movies (movies.h): movie_mode_update is one host frame of 0x2999a8.

#ifndef RAC1_GAME_OVL_MOVIE_MODE_H
#define RAC1_GAME_OVL_MOVIE_MODE_H

#include <cstdint>

namespace rac1 {

// orig: 0x002999a8 — game mode 1's update. Split for the host's frame loop: its start (the card's
// machine idle, the sounds stopped, the player started on the movie 0x161048 / 0x16104c) and
// one frame of the player's loop on each call; when the player is done its end (movie_mode_end).
void movie_mode_update();
// orig: 0x002999a8 (the same, run to its end at once: the oracle's whole-function check)
void movie_mode_run();
// orig: (host) the debug level change (the debug menu's Levels tab, RAC1_GOTO): while game mode 1 is on (a movie started
// or playing; true then), its player is stopped at its next frame (movie_mode_update), as the player's skip test stops
// it: the movie ends as a skipped one does (its picture and sound stream closed, movie_mode_end: mode 0, the music, the
// talk). The caller changes the level once this is false. The PS2 runs the whole movie inside one main loop pass, so
// nothing could change the level in the middle of one; the host's movie runs a frame at a time, and the level's main
// loop left in the middle of it left the movie playing over the next level.
bool movie_mode_stop();
// orig: 0x002997c8 — after the movie: the vsync callback back, the GS memory the movie start
// loaded (0x2995d0: 13 images 128 x 128 PSMCT24 and 17 of 64 x 64 PSMCT32 at *(0x15ee88) /
// *(0x15ee78)) stored back into the buffer *(0x161058) (sceGsSetDefStoreImage 0x122518 /
// sceGsExecStoreImage 0x1227d8), the music started again, game mode 0, the talk that played
// the movie stepped to its next record, the HUD flag 0x10.
void movie_mode_end();
// orig: 0x00299560 — story movie `n` (table of contents +0x1938 + n * 8, PAL +0x1998) in the
// game's language (the pause menu's screens, kind 4).
void movie_story_play(int32_t n);
// orig: 0x002994f8 — movie `n` of the table at +0x1a28 (PAL +0x1a50), language 0 (the pause menu's
// screens, kind 6).
void movie_table2_play(int32_t n);

}  // namespace rac1

#endif
