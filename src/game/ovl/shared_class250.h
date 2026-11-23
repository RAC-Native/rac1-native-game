// Class 250, a moby that plays its animations in step with the camera of type 20 (Oltanis, Quartu), shared by the two
// levels that have it: one port of the levels' copies of its update (shared_class250.cpp). Every level with a copy runs
// this port; it is proved against each level's own copy by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS250_H
#define RAC1_GAME_OVL_SHARED_CLASS250_H

#include <cstdint>

namespace rac1 {

void class250_update(uint32_t m);  // L14:0x002d96e0 L15:0x002c6f08

}  // namespace rac1

#endif
