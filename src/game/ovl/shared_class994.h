// Class 994 of Kerwan and class 1260 of Oltanis: the sea's controller (shared_class994.cpp: one port of both levels' copies,
// the same code relinked, the wave's alpha the one difference). Every level with a copy runs this port; it is proved against
// each level's own copy by oracle batch 519.
#ifndef RAC1_GAME_OVL_SHARED_CLASS994_H
#define RAC1_GAME_OVL_SHARED_CLASS994_H

#include <cstdint>

namespace rac1 {

// The controller's update: at its start the level's wave table scaled by 2/3 (the wave's alpha 0x20 on Kerwan, 0x40 on
// Oltanis), then the level's draw of the sea queued every frame.
void class994_update(uint32_t m);  // L3:0x002dc9b0 (class 994) L14:0x00303370 (class 1260)

}  // namespace rac1

#endif
