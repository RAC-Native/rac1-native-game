// Class 1111, the open sea about the camera (Pokitaru, Kalebo III): two texture layers scrolled with the camera and by
// themselves, a slow swell, and the draw callback its update queues: a band out to the horizon, a curtain at the far edge
// and the two layers' tiles under the camera (shared_class1111.cpp). One port of the two levels' copies. Phase 1 of stage
// 4: the levels' generated copies still run; these are proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1111_H
#define RAC1_GAME_OVL_SHARED_CLASS1111_H

#include <cstdint>

namespace rac1 {

void class1111_update(uint32_t m);  // L11:0x0030b358 L16:0x002dee10
void class1111_draw(uint32_t m);    // L11:0x0030a908 L16:0x002de3c0

}  // namespace rac1

#endif
