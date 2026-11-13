// The rocks (classes 696 / 697 / 698, and their builds on Orxon (1806) and Quartu (200)), shared by every level that has
// them: one port of the levels' copies (shared_rock.cpp). Each function's `orig:` line lists every level's address.
#ifndef RAC1_GAME_OVL_SHARED_ROCK_H
#define RAC1_GAME_OVL_SHARED_ROCK_H

#include <cstdint>

namespace rac1 {

// A rock of class `cls` at `pos` with the velocity `vel` (EE addresses): its life (frames) and fade mode, its scale and
// radius `scale`, gravity `gravity` (units of 9.8), spin `spin` (a random rate, either way) and the time before it collides
// `wait` (seconds). The moby, or 0.
uint32_t rock_spawn(uint32_t pos, uint32_t vel, uint32_t cls, int32_t life, uint32_t fade, float scale, float gravity, float spin,
                    float wait);  // L1:0x002f8530 L2:0x002dd4d8 L7:0x00300150 L10:0x002eafc8 L15:0x002be0b0 L18:0x002dfbc0
// A rock's update.
void rock_update(uint32_t m);  // L1:0x002f8718 L2:0x002dd6c0 L7:0x00300338 L10:0x002eb1b0 L15:0x002be298 L18:0x002dfda8

}  // namespace rac1

#endif
