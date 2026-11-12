// 1.02 Fixes: fixes of the game's later build, 1.02 (the Japanese disc and the PAL disc 2.00, the "Platinum"), ported onto
// the North American build's code as an option (main menu > Options > Advanced > 1.02 Fixes, off by default; a global
// setting, kept in the preferences only). Each fix is the 1.02 build's own change to a function the port has, translated
// from that build's code (its addresses taken back to the North American ones: data/regions/SCES-50916-2.00.txt), at the
// place it made it; with the row off the North American port runs, byte for byte the same. The fixes that change how the
// game plays are a row of their own, 1.02 Gameplay (gameplay102_on, RAC1_GAMEPLAY102; below).
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
// - 15: the cutscenes: none started while one runs (game mode 2), the underwater view (0x167014) put aside for the scene
//   and back at its end (FIX_UNDERWATER), the frame's draw lists emptied at its end (cutscene_start 0x298840,
//   cutscene_end 0x298b18)
// - 17: the ship's landing / take-off scene skipped (and its flight hurried) with circle too, not only triangle / cross
//   (ship_sequence_update 0x290590: the presses' mask 0x50 -> 0x70)
// - 18: Aridia's sand shark, killed, bursts (enemy_death_burst at its place) before it dives off to its next spot
//   (L2:0x2d3e50; tools/codegen/hooks.glue)
// - 19: the hoverbike races' texts from the level's text table, in the game's language and with a singular: the place
//   ("1st".. "5th"), the poses ("x1 Pose") and the flips ("+ 360 Flip") in place of English built into the code
//   (Rilgar L5:0x262f50 / 0x266320, Kalebo III L16:0x235660 / 0x238a30; fixes102_race_text; hooks.glue)
// - 20: Blarg's class 1083 breaking 32 units or more from Ratchet leaves no debris (its 50 pieces: moby slots)
//   (L6:0x2ffcb0 / 0x300278; hooks.glue)
// - 22: Rilgar's race organiser (class 918, L5:0x316ab8): the level's music back after a race ended in its state 3, and a
//   skill point earned at the race told (its flag 0x13d411: "You just earned a Skill Point!" over the prize's message)
//   (hooks.glue)
//
// 1.02 Gameplay (main menu > Options > Advanced > 1.02 Gameplay, off by default, global; docs/advanced.md "1.02 Gameplay"):
// the 1.02 build's fixes that change how the game plays (the exploits stay possible with the row off), numbered as the
// analysis's for RAC1_GAMEPLAY102:
// - 4: the RYNO's targets from the target list 0x1aba00 (fixes102_ryno_targets, in place of ryno_2cc360 0x2cc360)
// - 8: the Visibomb's steering ends in the hero state 0x32 too (visibomb_update 0x2b3058, visibomb_ball_update 0x2b6140:
//   visibomb.cpp, visibomb_ball.cpp)
// - 9: deadly ground (+0x12ea) for the Hologuise's states, the hoverboard's ride-off, quicksand in a vehicle mode
//   (hero_input_update 0x229b70, hero_set_state 0x2223f8, hero_surface_update 0x20b960 and their levels' copies:
//   the hand ports and tools/codegen/hooks.glue)
// - 10: the characters' talk guards (gameplay_frame 0x297f78; five levels' classes in tools/codegen/hooks.glue)
// - 12: the tilt to the ground stiffer and its speed capped (sub_002163f0 0x2163f0)
// - 13: the Hologuise's use test (sub_0020fe30 0x20fe30), its countdown stopped by hero_place 0x216f90, the state a forced
//   hero_set_state leaves it with
// - 14: the Decoy Glove's decoy pushed off by Ratchet swimming into it (state 0x31 within 1.5: decoy_object_update
//   0x2c4380: decoy_obj.cpp)
// - 21: Rilgar's moving water (class 831, L5:0x30ca80): its surface carries Ratchet swimming in it as it rises (his
//   water height +0x2f0), and it holds still in the cutscenes (hooks.glue)
// In the generated code each hook is a test that fails with the row off; on another region's disc the transliterated
// functions that region changed (stand-ins: on the PAL disc 2.00 the 1.02 build's own) get none.
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

// orig: (none) — enhancement: the 1.02 Gameplay fix number `fix` is on now: RAC1_GAMEPLAY102=<n>,<n>... (testing: "all",
// "none" or empty) over the 1.02 Gameplay row; false while every enhancement is off
bool gameplay102_on(int fix);

// orig: (none) — enhancement: fix 3's ring of the last 20 Glove of Doom agents (20 words, then the next slot's index)
constexpr uint32_t FIX_RING = 0x000ffc00u;
constexpr uint32_t FIX_RING_NEXT = FIX_RING + 20 * 4;
// orig: (none) — enhancement: fix 15's underwater view (0x167014) kept over a cutscene (the 1.02 build: the scene's +0x4b)
constexpr uint32_t FIX_UNDERWATER = FIX_RING_NEXT + 4;

// orig: (none) — enhancement: fix 15: a cutscene's start (cutscene_start, after its `scene < 0` test): true when the
// 1.02 build returns (a cutscene already runs: game mode 2); false with the fix off
bool fixes102_cutscene_refused();
// orig: (none) — enhancement: fix 15: the cutscene's state set up: the underwater view put aside and cleared; nothing with
// the fix off
void fixes102_cutscene_start();
// orig: (none) — enhancement: fix 15: the cutscene's end, after its fade: the frame's draw lists emptied
// (draw_lists_reset) and the underwater view put back; nothing with the fix off
void fixes102_cutscene_end();

// orig: (none) — enhancement: fix 18: Aridia's sand shark `m` killed: its death burst (three sparks of 0.333, the shards,
// no blast) 0.333 over its place, as the 1.02 build's L2:0x2d68d0; nothing with the fix off
void fixes102_sand_shark_burst(uint32_t m);

// orig: (none) — enhancement: fix 19: a hoverbike race's text `kind` (0 the poses "x%d %s", 1 the flips "%s + %d %s",
// 2 the place "%s") written into `buf` as the 1.02 build writes it (Rilgar's L5:0x2638f8 / 0x266d10 in the PAL disc 2.00):
// `name` the first %s (the flips' trick name), `n` the count (the place 1..), the word from the level's text table (the
// poses 20855 / 20856, the flips 20647 / 20648 at 360 / not, the places 21028 + n - 1, 5th the last); false with the fix
// off (the original's sprintf then runs)
bool fixes102_race_text(uint32_t buf, int kind, uint32_t name, int32_t n);

// orig: (none) — enhancement: fix 20: Blarg's class 1083 breaks far from Ratchet (L6:0x2ffd94, its call of 0x300278 with
// the 1.02 build's argument 0): `far` set over that call; fixes102_blarg_far() read by 0x300278 before its debris loop
void fixes102_blarg_far_set(bool far);
bool fixes102_blarg_far();

// orig: (none) — enhancement: fix 14 (1.02 Gameplay): the decoy `m` in its state of 0x2c4c08 pushed (an event 0x10000 of
// strength 1000 from Ratchet's place) when Ratchet swims (state 0x31) within 1.5 of it; nothing with the fix off
void gameplay102_decoy_push(uint32_t m);

// orig: (none) — enhancement: fix 21 (1.02 Gameplay): Rilgar's class 831 (the water) `m`: true when the 1.02 build skips
// its update (a cutscene: game mode 2); false with the fix off
bool gameplay102_water_held();
// orig: (none) — enhancement: fix 21 (1.02 Gameplay): the water `m` moved toward its height `target`: while more than 0.1
// from it, Ratchet swimming (+0x208c 0x11 / 0x12) below its surface (+0x2f0) gets its height; nothing with the fix off
void gameplay102_water_carry(uint32_t m, float target);

// orig: (none) — enhancement: a level's load (gameplay_load): fix 3's ring cleared, as the 1.02 build's gameplay_load
// does
void fixes102_level_load();

// orig: (none) — enhancement: fix 5: the Thruster-Pack's sound at a jump's frame 5 (the 1.02 build's
// hero_vertical_velocity / hero_horizontal_velocity); nothing with the fix off
void fixes102_thruster_jump_sound();

// orig: (none) — enhancement: fix 10 (1.02 Gameplay): a character's "talk" prompt (Pokitaru's, Gemlik's, Drek's fleet's
// mobys that take the triangle) not offered while the hero is already in the state 0x32 or 0x1d, or in a vehicle mode
// (+0x20a4); false with the fix off (the level's code then runs as transliterated; tools/codegen/hooks.glue)
bool gameplay102_talk_blocked();
// orig: (none) — enhancement: fix 10 (1.02 Gameplay): Rilgar's and Kalebo III's talking characters not updated
// (talk_update) while the hero is in the state 0x1d; false with the fix off
bool gameplay102_talk_held();
// orig: (none) — enhancement: fix 10 (1.02 Gameplay): those two characters lock the hero's input (+0x20ac) as they
// start their scene; nothing with the fix off
void gameplay102_talk_lock();

// orig: (none) — enhancement: fix 9 (1.02 Gameplay): the hero on deadly ground (+0x12ea: collision kinds 8 / 0xc), for
// the Hologuise's states (0x53..0x59) as the original has it for the others' (0x52 / 0x79: lost after 300 frames):
// - gameplay102_deadly_ground: on it (the 1.02 build's state 0x53 test)
// - gameplay102_deadly_ground_near: on it within `h` of the ground (+0x2dc: 0.3 in the state 0x59, 0.2 in 0x54)
// - gameplay102_deadly_landing: entering 0x53 / 0x54 (`sub` the sub-state the entry sets) on it, not in the air
//   (+0x30c 0): the sub-state and +0x2284 set as the entry sets them, then the state 0x58 (hero_set_state(0x58, 1));
//   true when it did, the entry then returns 0
// - gameplay102_board_ride_off: the hoverboard's ready stance (0x3f) rides off (2) only on its ground contact (+0x300),
//   out of deep water (+0x12e7) and tilted less than pi/4 (+0x90 / +0x94), the original's test of the height
//   (+0x30e / +0x2dc) gone; 0 fix off, 1 ride off, 2 stay
// - gameplay102_quicksand_lost: quicksand (+0x12eb) in a vehicle mode (+0x20a4 1 or 2) makes the hero lost
//   (hero_lost) instead of sinking him (0x7b); true when it did
// Each false / 0 / nothing with the fix off.
bool gameplay102_deadly_ground();
bool gameplay102_deadly_ground_near(float h);
bool gameplay102_deadly_landing(uint32_t sub);
int gameplay102_board_ride_off();
bool gameplay102_quicksand_lost();

// orig: (none) — enhancement: fix 13 (1.02 Gameplay): hero_set_state's entry into `state` out of the Hologuise's mode
// (hero_mode3_exit, which sets a state of its own) stores `state` again as the hero's state; nothing with the fix off
void gameplay102_mode3_left(uint32_t state);

// orig: (none) — enhancement: fix 4 (1.02 Gameplay): the 1.02 build's RYNO target search, run in place of the North
// American ryno_2cc360 (its arguments: the shot's start, its aim angles +4 / +8, the 7 mobys to skip or 0, the yaw and
// pitch tolerances, the range); the best moby or 0
int32_t fixes102_ryno_targets(uint32_t from, uint32_t aim, uint32_t skip, float yaw_tol, float pitch_tol, float range);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_FIXES102_H
