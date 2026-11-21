// Drek's fleet's (level 17) own helpers ported by hand (the class updates keep their declarations in
// veldin_orbit_mobys.h): natural signatures, level 17 only.
#ifndef RAC1_GAME_OVL_VELDIN_ORBIT_OWN_H
#define RAC1_GAME_OVL_VELDIN_ORBIT_OWN_H

#include <cstdint>

namespace rac1 {

// veldin_orbit_beams.cpp
uint32_t vo_26fb60_l17(uint32_t m, uint32_t pos, uint32_t vel);  // L17:0x0026fb60 (a spark: particle 0x4f)

// veldin_orbit_cannon.cpp
void vo_2cbc10_l17(uint32_t m);                                                // L17:0x002cbc10 (class 347's hits)
void vo_2cbd38_l17(uint32_t m);                                                // L17:0x002cbd38 (class 347's fighters on their path)
uint32_t vo_2e94d8_l17(uint32_t m, uint32_t from, uint32_t to, float speed);  // L17:0x002e94d8 (class 1368's spawn)
uint32_t vo_2f4fb8_l17(int32_t group);                                         // L17:0x002f4fb8 (a fighter of the group launched)
uint32_t vo_26fdd0_l17(uint32_t pos, uint32_t kind, int32_t fade);            // L17:0x0026fdd0 (a laser glow: particle 0x50)

// veldin_orbit_ship.cpp (class 1379, the hero's ship)
void vo_253a90_l17(int32_t cls, int32_t hidden, int32_t solid, int32_t glow);  // L17:0x00253a90 (a class's mobys shown / solid / glowing)
void vo_2ec000_l17(uint32_t m, uint32_t pv, float yaw, float pitch);          // L17:0x002ec000 (the ship's guns and sight)
void vo_2ec150_l17(uint32_t m, uint32_t pv);                                   // L17:0x002ec150 (the ship's missiles)
void vo_2edf10_l17(uint32_t m, uint32_t yaw_in, uint32_t pitch_in);           // L17:0x002edf10 (the arena's edge on the stick)
void vo_2ec360_l17(uint32_t m, uint32_t pv);                                   // L17:0x002ec360 (the ship's flight)

// veldin_orbit_lift.cpp (class 1380, a lift)
void vo_2ee9c0_l17(uint32_t m, uint32_t pos, uint32_t ang);                   // L17:0x002ee9c0 (the camera beside the lift)
void vo_2eea70_l17(uint32_t m);                                                // L17:0x002eea70 (the hero's path on it)

}  // namespace rac1

#endif
