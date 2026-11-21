// Class 280 (Novalis, Eudora, Batalia), the pump the hero works with the Hydrodisplacer (item 8) in hand: one port of the
// levels' copies (shared_class280.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS280_H
#define RAC1_GAME_OVL_SHARED_CLASS280_H

#include <cstdint>

namespace rac1 {

// The update: set up, waiting for the hero, worked (the hero walked round it), released, sinking, done.
void class280_update(uint32_t m);  // L1:0x002e0c68 L4:0x002bfd30 L8:0x002d8270
// Novalis's two story flags (0x13d394 / 0x13d395) for its pumps 0x34 and 0x35 done (on the other levels nothing).
void class280_level_flags(uint32_t m);  // L1:0x002e0bd8 L4:0x002bfca0 L8:0x002d81e0
// The scripted camera's (the moby of uid 5, or the current camera's when it is one) yaw and pitch.
void camera5_angles_set(float yaw, float pitch);  // L1:0x00316e88 L4:0x002ee798 L8:0x00313880

}  // namespace rac1

#endif
