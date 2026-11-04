// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/custom_waves.cpp.)
// A custom level's wave challenge (testing, local experiment: RAC1_CUSTOM_LEVEL, runtime/kernel/disc.h). Our own game
// logic, host-side, on the game's own enemies: the custom level's gameplay file places them hidden (moby +0x34 bits 0 and 1:
// not drawn, not updated) and <dir>/waves.txt (tools/custom_level/maktar.py) lists them by wave, by their uid (+0xb2):
//   wave <uid> <uid> ...      (one line a wave, in order; '#' comments)
// The hook shows the first wave a moment after the level starts, counts the kills (an enemy is killed when its slot is
// dead or reused, state +0x20 >= 0xfe or another uid / class, or when it falls below the level's death height: then it
// is killed as RAC1_KILL_UIDS does, state 0xfe), fills the nanotech and shows the next wave once a wave is cleared, and ends the round after the
// last one; the level loaded again (a death) starts it again. The HUD says the wave, the enemies left and the kills (the game's own message box, in hud_draw).
// (testing) RAC1_WAVES_AUTOKILL=<frames>: each wave's enemies killed that many frames after it shows.
// Nothing happens without a custom level, or on any other level: the game is the same.
#ifndef RAC1_HOST_CUSTOM_WAVES_H
#define RAC1_HOST_CUSTOM_WAVES_H

#include <cstdint>

namespace rac1 {

// orig: (none) — testing: once per game frame (the host's frame loop, between frames): the challenge's state stepped
void custom_waves_frame(uint64_t frame);

// The arena's crowd and arms follow the challenge (custom_classes.cpp):
enum class WavesMood { Waiting, Fighting, Cheering };
// orig: (none) — the challenge's mood: Waiting before the first wave and once the round is long over (or no challenge),
// Fighting while a wave runs, Cheering for a while after a wave is cleared or the round won
WavesMood custom_waves_mood();
// orig: (none) — how many waves have shown since the level loaded (the arms lower and drop at each)
uint32_t custom_waves_started();

// orig: (none) — testing: the HUD's draw (hud_draw, after the elements): the challenge's line, when one runs
void custom_waves_hud();

}  // namespace rac1

#endif
