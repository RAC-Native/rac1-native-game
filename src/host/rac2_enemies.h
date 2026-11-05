// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/rac2_enemy_*.cpp.)
// RAC2's arena enemies hand-ported (local experiment, branch feat/custom-levels-experiment; PLANET_PLAN.md P6): each one's
// class converted by the tools (tools/custom_level/maktar.py, rac2classes.py: mesh, animations, sounds, collision) and its
// behaviour written here by hand after RAC2's level code (read as the reference: its states, sequences, timings and
// numbers; nothing of it copied), on RAC1's engine through rac2_compat.h. The arena's controller (custom_arena.h) drops
// them through the gates in place of their stand-ins (planet.txt "port <RAC2 class> <RAC1 class>").
// With no custom planet nothing here is called: the game is the same.
#ifndef RAC1_HOST_RAC2_ENEMIES_H
#define RAC1_HOST_RAC2_ENEMIES_H

#include <cstdint>

namespace rac1 {

// orig: (none) — RAC2's 3134 (the arena's small enemy: its update 0x40be08), the update of its converted class
void rac2_enemy_3134_update(uint32_t m);
// orig: (none) — RAC2's 3131 (the arena's mid-sized enemy, the flamethrower walker: its update 0x40a4a8)
void rac2_enemy_3131_update(uint32_t m);
// orig: (none) — RAC2's 2850 (the arena's heavy enemy, the slammer: its update 0x3f0798)
void rac2_enemy_2850_update(uint32_t m);
// orig: (none) — RAC2's 2506 (the arena's gladiator, sword and shield: its update 0x3d0408)
void rac2_enemy_2506_update(uint32_t m);
// orig: (none) — RAC2's 2552, the boss Chainblade (its update 0x3d2de0)
void rac2_boss_2552_update(uint32_t m);
// orig: (none) — Chainblade's health now and most (0 / 0: none on the floor), for the arena's boss bar
void rac2_boss_2552_health(uint32_t m, float& now, float& most);
// orig: (none) — RAC2's 2628, the boss B2 Brawler (its update 0x3d7c20; its turret 2635 and shell 3336 its parts)
void rac2_boss_2628_update(uint32_t m);
// orig: (none) — the B2 Brawler's health now and most, for the arena's boss bar
void rac2_boss_2628_health(uint32_t m, float& now, float& most);

// ---- P7, the tag team (challenge 9: RAC2's controller 0x3fac00 activates both, pairs them, Chainblade first): each boss
// fights until a third more of its health is lost, then leaves the floor to its partner (Chainblade floats back to its
// entry point, the B2 Brawler jumps back onto its perch) and waits for its turn again ----
// orig: (none) — `m` (just spawned, before its first update) paired with `partner`; `waits`: it starts on its perch / at
// its entry point waiting for its turn (RAC2's 0x3d4980 / 0x3db870 and the activate's second argument)
void rac2_boss_2552_pair(uint32_t m, uint32_t partner, bool waits);
void rac2_boss_2628_pair(uint32_t m, uint32_t partner, bool waits);
// orig: (none) — its turn to fight (RAC2's 0x3d4990 / 0x3db880: the thirds of its health lost counted, its flag to come in)
void rac2_boss_2552_turn(uint32_t m);
void rac2_boss_2628_turn(uint32_t m);
// orig: (none) — still in the fight (RAC2's partner test: its state 3 or more)
bool rac2_boss_2552_fighting(uint32_t m);
bool rac2_boss_2628_fighting(uint32_t m);
// orig: (none) — the two above for a boss of either class (custom_arena.cpp: by its class)
void rac2_boss_turn(uint32_t m);
bool rac2_boss_fighting(uint32_t m);

}  // namespace rac1

#endif
