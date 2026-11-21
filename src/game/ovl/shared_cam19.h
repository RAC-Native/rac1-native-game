// Camera type 19 (a scripted fly-through along two of the level's paths: the camera on one, looking at the other or at a
// moby), shared by Orxon, Gemlik, Oltanis and Quartu: one port of the levels' copies (shared_cam19.cpp). Each function's
// `orig:` line lists every level's address. A camera's data: its record at *(0x15ef50) + 32 * camera+0x84, +0x1c.
#ifndef RAC1_GAME_OVL_SHARED_CAM19_H
#define RAC1_GAME_OVL_SHARED_CAM19_H

#include <cstdint>

namespace rac1 {

// 1 when the hero is in the camera's zone (its box, cylinder, sphere or polygon; none: its "on" flags), else 0.
int32_t cam_type19_zone(uint32_t cam);  // L10:0x002f5a78 L13:0x00316f80 L14:0x003141c8 L14:0x00314e20 L15:0x002f7958
// Whether the camera takes over from `other` (-1: never again, once it has run).
int32_t cam_type19_accept(uint32_t cam, uint32_t other);  // L10:0x002f65f8 L13:0x00317b00 L14:0x00314d48 L15:0x002f84d8
// The fly-through's start.
void cam_type19_init(uint32_t cam);  // L10:0x002f5b58 L13:0x00317060 L14:0x003142a8 L15:0x002f7a38
// The fly-through's frame.
void cam_type19_update(uint32_t cam);  // L10:0x002f5f18 L13:0x00317420 L14:0x00314668 L15:0x002f7df8

}  // namespace rac1

#endif
