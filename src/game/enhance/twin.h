// The twin run (the open engine, docs/open-engine.md): the acceptance test of a class brought into a session from another
// level. The same class runs twice, from the same state: on its own level (native) and imported into a session on another
// base (its code through its level's context, context.h). Both runs log the moby frame by frame (its record, its pvars,
// the hero's place and health, the mobys it spawns); tools/twin_compare.py compares the logs, with the two runs' places
// and addresses taken out (each run's origin, its class block, pvars, hero and context image).
//
// RAC1_SESSION_TWIN=<level>:<class>@<frame> and RAC1_TWIN_FILE=<file> (testing):
//   - a run on level <level> (native): at <frame> the live moby of <class> nearest the hero is taken: its record and
//     pvars, the hero's place relative to it, written to <file>; logged from then on;
//   - a run on another level (a session): the class is imported (session.h's session_import_add); at <frame> one is
//     spawned from <file> (its record and pvars, their pointers moved to the session's: its block, its pvars, the hero,
//     its level's context) at RAC1_TWIN_AT=<x>,<y>,<z>, the hero put at the same place relative to it; logged from then on.
// RAC1_TWIN_LOG=<file>: the log (binary, tools/twin_compare.py). The twin's update draws its random numbers from a seed
// of its own in both runs (the other mobys of the two levels draw theirs from the game's), so its choices can be equal.
#ifndef RAC1_GAME_ENHANCE_TWIN_H
#define RAC1_GAME_ENHANCE_TWIN_H

#include <cstdint>

namespace rac1 {

using TwinFn = void (*)(uint32_t);
// the twin moby (0: none: the original's case, one compare per moby update)
extern uint32_t g_twin_moby;
// orig: (host) the twin's update `fn` called on it: its own random seed swapped in, the mobys it spawns noted
void twin_call(TwinFn fn, uint32_t moby);
// orig: (host) what the twin run does with moby `moby`'s update: 2 the twin's group (the twin and the mobys taken with it:
// twin_call), 1 held (RAC1_TWIN_ALONE=1: the mobys live at the twin's start, the hero's and the group's aside, are not
// updated in either run: the twin meets the hero alone, whatever the two levels have around), 0 as the game does
int twin_kind(uint32_t moby);
// orig: (host) the testing switches read at the start (session_start_tests)
void twin_start_tests();
// orig: (host) a game frame begins (the host's loop, game thread): the capture, the spawn, the log
void twin_frame(uint64_t frame);
// orig: (host) a level's full setup (session_level_begin: the twin's heap blocks are freed): the twin and its views gone
void twin_level_begin();

}  // namespace rac1

#endif
