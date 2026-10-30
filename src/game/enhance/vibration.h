// Controller vibration (Options > Advanced > Vibration: Off, Low, High; Adaptive Triggers: Off, On; #10). The original
// has none (it came with Going Commando): an enhancement in the spirit of Ratchet & Clank 2 and 3's. Not a port.
//
// The game's state is only read: once a gameplay frame (advanced_frame, 0x297f78) vibration_frame looks at what the
// game's own code did that frame and starts a short pulse for each event (the table is in vibration.cpp and in
// docs/advanced.md): the strong low-frequency motor (left) for impacts and explosions, the weak high-frequency one
// (right) for shots and ticks, each pulse held for about half its length, then cut down to 0 within a few frames:
// - damage: the nanotech (+0x22a8) lower, or the hits taken (0x15eea8, hero_hurt 0x210ce8 counts them: the ship's shield
//   too) higher; the stronger and longer the more nanotech lost; felt from the attacker's side (+0x2280, kept by
//   hero_hurt);
// - Ratchet's death: the death states (0x3d, the ship's 0x5d) entered, or the lost-hero flag (+0x20b1: a pit, the water);
// - a hard landing: from the jump family's states to another, falling faster than 18 units/s the frame before;
// - explosions nearby: a new camera shake (0x166de0 its size, 0x166de8 its frames: the weapons' blasts, the mines, the
//   bombs set it when they go off in view, 0.4 - 0.0175 x the distance), as strong as the shake, felt from the side of
//   the sound the blast started that frame (the sound slots 0x13e5c0: the one nearest the shake's distance);
// - a weapon's shot: the ammo of the item in hand (0x13d428 + item * 4) lower; light ticks for the Blaster, the
//   Pyrocitor and the Tesla Claw, medium for the gloves and the others, heavy for the Devastator and the R.Y.N.O.;
// - the wrench's hit: the swing's "hit something" flag (+0xa64, wrench_hit_sparks 0x217368) set;
// - bolts picked up (0x15ed98 higher): a subtle tick, at most every 4 frames;
// and while they last:
// - the ship (the hero in mode 2, +0x20a4: the space flights): a low hum;
// - the thrown wrench (+0x10aa set, the wrench moby +0x1090): a light whir from its side;
// - a moving sound source (a sound slot playing for a second or more on a moby moving faster than 9 units/s within 20
//   units: the flying enemies' engines, the ships around): a low hum from its side, the nearest one.
// The side is the source's direction in the camera's view (its position 0x166dc0, its rows 0x166fd0: forward, left,
// up), what the player sees: a pan from -1 (left) to 1 (right), centred within a unit of the camera. On a DualSense the
// two actuators are a true left and right: a positional pulse is split between them with a constant-power pan (each
// side at most the pulse's strength, both at it in the centre). On the other pads the two motors are the low and the
// high frequencies, not left and right: there the pulses stay centred.
// With Adaptive Triggers on (a DualSense, Aiming = Modern: L2 aims, R2 fires), in gameplay on foot: L2 resists from a
// quarter of its travel; R2 by the item in hand: a light resistance that buzzes while firing for the rapid weapons, a
// stiff section that gives way (a click) for the others, a harder one for the Devastator and the R.Y.N.O., a short kick
// at each shot, nothing for the wrench and the gadgets; the light bar shows the health (green to red).
// Reading the game's results, not hooking the functions that make them, covers every level's own build of the hero and
// weapon code (several levels run transliterated copies) with one reader, and cannot change the game.
//
// The host takes the result once a game frame (vibration_take, before the next frame: neutral when no gameplay frame
// ran, so a menu, a pause or a level's loading stops the motors and gives the triggers and the light bar back) and
// drives the pad (src/host/rumble.h).
#ifndef RAC1_GAME_ENHANCE_VIBRATION_H
#define RAC1_GAME_ENHANCE_VIBRATION_H

#include <cstdint>

namespace rac1 {

struct VibeTrigger {
    uint8_t kind = 0;             // 0 off, 1 resistance (a position, b force), 2 section (a start, b end, c force),
    uint8_t a = 0, b = 0, c = 0;  // 3 vibration (a frequency in Hz, b amplitude, c position); 0..255 each
};

struct VibeOut {
    float low = 0.0f, high = 0.0f;    // the classic motors (the big slow one, the small fast one), 0..1
    float left = 0.0f, right = 0.0f;  // a DualSense's actuators, 0..1 (positional pulses panned)
    bool dualsense = false;           // Adaptive Triggers on, in gameplay: the triggers and the light bar below
    VibeTrigger r2, l2;
    float health = -1.0f;             // 0..1, below 0 the pad's own light bar
};

// orig: (none) — enhancement: once a gameplay frame (advanced_frame): the events read and the pulses advanced. `level`
// the Vibration setting (0 off: nothing read, the motors still), `triggers` the Adaptive Triggers one, `modern_aim`
// Aiming = Modern (L2 aims, R2 fires)
void vibration_frame(uint32_t level, uint32_t triggers, bool modern_aim);

// orig: (none) — enhancement: the host, once a game frame: the last gameplay frame's result; then neutral until another
// gameplay frame runs
void vibration_take(VibeOut& out);

// orig: (none) — enhancement: the host tells whether the pad in use is a DualSense (the Adaptive Triggers row is shown
// only then; Modern and Experimental turn it on only then)
void vibration_set_dualsense(bool present);
// orig: (none) — enhancement: what the host told
bool vibration_dualsense();

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_VIBRATION_H
