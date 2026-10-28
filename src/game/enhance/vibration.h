// Controller vibration (Options > Advanced > Vibration: Off, Low, High; #10). The original has none (it came with Going
// Commando): an enhancement in the spirit of Ratchet & Clank 2 and 3's. Not a port.
//
// The game's state is only read: once a gameplay frame (advanced_frame, 0x297f78) vibration_frame looks at what the
// game's own code did that frame and starts a pulse of the two motors for each event:
// - damage: the nanotech (+0x22a8) lower, or the hits taken (0x15eea8, hero_hurt 0x210ce8 counts them: the ship's shield
//   too) higher; the stronger the more nanotech lost;
// - Ratchet's death: the death states (0x3d, the ship's 0x5d) entered, or the lost-hero flag (+0x20b1: a pit, the water);
// - a hard landing: from the jump family's states to another, falling faster than 18 units/s the frame before;
// - explosions nearby: a new camera shake (0x166de0 its size, 0x166de8 its frames: the weapons' blasts, the mines, the
//   bombs set it when they go off in view, bigger the closer), as strong as the shake;
// - a weapon's shot: the ammo of the item in hand (0x13d428 + item * 4) lower; light ticks for the Blaster, the
//   Pyrocitor and the Tesla Claw, medium for the gloves and the others, heavy for the Devastator and the R.Y.N.O.;
// - the wrench's hit: the swing's "hit something" flag (+0xa64, wrench_hit_sparks 0x217368) set;
// - bolts picked up (0x15ed98 higher): a subtle tick, at most every 4 frames;
// - the ship (the hero in mode 2, +0x20a4: the space flights): a low hum while it flies.
// Reading the game's results, not hooking the functions that make them, covers every level's own build of the hero and
// weapon code (several levels run transliterated copies) with one reader, and cannot change the game.
//
// The host takes the motors once a game frame (vibration_take, before the next frame: 0 when no gameplay frame ran, so a
// menu or a pause stops them) and drives the pad (src/host/rumble.h).
#ifndef RAC1_GAME_ENHANCE_VIBRATION_H
#define RAC1_GAME_ENHANCE_VIBRATION_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: once a gameplay frame (advanced_frame): the events read and the motors' pulses advanced;
// `level` the setting (0 off: nothing read, the motors still)
void vibration_frame(uint32_t level);

// orig: (none) — enhancement: the host, once a game frame: the motors' strengths (0..1, `low` the big slow motor, `high`
// the small fast one) of the last gameplay frame; then 0 until another gameplay frame runs
void vibration_take(float& low, float& high);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_VIBRATION_H
