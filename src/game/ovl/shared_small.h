// Small helpers several levels carry a copy of (shared_small.cpp: one port for every level's copy). Phase 1 of stage 4: the
// levels' generated copies still run; these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_SMALL_H
#define RAC1_GAME_OVL_SHARED_SMALL_H

#include <cstdint>

namespace rac1 {

// the level's movie `index` played (Ratchet lost instead when he has no ...+0x22a8) (L2:0x00298c68 L8:0x002a1880 L16:0x002923d0)
void level_movie_play(int32_t index);
// `point` (EE vector) turned about `origin` from the turn `from` to the turn `to` (Euler angles), into `out`
// (L7:0x00288968 L9:0x0027fe88)
void point_turned(uint32_t point, uint32_t out, uint32_t origin, uint32_t from, uint32_t to);
// the camera's flag +0x88 set (L8:0x00313ba8 L9:0x00312888 L12:0x00312f58)
void camera_flag_88();
// a push on `m` (sub_00259bc8) with no direction (the zero vector) (L10:0x0024c7f0 L13:0x00265d68)
void push_without_direction(uint32_t m, uint32_t a, uint32_t b, uint32_t pos, float strength);

}  // namespace rac1

#endif
