// Class 1633 (the Sandmouse's shot) and class 1818 (the Sandmouse: its update, helpers and glow, its draw callback), shared
// by every level that has them: one port of the levels' copies (shared_sandmouse.cpp). Each function's `orig:` line lists
// every level's address. The update and its helpers are phase 1 of stage 4: the levels' generated copies still run; these
// are proved against them by oracle batch 514.
#ifndef RAC1_GAME_OVL_SHARED_SANDMOUSE_H
#define RAC1_GAME_OVL_SHARED_SANDMOUSE_H

#include <cstdint>

namespace rac1 {

// The shot's update.
void class1633_update(uint32_t m);  // L1:0x0030c9a8 L2:0x002efee0 L3:0x002e08c8 L4:0x002e5928 L5:0x0031d760 L6:0x003094f0 L8:0x00308920 L11:0x0031b758 L12:0x00309648 L14:0x00308be0
// The Sandmouse's two glows (its pvars' points +0x70 / +0x80), each through the level's own build of glow_draw.
void sandmouse_glow(uint32_t m);  // L1:0x0030de68 L2:0x002f0eb0 L3:0x002e1898 L4:0x002e68f8 L5:0x0031e730 L6:0x0030adb0 L8:0x0030ab50 L11:0x0031c728 L12:0x0030a618 L14:0x00309bb0

// The Sandmouse's update.
void sandmouse_update(uint32_t m);  // L1:0x0030df40 L2:0x002f0f88 L3:0x002e1970 L4:0x002e69d0 L5:0x0031e808 L6:0x0030ae88 L8:0x0030ac28 L11:0x0031c800 L12:0x0030a6f0 L14:0x00309c88
// Its step toward its goal (+0 of its pvars `pv`): turning, walking, pushed away from the hero, leaning.
void sandmouse_move(uint32_t m, uint32_t pv);  // L1:0x0030d880 L2:0x002f08c8 L3:0x002e12b0 L4:0x002e6310 L5:0x0031e148 L6:0x0030a7c8 L8:0x0030a568 L11:0x0031c140 L12:0x0030a030 L14:0x003095c8
// The best target from `from` for a shooter turned by `angles` (+4 its pitch, +8 its yaw): a moby of kind 5 alive (its
// pvars' first word above 0) within `range`, within the yaw and pitch reaches (or `near` and within the near reaches),
// seen; 0: none.
uint32_t sandmouse_find_target(uint32_t m, uint32_t from, uint32_t angles, float yaw_reach, float pitch_reach, float range,
                               float near, float near_yaw, float near_pitch);  // L1:0x0030d308 L2:0x002f0350 L3:0x002e0d38 L4:0x002e5d98 L5:0x0031dbd0 L6:0x0030a250 L8:0x00309ff0 L11:0x0031bbc8 L12:0x00309ab8 L14:0x00309050
// A shot at its target when its timer runs out.
void sandmouse_shoot(uint32_t m, uint32_t pv);  // L1:0x0030d5f0 L2:0x002f0638 L3:0x002e1020 L4:0x002e6080 L5:0x0031deb8 L6:0x0030a538 L8:0x0030a2d8 L11:0x0031beb0 L12:0x00309da0 L14:0x00309338
// A shot (class 1633) fired by `m` from `pos`: its aim (+8, +0xc of its pvars) and its pitch and yaw (+0x48, +0x44).
uint32_t class1633_spawn(uint32_t m, uint32_t pos, float aim8, float aimc, float pitch, float yaw);  // L1:0x0030c898 L2:0x002efdd0 L3:0x002e07b8 L4:0x002e5818 L5:0x0031d650 L6:0x003093e0 L8:0x00308810 L11:0x0031b648 L12:0x00309538 L14:0x00308ad0
// The sparks at its chain point 0 (three of particle 0x4a, the level's records).
void sandmouse_sparks(uint32_t m, uint32_t pv);  // L1:0x0030dda0 L2:0x002f0de8 L3:0x002e17d0 L4:0x002e6830 L5:0x0031e668 L6:0x0030ace8 L8:0x0030aa88 L11:0x0031c660 L12:0x0030a550 L14:0x00309ae8
// Its goal beside the hero: 1.1 ahead, 1 to the side, 1 up (into `pv`).
void sandmouse_goal_by_hero(uint32_t m, uint32_t pv);  // L1:0x0030d260 L2:0x002f02a8 L3:0x002e0c90 L4:0x002e5cf0 L5:0x0031db28 L6:0x0030a1a8 L8:0x00309f48 L11:0x0031bb20 L12:0x00309a10 L14:0x00308fa8
// The glows' points (chain points 3, 4 into the pvars +0x70 / +0x80) and its glow queued.
void sandmouse_glow_points(uint32_t m);  // L1:0x0030ded0 L2:0x002f0f18 L3:0x002e1900 L4:0x002e6960 L5:0x0031e798 L6:0x0030ae18 L8:0x0030abb8 L11:0x0031c790 L12:0x0030a680 L14:0x00309c18
// The point 1 ahead of the hero (into `out`; the first argument unused). Other classes' copies too.
void point_ahead_of_hero(uint32_t m, uint32_t out);  // L1:0x002e30b8 L1:0x0030d208 L2:0x002cbda0 L2:0x002f0250 L3:0x002e0c38 L4:0x002c6fc0 L4:0x002e5c98 L5:0x0031dad0 L6:0x0030a150 L8:0x00309ef0 L11:0x0031bac8 L12:0x003099b8 L14:0x00308f50

}  // namespace rac1

#endif
