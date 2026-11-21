// Small moby helpers several of the levels 11, 13, 16 and 17 carry copies of (shared_moby_helpers.cpp): a group's members
// shown / hidden / made solid, a moby spawned at a place, an eased value between two times, a moby's state tested. Phase
// 1 of stage 4: the levels' generated copies still run; these are proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_MOBY_HELPERS_H
#define RAC1_GAME_OVL_SHARED_MOBY_HELPERS_H

#include <cstdint>

namespace rac1 {

// The live members of `group` (the group lists 0x1ab840): each of the three switches -1 leaves it, else `far` 0 sets the
// +0x34 flag 2 (else clears it), `shown` 0 hides the moby (+0x31 0, flag 1) or shows it, `solid` 0 takes its collision
// away or gives it its class's.
void group_set_flags(int32_t group, int32_t far, int32_t shown, int32_t solid);  // L13:0x002650a8 L16:0x00254a30 L17:0x002539c8
// A moby of class `cls` at `pos` (an EE vector), shown, in its state 0, coloured as the hero; the moby or 0.
uint32_t spawn_at_place(uint32_t pos, uint32_t cls);  // L13:0x002e1c50 L17:0x002c5360
// The value between `from` and `to` at the time `t` of `t0` .. `t1` (before t0: `from`), the fraction raised to
// 2^(power - 1); `angles` eases angles the short way round.
float ease_between(int32_t angles, int32_t power, float t, float from, float to, float t0, float t1);  // L13:0x002ba088 L17:0x002ec270
// Whether the moby is in the state `state` (Pokitaru's two copies test 5, Kalebo III's 6).
bool moby_in_state(uint32_t m, uint32_t state);  // L11:0x0030a830 / 0x0030e078 (5), L16:0x002c4710 (6)

}  // namespace rac1

#endif
