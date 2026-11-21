// Class 408 of Quartu and Drek's fleet: an alarm that class 44's guards raise, with the level's alarm sound and draw
// (shared_class408.cpp: one port of both levels' copies, the same code relinked). Phase 1 of stage 4: the levels' generated
// copies still run; these are proved against them by oracle batch 516.
#ifndef RAC1_GAME_OVL_SHARED_CLASS408_H
#define RAC1_GAME_OVL_SHARED_CLASS408_H

#include <cstdint>

namespace rac1 {

// Class 408's update.
void class408_update(uint32_t m);  // L15:0x002cb4c8 L17:0x002cbe80
// The alarm of the frame (once a frame, from the first class-408 moby updated): its level (0 .. 1) toward on while an alarm is
// raised, its sound and draw while it is above 0.
void class408_alarm(uint32_t m);  // L15:0x002cbc30 L17:0x002cc5e8
// The alarms of `m`'s group (or of `m` alone, ungrouped) raised (`frames` > 0, for that long) or lowered; -1 lowers them
// and the alarm's level at once.
void class408_raise(uint32_t m, int32_t frames);  // L15:0x002cbac0 L17:0x002cc478
// A class-408 moby woken (state 1 to 2, its animation 1), and its alarm's time set when it is lowered in state 3.
void class408_wake(uint32_t m);  // L15:0x002cbba0 L17:0x002cc558

}  // namespace rac1

#endif
