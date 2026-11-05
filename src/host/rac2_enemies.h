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
// orig: (none) — RAC2's 2722 (2850's spinning wheels, carried by it): gone with it
void rac2_part_2722_update(uint32_t m);

}  // namespace rac1

#endif
