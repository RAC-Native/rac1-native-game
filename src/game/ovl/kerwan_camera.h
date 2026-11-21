// Kerwan's (level 3) camera type 14, and the camera code several levels carry a copy of (kerwan_camera.cpp, one port for
// every level's copy; the levels' addresses are on the definitions): camera type 1 (Kerwan's, Eudora's), camera type 18
// (levels 2, 3, 6, 7, 8, 12, 18), the follow camera's look-height hook and its callback, the callback queue, the follow
// camera's distance rates and word +0x218, the turn toward a direction.
#ifndef RAC1_GAME_OVL_KERWAN_CAMERA_H
#define RAC1_GAME_OVL_KERWAN_CAMERA_H

#include <cstdint>

#include <game/math/vecmath.h>

namespace rac1 {

int32_t cam_type1_accept(uint32_t cam, uint32_t other);  // L3:0x002e8870 L4:0x002ed850
void cam_type1_init(uint32_t cam);                       // L3:0x002e8940 L4:0x002ed920 (and their callees L3:0x002e8628 L4:0x002ed608)
void cam_type1_update(uint32_t cam);                     // L3:0x002e8960 L4:0x002ed940 (and their callees L3:0x002e8730 L4:0x002ed710)
void cam_type1_exit(uint32_t cam);                       // L3:0x002e8980 L4:0x002ed960

int32_t ker_cam_type14_accept_l3(uint32_t cam, uint32_t other);  // L3:0x002ebd70
void ker_cam_type14_init_l3(uint32_t cam);                       // L3:0x002eb978
void ker_cam_type14_update_l3(uint32_t cam);                     // L3:0x002ebdb0
void ker_cam_type14_exit_l3(uint32_t cam);                       // L3:0x002ebde8

// camera type 18 (0x12): the accept (where the modifier's frame runs), the frame, the moby's cone and the hero's zone tests
// (L3:0x002ed5f8 / 0x002ecd28 / 0x002ecae8 / 0x002ec9a8, L2:0x002fc298 / 0x002fb9c8 / 0x002fb788 / 0x002fb648 ... : every
// level's copy is on the definitions)
int32_t cam_type18_accept(uint32_t cam, uint32_t other);
void cam_type18_update(uint32_t cam);
int32_t cam_type18_cone_check(uint32_t cam, float max_yaw_deg, float max_pitch_deg);
int32_t cam_type18_inside(uint32_t cam);

// the follow camera's look-height hook and its end-of-update callback (L3:0x002eb408 / L3:0x002eb468, L2:0x002fa710 /
// L2:0x002fa770 ... : every level's copy is on the definitions)
void cam0_level_hook(uint32_t a0);
void cam0_level_callback(uint32_t a0);
// the camera's end-of-update callback queue (L3:0x001e4a88 L2:0x001fc008 ...)
void cam_callback_queue(uint32_t fn);
// the follow camera's distance rates (L3:0x002e6f80 L2:0x002f6598 L6:0x003106b8 L7:0x00323ff8 L8:0x00310218 L12:0x00310818
// L15:0x002f28b8 L18:0x00301280)
void cam0_set_20c(float a, float b, float g);
// the follow camera's word +0x218 (L3:0x002e6f58 L2:0x002f6570 L6:0x00310690 L7:0x00323fd0 L8:0x003101f0 L12:0x003107f0
// L18:0x00301258)
void cam0_set_218(uint32_t v);
// `from` turned toward `to` by a share of the angle (L3:0x0024e6f8 L4:0x00252ce0)
void slerp_vec_toward(Vec4& out, const Vec4& from, const Vec4& to, int32_t unit_flag, float frac);

}  // namespace rac1

#endif
