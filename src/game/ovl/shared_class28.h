// Class 28, a turret that rides a path, wakes when another moby of its group calls it, turns its body and head toward the
// hero and fires rockets (class 403) from a firing position; when it dies it throws class 325's shells. One port of the
// copies Oltanis and Quartu carry (shared_class28.cpp; its two draw callbacks are shared_class28_draw.cpp). Phase 1 of
// stage 4: the levels' generated copies still run; these are proved against them by oracle batch 514.
#ifndef RAC1_GAME_OVL_SHARED_CLASS28_H
#define RAC1_GAME_OVL_SHARED_CLASS28_H

#include <cstdint>

namespace rac1 {

void class28_update(uint32_t m);  // L14:0x002b17d8 L15:0x002955c0
// The hits: the knock-back, the death fall, the colour pulse.
void class28_hits(uint32_t m);  // L14:0x002b2f60 L15:0x00296d48
// The glows' points, the sparks at the muzzle, the two draws queued.
void class28_visuals(uint32_t m);  // L14:0x002b2600 L15:0x002963e8
// A rocket (class 403) fired by `m` in `state` (0: along the level's path of `m`, at the speed `*speed` a second; else
// thrown up from the turret's place). The rocket (0: none).
uint32_t class28_spawn_rocket(uint32_t m, uint32_t speed, uint32_t state);  // L14:0x002e0170 L15:0x002cae58
// The body's turn toward `yaw` (and with `pitch_too` its tilt toward the target); the turn left to make.
float class28_aim(uint32_t m, int32_t pitch_too, float yaw);  // L14:0x002b35d8 L15:0x002973c0
// A burning shell (class 325) thrown from `pos` by `m`. The shell (0: none).
uint32_t class28_spawn_shell(uint32_t m, uint32_t pos);  // L14:0x002dea98 L15:0x002ca668
// One step along the turret's path; whether the path goes on.
int32_t class28_path_step(uint32_t m);  // L14:0x002b3418 L15:0x00297200
// The turret's path's segment lengths (and its path state reset).
void class28_path_lengths(uint32_t m);  // L14:0x002b3358 L15:0x00297140
// The head's turn toward `yaw`.
void class28_head_turn(uint32_t m, float yaw);  // L14:0x002b37b0 L15:0x00297598
// The target: the hero within the level's box and range (or out of range: the mode 2), else the hero's moby.
void class28_pick_target(uint32_t m);  // L14:0x002b3268 L15:0x00297050
// Every turret of the group `group` holds its fire for the level's pause.
void class28_group_pause(uint32_t group);  // L14:0x002b3890 L15:0x00297678
// Whether no turret of the moby's group is waking (state 2).
int32_t class28_group_settled(uint32_t m);  // L14:0x002b39a0 L15:0x00297788
// How many turrets of the group `group` are firing (states 5, 6).
int32_t class28_group_firing(uint32_t group);  // L14:0x002b3920 L15:0x00297708
// The range: the level's, 6 more while its timer runs (dealt anew when the pvar +0x38 asks).
void class28_range(uint32_t m);  // L14:0x002b2ed8 L15:0x00296cc0
// The moby of index `idx` called: a sleeping one (state 0) wakes (state 5); its timer set. Whether it was in state 6.
int32_t class28_call(uint32_t idx);  // L14:0x002d9870 L15:0x002c7098
// The drift: the pvars' velocity slowed by the level's drag and added to the place.
void class28_drift(uint32_t m);  // L14:0x002b3578 L15:0x00297360
// Hidden: not drawn, no target bounds.
void class28_hide(uint32_t m);  // L14:0x002b3318 L15:0x00297100

}  // namespace rac1

#endif
