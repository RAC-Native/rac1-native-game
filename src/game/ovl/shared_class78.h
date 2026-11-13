// Class 78 of Quartu and Drek's fleet (a sleeping turret the alarms of class 408 wake) and two helpers of class 44's guards:
// the class-78 break and the shot of class 934 (shared_class78.cpp: one port of both levels' copies, the same code
// relinked). Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by oracle batch 516.
#ifndef RAC1_GAME_OVL_SHARED_CLASS78_H
#define RAC1_GAME_OVL_SHARED_CLASS78_H

#include <cstdint>

namespace rac1 {

// Class 78's update.
void class78_update(uint32_t m);  // L15:0x002a2bf0 L17:0x002a82c0
// The first sleeping (state 1) moby of the group `group` woken at `pos` (an EE vector) facing the hero; 1, or 0 if none.
uint32_t class78_wake(int32_t group, uint32_t pos);  // L15:0x002a2868 L17:0x002a7f38
// A moby broken: its flags of the level's done lists set, its sound stopped, state 2.
void class78_break(uint32_t m);  // L15:0x002a35d8 L17:0x002a8ca8
// A spark (particle 0x47) at `pos` moving by `vel`, its other direction `dir`, its colours `colour` / `colour2`, its three
// times, the pvars of `m` riding with it; the particle, or 0.
uint32_t particle_47_spawn(uint32_t pos, uint32_t vel, uint32_t dir, uint32_t colour, uint32_t colour2, uint32_t t1, uint32_t t2,
                           uint32_t t3, uint32_t m);  // L15:0x002647f0 L17:0x0026e978
// A shot (class 934) from `m` at `pos` (an EE vector) along `dir`; the shot, or 0.
uint32_t class934_shot_spawn(uint32_t m, uint32_t dir, uint32_t pos);  // L15:0x002e4580 L17:0x002e1e20

}  // namespace rac1

#endif
