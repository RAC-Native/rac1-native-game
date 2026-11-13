// The hoverboard races' camera (camera type 8 on Rilgar and Kalebo III: the same code at each level's own address), one port
// of both levels' copies (shared_race_camera.cpp). Phase 1 of stage 4: the levels' transliterated copies still run (their
// hooks: rilgar_hooks.cpp, kalebo_hooks.cpp); these are proved against them by oracle batch 503.
//
// The camera record `cam`: its basis rows +0x00 / +0x10 / +0x20, position +0x30, rotation +0x40, the orbit's angles and
// distance +0x50 / +0x54 / +0x58, the point it came from +0x64, its block +0x70, its entry +0x84 in the level's camera table
// (*(0x15ef50), 0x20 bytes each: +0x1c the parameters). The block: the position springs' speeds +0x00, their tuning +0x10..
// +0x18, the orbit flag +0x1c, the board's point +0x80 (its height +0x88), the way up +0xa0, the orbit's axis +0xb0, the
// lift +0xcc, the distance +0xe0 / height +0xf0, the sideways lean +0x140 and its spring +0x120.., the orbit's springs +0x150..,
// the targets +0x180..+0x194, the drift +0x1b0 (its speeds +0x1c0), the board's next point +0x1d0.
#ifndef RAC1_GAME_OVL_SHARED_RACE_CAMERA_H
#define RAC1_GAME_OVL_SHARED_RACE_CAMERA_H

#include <cstdint>

namespace rac1 {

// The camera's start: the basis and position of the camera *(0x166e04), its rotation, then race_cam_setup.
void race_cam_init(uint32_t cam);  // L5:0x0032a6e0 L16:0x002f51e8
// A frame: the board's point (race_cam_anchor), then the follow (race_cam_follow).
void race_cam_update(uint32_t cam);  // L5:0x0032a758 L16:0x002f5260
// The block set from the entry's parameters, the springs at rest, then race_cam_params and race_cam_basis.
void race_cam_setup(uint32_t cam);  // L5:0x003296e0 L16:0x002f41e8
// The targets of the current camera *(0x166e00)'s block (+0x184 / +0x188 / +0x180 / +0x190 / +0x194 / +0x18c) and its spring
// tuning (`k` below 0: 0.01, `damp` below 0: 0.2).
void race_cam_params(float dist, float height, float lift, float boost_dist, float boost_height, float boost_lift, float k,
                     float damp);  // L5:0x003294c0 L16:0x002f3fc8
// The camera put behind the board's point at once: its position and basis.
void race_cam_basis(uint32_t cam);  // L5:0x00329540 L16:0x002f4048
// The board's point (the hero's, 0.7 up his axes) and the way up; on the levels' two tracks the point's height kept above a floor.
void race_cam_anchor(uint32_t cam);  // L5:0x00329358 L16:0x002f3e60
// The orbit about the board's point while the hero's +0x88e flag is set: its angles and distance measured once, then sprung
// back to their rests (pi, -1.536, 5) and the position set from them.
void race_cam_orbit(uint32_t cam);  // L5:0x003298d8 L16:0x002f43e0
// The follow: the position sprung toward its place behind the point (the ground under the way there pushing the height up), the
// distances eased toward their targets (the boost's while the board goes fast), then the orbit, the basis and the lean.
void race_cam_follow(uint32_t cam);  // L5:0x00329ce8 L16:0x002f47f0

}  // namespace rac1

#endif
