#ifndef RAC1_GAME_OVL_HERO_HOVERBOARD_H
#define RAC1_GAME_OVL_HERO_HOVERBOARD_H

#include <cstdint>

namespace rac1 {

// Hoverboard race (states 0x6b..0x6f, Zoomerator item 0x24, Rilgar L5 & Kalebo III L16) and
// Kalebo III's slide/board state 0x3e.
void hero_hoverboard_moby_sync();   // trailing substate 0x15..0x16 moby matrix sync in hero_motion_update
void hero_hoverboard_pair_table();  // sub_002321a8's 0x17c068 pair-anim table on L5 / L16
void hero_item_use_zoomerator(uint32_t mask);

void hero_state_3e_set(int32_t mode);
void hero_state_3e_update();

void hero_hoverboard_set_state(uint32_t state, int32_t mode);
void hero_hoverboard_state_update(uint32_t state);
// Returns true if hero_input_update should return immediately without running its trailing
// attached-moby / gait-timer / anim-timer epilogue (state 0x6d falling into 0x6e).
bool hero_hoverboard_input_update(uint32_t state);

}  // namespace rac1

#endif
