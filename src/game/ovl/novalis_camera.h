// Novalis (level 1) camera type 3 (path camera for slides and rails): init, update, and exit.
#ifndef RAC1_GAME_OVL_NOVALIS_CAMERA_H
#define RAC1_GAME_OVL_NOVALIS_CAMERA_H

#include <cstdint>

namespace rac1 {

void nov_cam_type3_init_l1(uint32_t cam);    // orig: L1:0x00315de0
void nov_cam_type3_update_l1(uint32_t cam);  // orig: L1:0x00315358
void nov_cam_type3_exit_l1(uint32_t cam);    // orig: L1:0x00316030

}  // namespace rac1

#endif
