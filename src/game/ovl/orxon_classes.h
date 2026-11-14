// Orxon (level 10): the level's own classes written by hand (orxon_platforms.cpp, orxon_doors.cpp, orxon_clank.cpp,
// orxon_hazards.cpp, orxon_props.cpp; their helpers in orxon_common.h). Registered in orxon_mobys.cpp; level 10's own
// addresses. The class updates keep the generated entries' names and signatures.
#ifndef RAC1_GAME_OVL_ORXON_CLASSES_H
#define RAC1_GAME_OVL_ORXON_CLASSES_H

#include <cstdint>

#include <game/math/vecmath.h>

namespace rac1 {

// orxon_platforms.cpp
void orx_class1033_update_l10(uint32_t m);  // L10:0x00295a38  a platform rising and sinking on a timer
void orx_class1117_update_l10(uint32_t m);  // L10:0x00295c20  a floating platform that sinks under the hero
// orxon_doors.cpp
void orx_class353_update_l10(uint32_t m);   // L10:0x002befe8  a gate that sinks once its flag is set
bool orx_2d9000_l10(uint32_t m, const Vec4& p);  // L10:0x002d9000  whether p is in class 1015's footprint
void orx_class1015_update_l10(uint32_t m);  // L10:0x002d90a8  a door its floor switch closes and opens
void orx_class1031_update_l10(uint32_t m);  // L10:0x002d92b8  the floor switch of Clank's part
// orxon_clank.cpp
void orx_class22_update_l10(uint32_t m);    // L10:0x00298b68  the hand-over to Clank
void orx_class351_update_l10(uint32_t m);   // L10:0x002be858  Clank's teleport pads
void orx_class18_update_l10(uint32_t m);    // L10:0x00298668  the O2 Mask on its stand
// orxon_hazards.cpp
void orx_class702_update_l10(uint32_t m);   // L10:0x002c7a20  a steam vent
uint32_t orx_2d8058_l10(uint32_t pipe, uint32_t pos, uint32_t vel);  // L10:0x002d8058  a blob (class 938) spawned
void orx_class939_update_l10(uint32_t m);   // L10:0x002d8c00  the pipe that spits blobs
uint32_t orx_2d8170_l10(uint32_t m, uint32_t pv, uint32_t from);  // L10:0x002d8170  a blob's bounce
void orx_class938_update_l10(uint32_t m);   // L10:0x002d85c8  the blob of sludge
uint32_t orx_2dd270_l10(uint32_t owner, uint32_t pos, uint32_t path, float dist, float speed, float leg);  // L10:0x002dd270  a spark (class 1090) shed
void orx_class947_update_l10(uint32_t m);   // L10:0x002d8d30  a spark running along its path
void orx_2d7b18_l10(uint32_t m, float z);   // L10:0x002d7b18  a shot's height eased toward z
void orx_class859_update_l10(uint32_t m);   // L10:0x002d7b80  a shot that keeps to a height
// orxon_props.cpp
void orx_2c9958_l10(uint32_t m, uint32_t pv);  // L10:0x002c9958  class 794's push of the mobys in its way
void orx_class794_update_l10(uint32_t m);   // L10:0x002c9c70  a box tied to another moby
void orx_class1047_update_l10(uint32_t m);  // L10:0x002d9eb8  a breakable wall
void orx_class1067_update_l10(uint32_t m);  // L10:0x002da2c8  the panel of shuffled lights

}  // namespace rac1

#endif
