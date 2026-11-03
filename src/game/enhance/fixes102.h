// 1.02 Fixes: fixes of the game's later build, 1.02 (the Japanese disc and the PAL disc 2.00, the "Platinum"), ported onto
// the North American build's code as an option (main menu > Options > Advanced > 1.02 Fixes, off by default; a global
// setting, kept in the preferences only). Each fix is the 1.02 build's own change to a function the port has, translated
// from that build's code (its addresses taken back to the North American ones: data/regions/SCES-50916-2.00.txt), at the
// place it made it; with the row off the North American port runs, byte for byte the same. The fixes that change how the
// game plays (the RYNO's targets, the Visibomb's steering, the deadly ground, the interaction guards...) are left to a
// row of their own, later.
//
// The fixes (their numbers for RAC1_FIXES102, docs/advanced.md "1.02 Fixes"):
// - 1: New Game keeps the sound settings (new_game_state 0x24b628: effects 0x15edf0, music 0x15edec, mono 0x15ede8 kept
//   over the disc's default save)
// - 2: explosions leave moby slots free (bomb_update 0x2adb30: no debris while fewer than 51 / 21 slots are free;
//   explosion_effect 0x25e450: its debris mobys at most the free slots less 10)
// - 3: at most 20 Glove of Doom agents (doom_agent_spawn 0x2c8218: a ring of the last 20, the 21st sends the oldest one
//   still alive to state 4; the ring, FIX_RING, cleared at each level's load)
// - 5: the Thruster-Pack's jump sound (hero_vertical_velocity 0x214108 in a jump, hero_horizontal_velocity 0x214658 in
//   the long / wall jumps: at the state's frame 5 with the Thruster-Pack on the back, hero_sound_start(1, 0); not on
//   Oltanis)
// - 6: an animation's next frame kept in its sequence (hero_anim_set 0x2325a0: +0x51 at most the frame count - 1)
// - 7: Ratchet's colour pulse in the cutscenes (hero_ship_update 0x234b38 pulses the cutscene's Ratchet; hero_gadget_glow
//   0x2080b0 only in play, with the second colour pair while 0x179410 is set and not 8; on Veldin's scene frames 835 to
//   1080 the second pair too)
// - 11: the talk prompt only once in range (talk_update 0x266448: talk_step after the distance and facing checks)
// - 16: the pause menu: its goals updated when a page comes back to the goals (menu_page_start 0x276368), its open
//   counter (MS+0x110) held at 32000 (menu_update 0x276bd0)
//
// The EE block FIX (0xffc00..0x100000, in the PS2 kernel's first megabyte past corrected_texts.cpp's block: nothing of
// the game or the runtime lives there) holds the new state fix 3 needs. The oracle and the headless runs have the row
// off (g_advanced_disabled), unless a headless run keeps the settings (RAC1_ADVANCED=1).
#ifndef RAC1_GAME_ENHANCE_FIXES102_H
#define RAC1_GAME_ENHANCE_FIXES102_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: the 1.02 fix number `fix` is on now: RAC1_FIXES102=<n>,<n>... (testing: "all", "none" or
// empty) over the 1.02 Fixes row; false while every enhancement is off
bool fixes102_on(int fix);

// orig: (none) — enhancement: fix 3's ring of the last 20 Glove of Doom agents (20 words, then the next slot's index)
constexpr uint32_t FIX_RING = 0x000ffc00u;
constexpr uint32_t FIX_RING_NEXT = FIX_RING + 20 * 4;

// orig: (none) — enhancement: a level's load (gameplay_load): fix 3's ring cleared, as the 1.02 build's gameplay_load
// does
void fixes102_level_load();

// orig: (none) — enhancement: fix 5: the Thruster-Pack's sound at a jump's frame 5 (the 1.02 build's
// hero_vertical_velocity / hero_horizontal_velocity); nothing with the fix off
void fixes102_thruster_jump_sound();

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_FIXES102_H
