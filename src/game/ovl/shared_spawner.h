// Classes 455 (the Spawner) and 545 (the pods it throws), shared by every level that has them (Kerwan, Batalia, Oltanis):
// one port of the levels' copies (shared_spawner.cpp). Each function's `orig:` line lists every level's address.
#ifndef RAC1_GAME_OVL_SHARED_SPAWNER_H
#define RAC1_GAME_OVL_SHARED_SPAWNER_H

#include <cstdint>

namespace rac1 {

// A random point inside the level's box `box` (its 0x80-byte record at *(0x1600ec): its matrix, its centre +0x30) into
// `out` (x, y, z); 1 (and nothing written) for no box (-1), else 0.
uint32_t class455_box_point(int32_t box, uint32_t out);  // L3:0x002beba0 L8:0x002e6168 L14:0x002e1758
// A pod (class 545) thrown by the Spawner `owner` from `pos` with the velocity `vel` (EE addresses); 0 when none could spawn.
uint32_t class455_pod_spawn(uint32_t owner, uint32_t pos, uint32_t vel, int32_t life, uint32_t flags);  // L3:0x002c45d8 L8:0x002ee5b0 L14:0x002e7198
// The Spawner's update.
void class455_update(uint32_t m);  // L3:0x002bef68 L8:0x002e6530 L14:0x002e1b20
// One of the Spawner `owner`'s dead mobys (of class `cls`, -1 any) brought back at `pos` (an EE address); its live ones
// counted again. The moby, or 0.
uint32_t class545_revive(uint32_t owner, uint32_t pos, int32_t cls);  // L3:0x002bec60 L8:0x002e6228 L14:0x002e1818
// The pod's update.
void class545_update(uint32_t m);  // L3:0x002c4718 L8:0x002ee6f0 L14:0x002e72d8

}  // namespace rac1

#endif
