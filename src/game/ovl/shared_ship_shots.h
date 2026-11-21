// The shots of the space combat (Pokitaru, Gemlik Base, Drek's fleet): class 1009, a homing shot that picks a target ahead
// of it (a moby of the target list within a cone and a range) every sixth frame and turns towards it at a limited rate, and
// class 1017, a straight shot trailing sparks; both burst into sparks where they meet something (shared_ship_shots.cpp).
// One port of the three levels' copies of their updates and of the target search. Phase 1 of stage 4: the levels'
// generated copies still run; these are proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_SHIP_SHOTS_H
#define RAC1_GAME_OVL_SHARED_SHIP_SHOTS_H

#include <cstdint>

namespace rac1 {

void class1009_update(uint32_t m);  // L11:0x003089d0 L13:0x00302780 L17:0x002e22b0
void class1017_update(uint32_t m);  // L11:0x00309098 L13:0x00302e48 L17:0x002e2978
// A homing shot (class 1009) of `owner` from `pos` along `vel` (EE vectors) at `speed` (0 or less: the velocity's own),
// flying `range` (its frames range / speed; +0x32 the range too), its glow full, its scale the owner's part of its class's,
// lit. The shot, or 0.
uint32_t class1009_launch(uint32_t owner, uint32_t vel, uint32_t pos, float range, float speed);  // L11:0x00308848 L13:0x003025f8 L17:0x002e2128
// A straight shot (class 1017) the same way, without collision, its hit record's reach `reach`, its timer 13 frames short.
uint32_t class1017_launch(uint32_t owner, uint32_t vel, uint32_t pos, float range, float speed, float reach);  // L11:0x00308f48 L13:0x00302cf8 L17:0x002e2828
// The best target from `pos` (an EE vector) for a flight turned as `rot` (+4 the pitch, +8 the yaw): a moby of the target
// list (0x15ffe4, linked by +0x28; flagged 0x1000, of type 5) within `range` across and within `yaw_cone` / `pitch_cone`
// of the flight's yaw / pitch (0.4 over its place), the one whose angles' squares' product x 30 plus the square root of its
// distance is least (under 10); 0 for none.
uint32_t class1009_target(uint32_t pos, uint32_t rot, float yaw_cone, float pitch_cone, float range);  // L11:0x00308670 L13:0x00302420 L17:0x002e1f50

}  // namespace rac1

#endif
