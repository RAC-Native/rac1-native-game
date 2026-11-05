// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/custom_classes.cpp.)
// A custom planet's own moby classes (local experiment, branch feat/custom-levels-experiment): RAC2's classes the tools
// convert into the planet's core (tools/custom_level/rac2classes.py: Maktar's crowd, monitors, jointed arms, the floor's
// fittings) are RAC1 moby classes there, drawn and animated by RAC1's own engine (the VU1 path and the MRP). What they DO
// is ours: hand-written behaviour after RAC2's (its level code read as the reference, nothing of it copied), registered
// as their update functions.
//
// planet.txt names them, one line a class:
//   class <role> <RAC1 class id> <bank>
// role: crowd, monitor, arm, tile, lift, prop or enemy_<RAC2 class> (the behaviour: an enemy's is rac2_enemies.h's); bank: the index in "rac2_banks" of the sound bank their sound records
// play from (-1: no sounds), custom_planet.h.
//
// - The update function: a class of ours, registered while the planet is on (moby_class_register), gets an address of
//   our own in the class update table (0x197680), which the dispatch (ovl_register_moby_fn) sends to our function.
//   RAC1's class ids index a 2048-byte table, so the tools give them ids of their own (2032..), not RAC2's.
// - Their sounds: the class's sound records (+0x28, 32 bytes each) get RAC2's sound ids from the index (the game's own
//   class sound pass) and here the RAC2 bank's handle (+0x1c): the animation's sound events then play them as any
//   RAC1 class's.
// With no custom planet, or on any other level, nothing here does anything: the game is the same.
#ifndef RAC1_HOST_CUSTOM_CLASSES_H
#define RAC1_HOST_CUSTOM_CLASSES_H

#include <cstdint>

namespace rac1 {

// orig: (none) — a moby class registered (moby_class_register): the update function's address for class `cls` when it is
// one of the custom planet's (its behaviour registered for the dispatch), else 0 (the class table's own stays)
uint32_t custom_classes_update_fn(int32_t cls);

}  // namespace rac1

#endif
