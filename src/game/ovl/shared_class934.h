// Class 934 of Quartu and Drek's fleet: a spinning shot (shared_class934.cpp: one port of both levels' copies, the same code
// relinked). Phase 1 of stage 4: the levels' generated copies still run; this is proved against them by oracle batch 519.
#ifndef RAC1_GAME_OVL_SHARED_CLASS934_H
#define RAC1_GAME_OVL_SHARED_CLASS934_H

#include <cstdint>

namespace rac1 {

// Class 934's update: the shot flies by its velocity, spinning, and bursts on what it meets (or when its time is out and it
// met a moby); a spark trails it every third frame.
void class934_update(uint32_t m);  // L15:0x002e4380 L17:0x002e1c20

}  // namespace rac1

#endif
