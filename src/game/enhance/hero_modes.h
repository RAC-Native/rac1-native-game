// The open engine's hero contexts and hero modes (docs/open-engine.md, "Hero contexts (stage 9)"; DESIGN 5.8 and stage 9).
//
// A session runs on its base level's overlay (Veldin's): its hero code is that level's build. Another level's own hero build
// (Kalebo III's, which rides the race's hoverboard; Quartu's, Giant Clank's planet) and its own camera code can run Ratchet
// instead: a **hero context**. The Veldin ports of the hero controller already dispatch to a level's transliterated build
// while that level is the translation target (`on_kalebo()`...); in a session the hero context makes those lines take the
// level's branch (variants.h on_hero_level) while the session's overlay stays loaded, and the build's own addresses go
// through the level's context (context.h: its data copied and relocated, its code tags). Levels with a hero build or camera
// code of their own the predicates know: Rilgar (5), Orxon (10), Gemlik (13), Quartu (15), Kalebo III (16).
//
// A **hero mode** is one of the game's own: Ratchet riding another object that becomes the hero's moby (`hero_mode_enter`:
// Clank, the form 1, Orxon's class 87; Giant Clank, the form 2, Quartu's class 419), or on the hoverboard (Kalebo III's or
// Rilgar's class 439: the board state 0x6b of a build that has it). Each is entered with the game's own call, and left with
// it (`hero_mode_exit`: Ratchet back where the mode's object is, his health and state as the game puts them back).
//
// The hero's **sequences** (his animations) are his class's slots, filled from the level's core at its load; a base whose
// core lacks some (Kalebo III's: no magnetic walk) gets them from another level's core when the hero is about to play one
// (Veldin's has every one), read from the disc on a worker, put above the game's 32 MB (host_layout::HERO_SEQS).
//
// Nothing here runs on the original's levels: the hero context only in a session, the sequences only on a level mod's level.
#ifndef RAC1_GAME_ENHANCE_HERO_MODES_H
#define RAC1_GAME_ENHANCE_HERO_MODES_H

#include <cstdint>
#include <string>

namespace rac1 {

// The modes (the SDK's RAC1_HERO_*): the game's form byte PLAYER+0x20a4 for 1 and 2; the board is ours (the board state 0x6b)
constexpr int32_t HERO_MODE_NONE = 0;
constexpr int32_t HERO_MODE_CLANK = 1;        // Clank (Orxon's class 87): the game's form 1, his states 0x43..
constexpr int32_t HERO_MODE_GIANT_CLANK = 2;  // Giant Clank (Quartu's class 419): the game's form 2, his states 0x5a..0x61
constexpr int32_t HERO_MODE_BOARD = 4;        // the hoverboard (class 439 of Kalebo III or Rilgar): the board state 0x6b
constexpr int32_t HERO_MODE_SHIP = 5;         // the ship (Gemlik's class 69): not done (docs/open-engine.md)

// orig: (host) level `level` has a hero build or camera code of its own a hero context can run (5, 10, 13, 15, 16)
bool hero_build_known(int level);
// orig: (host) the hero code and camera code of level `level` run Ratchet from the next call on (-1: the session base's own):
// its context made when it has none. False (nothing changed) outside a session, for a level without a build of its own, or
// when its context cannot be made. `why` for the log
bool hero_build_set(int level, const char* why);
// orig: (host) the hero context now: its level, -1 the base's
int hero_build_level();

// orig: (host) Ratchet in mode `mode` (HERO_MODE_*) riding the object `moby` (an object of that mode's class), or out of the
// mode he is in (HERO_MODE_NONE, moby ignored). False when it cannot be: no hero, dying or lost, another mode first, the
// object not of the mode's class (or its form unknown to the hero build: a board needs a build with the board state)
bool hero_mode_set(int32_t mode, uint32_t moby, const char* why);
// orig: (host) Ratchet's mode now (HERO_MODE_*): the game's form, or the board while he rides one
int32_t hero_mode_now();
// orig: (host) the class a mode's object is (87, 419, 439, 69), 0 none
uint32_t hero_mode_class(int32_t mode);

// The sequences (hero_anim_set's ask)
extern bool g_hero_seq_on;  // (a level mod's level: hero_anim_set asks hero_seq_need; never on the original's levels)
// orig: (host) the hero is about to play his sequence `set`: when his class's slot for it is empty, filled from a donor
// level's core (the hero context's level's, else Veldin's) if that core has it
void hero_seq_need(uint32_t set);

// The level's lifecycle
// orig: (host) a level's header read (session_rt_level): `level_mod` a level mod's (any format, its base level `base`),
// `session` a format 3 one's: the hero context forgotten (the game's load makes the hero again), the sequences' slots
// forgotten; on a base other than Veldin's, Veldin's core read on the worker for the hero's empty slots
void hero_modes_level(bool level_mod, bool session, int base);
// orig: (host) a level mod's build's hero.txt (format 1 / 2: level.txt's `hero generic`): the generic hero code (Veldin's ports)
// runs Ratchet on that level, its base's own hero build (Kalebo III's) not (its lava, its states: the generic ones)
void hero_modes_build_file(const std::string& build_dir);
// orig: (host) once a game frame (session_frame)
void hero_modes_frame(uint64_t frame);
// orig: (host) a session's soft reset is about to make the hero again (session_respawn): his mode left first (his own moby
// back as the hero's)
void hero_modes_respawn_begin();

}  // namespace rac1

#endif
