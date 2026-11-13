// The shared code of Pokitaru's class 1319 and Drek's fleet's class 1843 (the same flyer): the vapour trails its group
// leaves (a record of the last points of two trails per moby and the draw callback that strips them), its pose along a
// path, and its reaction to a hit (shared_class1319.cpp). One port of the two levels' copies; the classes' own updates
// (L11:0x319838, L17:0x2f40d8) stay the levels'. Phase 1 of stage
// 4: the levels' generated copies still run; these are proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1319_H
#define RAC1_GAME_OVL_SHARED_CLASS1319_H

#include <cstdint>

namespace rac1 {

// The draw callback: every visible moby of the callback moby's group and class draws its two trails as strips of quads
// fading from white-blue to dark.
void class1319_trails_draw(uint32_t m);    // L11:0x003192c8 L17:0x002f3b68
// The trails' record: every fourth frame a new head (the record grows to the level's length); the two points behind the
// moby's wings written at the head; the draw queued once a frame for the group.
void class1319_trails_record(uint32_t m);  // L11:0x00319670 L17:0x002f3f10
// The moby along its path (pvar +0x60 the path, +0x64 the point, +0x68 the way to the next): its turn eased between the two
// segments' (turned round when +0x6c is positive), its place swung about the path by +0x78 at the angle +0x70, and moved
// there at most at +0x80 (seek_1d).
void class1319_path_pose(uint32_t m);      // L11:0x0031a438 L17:0x002f4c90
// A hit (mask 0x330000): counted in the game's records, on Pokitaru the third by class 0xac earns its reward once; the
// moby goes to its state 2 / 6 with its hit animation. Unhit, in its state 5: hidden unless the hero flies (state 0x32),
// and state 6 when it touches something.
void class1319_hit(uint32_t m);            // L11:0x0031a1c0 L17:0x002f4a18

}  // namespace rac1

#endif
