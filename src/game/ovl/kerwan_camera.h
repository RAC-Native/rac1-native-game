// Kerwan (level 3) camera types 1, 14, and 18 (accept / init / update / exit) and follow-camera hook + callback.
#ifndef RAC1_GAME_OVL_KERWAN_CAMERA_H
#define RAC1_GAME_OVL_KERWAN_CAMERA_H

#include <cstdint>

namespace rac1 {

int32_t ker_cam_type1_accept_l3(uint32_t cam, uint32_t other);   // orig: L3:0x002e8870
void ker_cam_type1_init_l3(uint32_t cam);                        // orig: L3:0x002e8940
void ker_cam_type1_update_l3(uint32_t cam);                      // orig: L3:0x002e8960
void ker_cam_type1_exit_l3(uint32_t cam);                        // orig: L3:0x002e8980

int32_t ker_cam_type14_accept_l3(uint32_t cam, uint32_t other);  // orig: L3:0x002ebd70
void ker_cam_type14_init_l3(uint32_t cam);                       // orig: L3:0x002eb978
void ker_cam_type14_update_l3(uint32_t cam);                     // orig: L3:0x002ebdb0
void ker_cam_type14_exit_l3(uint32_t cam);                       // orig: L3:0x002ebde8

int32_t ker_cam_type18_accept_l3(uint32_t cam, uint32_t other);  // orig: L3:0x002ed5f8

void ker_cam0_hook_l3(uint32_t a0);                              // orig: L3:0x002eb408
void ker_cam_callback_l3(uint32_t a0);                           // orig: L3:0x002eb468

}  // namespace rac1

#endif
