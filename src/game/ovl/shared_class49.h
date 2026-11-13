// Class 49, a shot that flies straight and bursts into shards where it hits (Quartu, Kalebo III, Drek's fleet, Veldin), and
// its trail of sparks, which class 50 shares with a tuning record of its own: one port of the four levels' copies
// (shared_class49.cpp). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle
// batch 505.
#ifndef RAC1_GAME_OVL_SHARED_CLASS49_H
#define RAC1_GAME_OVL_SHARED_CLASS49_H

#include <cstdint>

namespace rac1 {

void class49_update(uint32_t m);  // L15:0x0029ac30 L16:0x002a05e8 L17:0x002a2b30 L18:0x002a76e0
// the trail's sparks, from the tuning record `tune` (class 49's, or class 50's on Kalebo III and Veldin)
void class49_trail(uint32_t m, uint32_t tune);  // L15:0x0029ae18 L16:0x002a07d0 L16:0x002a0f18 L17:0x002a2d18 L18:0x002a78c8 L18:0x002a8010
void class49_shards(uint32_t m);  // L15:0x0029af88 L16:0x002a0940 L17:0x002a2e88 L18:0x002a7a38
// a shot of `owner`'s from `pos` with the velocity `vel` (EE vectors); the shot or 0 (class 638 fires them)
uint32_t class49_spawn(uint32_t owner, uint32_t vel, uint32_t pos);  // L16:0x002a09a0 L18:0x002a7a98
// a shot of class 50 (a homing one: its target point `target`, its life `life` frames) of `owner`'s from `pos` at `vel`; the
// shot or 0 (class 1356 fires them)
uint32_t class50_spawn(uint32_t owner, uint32_t vel, uint32_t pos, uint32_t target, int32_t life);  // L16:0x002a0dc0 L18:0x002a7eb8
// class 49's tuning record on the level being run
uint32_t class49_tuning();

}  // namespace rac1

#endif
