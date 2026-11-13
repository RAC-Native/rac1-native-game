// Class 123, a laser emitter that circles, swings or slides about its post and draws its beam (Quartu, Drek's fleet): one
// port of the two levels' copies of its update (shared_class123.cpp; its draw callback is shared_class123_draw.cpp's). Phase 1
// of stage 4: the levels' generated copies still run; this is proved against them by oracle batch 505.
#ifndef RAC1_GAME_OVL_SHARED_CLASS123_H
#define RAC1_GAME_OVL_SHARED_CLASS123_H

#include <cstdint>

namespace rac1 {

void class123_update(uint32_t m);  // L15:0x002a6e68 L17:0x002ac740

}  // namespace rac1

#endif
