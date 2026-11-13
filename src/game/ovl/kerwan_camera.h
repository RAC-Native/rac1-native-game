// Kerwan's (level 3) camera types 14 and 18, and the camera code several levels carry a copy of (kerwan_camera.cpp, one port
// for every level's copy; the levels' addresses are on the definitions): camera type 1 (Kerwan's, Eudora's), the follow
// camera's look-height hook and its callback, the callback queue, the follow camera's distance rates, the turn toward a
// direction.
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

int32_t ker_cam_type18_accept_l3(uint32_t cam, uint32_t other);  // L3:0x002ed5f8

// the follow camera's look-height hook and its end-of-update callback (L3:0x002eb408 / L3:0x002eb468, L2:0x002fa710 /
// L2:0x002fa770 ... : every level's copy is on the definitions)
void cam0_level_hook(uint32_t a0);
void cam0_level_callback(uint32_t a0);
// the camera's end-of-update callback queue (L3:0x001e4a88 L2:0x001fc008 ...)
void cam_callback_queue(uint32_t fn);
// the follow camera's distance rates (L3:0x002e6f80 L2:0x002f6598 L6:0x003106b8 L7:0x00323ff8 L8:0x00310218 L12:0x00310818
// L15:0x002f28b8 L18:0x00301280)
void cam0_set_20c(float a, float b, float g);
// `from` turned toward `to` by a share of the angle (L3:0x0024e6f8 L4:0x00252ce0)
void slerp_vec_toward(Vec4& out, const Vec4& from, const Vec4& to, int32_t unit_flag, float frac);

}  // namespace rac1

#endif
