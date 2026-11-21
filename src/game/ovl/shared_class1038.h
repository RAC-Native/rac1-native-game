// Classes 1038 and 1040 of Blarg, Orxon and Drek's fleet (a holder that spawns a floating crystal, class 1040, and glows
// toward the camera while it lives, and the crystal, which spins and bursts when hit), shared by the levels that carry them:
// one port of the levels' copies (shared_class1038.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1038_H
#define RAC1_GAME_OVL_SHARED_CLASS1038_H

#include <cstdint>

namespace rac1 {

// Class 1038's update.
void class1038_update(uint32_t m);  // L6:0x002f7288 L10:0x002d9550 L17:0x002e3690
// Its glow's start: four phases at random, their rates, timers and sizes.
void class1038_glow_init(uint32_t m);  // L6:0x002f7548 L10:0x002d9810 L17:0x002e3950
// Its glow: four sprites stepping from the crystal toward the camera, and a sparkle now and then.
void class1038_glow(uint32_t m);  // L6:0x002f7628 L10:0x002d98f0 L17:0x002e3a30
// Class 1040's update (the crystal).
void class1040_update(uint32_t m);  // L6:0x002f7ab8 L10:0x002d9bf8 L17:0x002e3d38
// The crystal's burst: forty sparks.
void class1040_burst(uint32_t m);  // L6:0x002f7c68 L10:0x002d9da8 L17:0x002e3ee8

}  // namespace rac1

#endif
