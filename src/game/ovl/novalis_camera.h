// Camera type 3 (the path camera for slides and rails): reset, init, update and exit (novalis_camera.cpp). Novalis's and the
// same code on levels 5, 8, 16 and 17: one port for every level's copy.
#ifndef RAC1_GAME_OVL_NOVALIS_CAMERA_H
#define RAC1_GAME_OVL_NOVALIS_CAMERA_H

#include <cstdint>

namespace rac1 {

void cam_type3_reset(uint32_t cam);   // L1:0x00314e98 L5:0x00325340 L8:0x00311890 L16:0x002efe48 L17:0x002fc2c8
void cam_type3_init(uint32_t cam);    // L1:0x00315de0 L5:0x00326288 L8:0x003127d8 L16:0x002f0d90 L17:0x002fd210
void cam_type3_update(uint32_t cam);  // L1:0x00315358 L5:0x00325800 L8:0x00311d50 L16:0x002f0308 L17:0x002fc788
void cam_type3_exit(uint32_t cam);    // L1:0x00316030 L5:0x003264d8 L8:0x00312a28 L16:0x002f0fe0 L17:0x002fd460

}  // namespace rac1

#endif
