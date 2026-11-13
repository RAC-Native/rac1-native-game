// Class 44 of Quartu and Drek's fleet: a guard that walks its path, raises the alarms of class 408 when it sees the hero,
// shoots class 934's shots and calls its group (shared_class44.cpp: one port of both levels' copies, the same code relinked;
// its glows' draw is shared_three_glows.cpp, the alarms shared_class408.cpp, the shots shared_class78.cpp). Phase 1 of stage
// 4: the levels' generated copies still run; these are proved against them by oracle batch 516.
#ifndef RAC1_GAME_OVL_SHARED_CLASS44_H
#define RAC1_GAME_OVL_SHARED_CLASS44_H

#include <cstdint>

namespace rac1 {

// Class 44's update.
void class44_update(uint32_t m);  // L15:0x002979d8 L17:0x0029f8d8
// Its frame's common part: its hit reactions (knocked back, dying), its colour pulse; unless `quiet`, its view of the hero
// (in sight, the target picked along its path, its alert).
void class44_think(uint32_t m, uint32_t quiet);  // L15:0x0029a570 L17:0x002a2470
// Its head and body turned toward what it looks at (an ally, its target, its aim point).
void class44_aim(uint32_t m);  // L15:0x0029a2a8 L17:0x002a21a8
// The nearest ally (class 635 in states 6 .. 15) of its ally group in sight and in front of it (or any, once one is calling);
// 0 when none (or the nearest is past 8 units while none calls).
uint32_t class44_find_ally(uint32_t m);  // L15:0x00299dd8 L17:0x002a1cd8
// A turn to `angle`: a turning state (6 / 7 / 8, their animations) for a wide one, then state `state` with the animation
// `set`; 1 when it turns, else 0 (state and animation set at once).
uint32_t class44_turn_to(uint32_t m, uint32_t pv, uint32_t state, uint32_t set, float angle);  // L15:0x00299880 L17:0x002a1780
// The same toward the point `point` (an EE vector).
uint32_t class44_turn_toward(uint32_t m, uint32_t pv, uint32_t point, uint32_t state, uint32_t set);  // L15:0x00299a68 L17:0x002a1968
// A step toward `point` (an EE vector) at up to `speed`, turning; 1 once within 0.2 of it.
uint32_t class44_walk_to(uint32_t m, uint32_t point, float speed);  // L15:0x00299ae0 L17:0x002a19e0
// The guards of its group in sight of it alerted (or itself, ungrouped), and told to look at it.
void class44_alert_group(uint32_t m);  // L15:0x0029a0d0 L17:0x002a1fd0
// 1 when its alarm is down and no guard of its group is nearer to its own alarm than it is to its own (and none is in state
// 14), else 0.
uint32_t class44_group_calm(uint32_t m);  // L15:0x00299ca0 L17:0x002a1ba0
// 1 when no guard of its group, stopped on its path, is further along it, else 0.
uint32_t class44_path_first(uint32_t m);  // L15:0x00299ff8 L17:0x002a1ef8
// The guard `o` told to look at `m` (for three seconds; when it looks at another already, one time in sixteen).
void class44_look_at(uint32_t m, uint32_t o);  // L15:0x0029a248 L17:0x002a2148

}  // namespace rac1

#endif
