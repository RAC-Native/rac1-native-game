#pragma once

#include <utility>

#include "common/common_types.h"

#include "game/sound/989snd/vagvoice.h"

namespace snd {

extern const u16 NotePitchTable[];
extern const VolPair normalPanTable[181];

// rac1: rand() of the C library replaced by a generator of each thread with the same sequence (glibc's TYPE_3
// additive generator, never seeded: seed 1). The IOP's sound runs twice, the game's 989snd and the host's mirror on the
// audio thread (docs/decisions.md, decision 13): one shared rand() would make each depend on the other's calls.
int Rand();
// rac1: the calling thread runs the game's 989snd from now: its Rand() is the game's generator, one for every such thread
// (the main thread, the game thread of runtime/ee/game_thread.h: the game's sound is never on both at once), so the sequence
// is the game's whatever thread runs a game frame. The other threads (the mirror's) keep a generator of their own.
void RandUseGame();

u16 PS1Note2Pitch(s8 center_note, s8 center_fine, short note, short fine);
u16 sceSdNote2Pitch(u16 center_note, u16 center_fine, u16 note, short fine);
std::pair<s16, s16> PitchBend(Tone& tone,
                              int current_pb,
                              int current_pm,
                              int start_note,
                              int start_fine);

}  // namespace snd
