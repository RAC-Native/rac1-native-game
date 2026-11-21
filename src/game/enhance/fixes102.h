// 1.02 Fixes: fixes of the game's later build, 1.02 (the Japanese disc and the PAL disc 2.00, the "Platinum"), ported onto
// the North American build's code as an option (main menu > Options > Advanced > 1.02 Fixes, on by default; a global
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
// - 20: Blarg's class 1083 breaking 32 units or more from Ratchet without the moby it breaks for (its pvar +0x20 -1) leaves
//   no debris (its 50 pieces: moby slots) (L6:0x2ffcb0 / 0x300278: blarg_tank.cpp)
// - 22: Rilgar's race organiser (class 918, L5:0x316ab8): the level's music back after a race ended in its state 3, and a
//   skill point earned at the race told (its flag 0x13d411: "You just earned a Skill Point!" over the prize's message)
//   (hooks.glue)
// - 26: the Vendor's purchase screens: the message box shows the case's own text ("Back" for an ammo full or the bolts
//   short, "Buy" for ammo, none for a weapon the bolts pay) in place of "Buy" each time, and no "No" prompt for a weapon,
//   whose quantity box already asks Yes / No (vendor_draw.cpp: panel_message 0x29e6c0, panel_prompt 0x29e918)
// - 27: the end menu's Timewarp (cross) keeps the cheats unlocked (0x15edc0, 12 bytes) and switched on (0x15edb0, 12) over
//   the save's read, as it already keeps 0x13d425 and 0x14bf08 (end_menu_choice_update_tr 0x27fed8)
// - 28: the weapons / gadgets help's icon list packed tighter: 0x252 apart (not 0x260), its first 0x52 / 0x182 down (not
//   0x60 / 0x190) (icon_list_update 0x277a58, icon_list_draw 0x277be8)
// - 29: the glow of the hero's item mode 1 (+0x20a4: Clank's) a seventh smaller: 0.06 / 0.087 (not 0.07 / 0.1015)
//   (hero_glows_draw 0x207d40)
// - 30: an item panel's focus out frees its second menu moby (+0x48) too; the original frees the first one twice
//   (menu_model_focus_out 0x27bba0)
// - 32: Blarg's Snagglebeast (class 1051, L6:0x2f9a28): once the countdown of its fight's start runs, the fight starts
//   even if Ratchet has meanwhile stepped onto a pump (+0xb4 2) or its target changed; the North American build tests them
//   each frame of the countdown, and holds the start (blarg_snagglebeast.cpp)
// - 33: Umbris's Blargian Snagglebeast (class 1106, L7:0x314150, and its walk step L7:0x313af0): the walk's flags tested as
//   bits — its walk ends once bit 4 (goal reached) is set even while another walk flag is set, and bit 2 pulls it back onto
//   its path even with other flags set; the North American build tests `== 4` and `== 2` (umbris_snagglebeast.cpp)
// - 34: Batalia's unlinked gate (class 467, L8:0x2ea398, pvar +0x8 negative): once the level flag 0x13d3b6 is set, retires
//   the live mobys of classes 0x24c..0x256 and 0x1d8 in its moby group (+0x21) and itself (batalia_fort.cpp)
// - 35: Pokitaru's sewer water (class 1158, L11:0x30e088): sets the underwater colour bytes (0x161200..0x161206) back to the
//   sewer's (0, 0x28, 0x30, 0x40) and (0, 0x28, 0x30) while the camera is at or below 180 (0x1677c8 <= 180), pairing with the
//   1.02 open sea (class 1111) above 180 (pokitaru_sewer.cpp)
// - 36: Pokitaru's fighter jet (class 1242, flight step L11:0x312838): drops the square-button camera toggle (+0x60) and its
//   second camera offset, keeping the chase camera (pokitaru_jet.cpp)
// - 37: the Vendor's hologram moby (class 11, vendor_update_tr 0x2a57a8, state 0): only sets the hologram moby's byte
//   +0x73 to 0x20 when its class's byte +6 is non-zero; the North American build writes 0x20 unconditionally
//   (veldin_shared_classes.cpp)
// - 38: Gemlik Base's wall turrets' tops (class 36, spawned by class 29's L13:0x2b4958): +0x10 of the top's hit record (its
//   pvars +0x30) 0.7 instead of 0.9; its use in play not understood (the Morph-o-Ray's beam copies a target's +0x10;
//   other readers not traced) (gemlik_turret.cpp)
// - 39: Drek's fleet's pod (class 1448, L17:0x2f1940): counts its frames at rest (pvars +0xb0, cleared as it is set up and
//   as it arrives) and offers its prompt, and takes the triangle, only past frame_scale_i(30) of them; the North American
//   build offers it at once (veldin_orbit_pod.cpp)
// - 43: Gemlik Base's breakable towers (class 170, L13:0x2cefc0, state 0): the hit proxy each spawns (class 1632, by
//   L13:0x30bfb0) is scaled 2.8 instead of 2.7 of its class's scale and gets a hit record (its pvars +0 pointing at +0x20)
//   and the moby flag 0x20 (+0x34), as the 1.02 build does (gemlik_tower.cpp)
// - 44: Gemlik Base's beam robot (class 1262, L13:0x307e98, state 1): starts its beam's run-up (state 2) only while its
//   target is nearer than 15.0 as well as beyond L13:0x161e74 and in front; from 15 on it takes state 7 (the walk toward
//   the target), as when it does not face it; the North American build throws the beam at any distance and its state 3
//   then falls back to state 1 from 15 on (gemlik_robot.cpp)
// - 49: Oltanis's gondola (class 685, L14:0x2ee8d8, state 1): its ride starts only while its up row (+0xe0) and the
//   hero's moby's point the same way (their dot product positive), as well as the hero standing on it within 1 of its
//   centre; the North American build tests only the latter (oltanis_gondola.cpp)
//
// 1.02 Gameplay (main menu > Options > Advanced > 1.02 Gameplay, on by default, global; docs/advanced.md "1.02 Gameplay"):
// the 1.02 build's fixes that change how the game plays (the exploits stay possible with the row off), numbered as the
// analysis's for RAC1_GAMEPLAY102:
// - 4: the RYNO's targets from the target list 0x1aba00 (fixes102_ryno_targets, in place of ryno_2cc360 0x2cc360)
// - 8: the Visibomb's steering ends in the hero state 0x32 too (visibomb_update 0x2b3058, visibomb_ball_update 0x2b6140:
//   visibomb.cpp, visibomb_ball.cpp)
// - 9: deadly ground (+0x12ea) for the Hologuise's states, the hoverboard's ride-off, quicksand in a vehicle mode
//   (hero_input_update 0x229b70, hero_set_state 0x2223f8, hero_surface_update 0x20b960 and their levels' copies:
//   the hand ports and tools/codegen/hooks.glue)
// - 10: the characters' talk guards (gameplay_frame 0x297f78; pokitaru_jet.cpp and four levels' classes in tools/codegen/hooks.glue)
// - 12: the tilt to the ground stiffer and its speed capped (sub_002163f0 0x2163f0)
// - 13: the Hologuise's use test (sub_0020fe30 0x20fe30), its countdown stopped by hero_place 0x216f90, the state a forced
//   hero_set_state leaves it with
// - 14: the Decoy Glove's decoy pushed off by Ratchet swimming into it (state 0x31 within 1.5: decoy_object_update
//   0x2c4380: decoy_obj.cpp)
// - 21: Rilgar's moving water (class 831, L5:0x30ca80): its surface carries Ratchet swimming in it as it rises (his
//   water height +0x2f0), and it holds still in the cutscenes (hooks.glue)
// - 23: Giant Clank's pad (Quartu's class 1451 L15:0x2ed068, Veldin's finale's 1899 L18:0x2fb868; the shared port
//   clank_pad_update, shared_clank_pad.cpp): boarding retires a moby still linked at its pvar +0x70, and on Quartu saves the
//   pad's place as the checkpoint (checkpoint_save 0x284e50) before Ratchet is put on it
// - 24: the open sea of Pokitaru and Kalebo III (class 1111, L11:0x30b358 / L16:0x2dee10; the shared port class1111_update,
//   shared_class1111.cpp) is the level's water disc (0x1612dc: on, centre (512, 512), radius 1024, at the swell's height,
//   set each frame the sea is drawn), so that surface_height gives the hero's movement, the bombs and the mines its surface
// - 25: Gemlik Base's class 1885 (and Gaspar's copy; L9:0x30a9e8 / L13:0x30d3b8, the shared port class1885_hit,
//   shared_class1885.cpp) killed with Ratchet at the height 248 (within 4) on Gemlik Base, and Gemlik Base's class 63
//   (gml_class63_update_l13 L13:0x2b50d8, gemlik_trooper.cpp) killed with Ratchet at that height: their bolts with
//   moby_drop_bolts' flag 0x200 (their flag 2, as when Ratchet stands next to it)
// - 48: Gemlik Base's enemies under a heavy frame load (either of the frame's load factors 0x15f5d0 / 0x15f5d4): a class-63
//   robot of a wave (pvar +0x20a 2) not drawn this frame, whose group has 3 or more live members, retired one frame in 29
//   while a factor is above 0.98 (gml_class63_update_l13 L13:0x2b50d8, gemlik_trooper.cpp); a blob thrower (class 1885,
//   Gemlik's and Gaspar's, class1885_update) with blobs out throws no more while a factor is 0.85 or more
// - 31: the Tesla Claw's hits give their direction the z 1.0 (the North American build 0, unless the hit is straight
//   below the claw): the arc's hit and the two held targets' (tesla_2b94d0 0x2b94d0: tesla.cpp)
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
// no blast) 0.333 over its place, as the 1.02 build's L2:0x2d68d0; nothing with the fix off, except on the PAL disc 2.00
// (the 1.02 build itself: region_core_moved()), whose own shark bursts
void fixes102_sand_shark_burst(uint32_t m);

// orig: (none) — enhancement: fix 19: a hoverbike race's text `kind` (0 the poses "x%d %s", 1 the flips "%s + %d %s",
// 2 the place "%s") written into `buf` as the 1.02 build writes it (Rilgar's L5:0x2638f8 / 0x266d10 in the PAL disc 2.00):
// `name` the first %s (the flips' trick name), `n` the count (the place 1..), the word from the level's text table (the
// poses 20855 / 20856, the flips 20647 / 20648 at 360 / not, the places 21028 + n - 1, 5th the last); false with the fix
// off (the original's sprintf then runs), but on a PAL build's disc, whose own texts these are (region: written there
// whatever the fix's row)
bool fixes102_race_text(uint32_t buf, int kind, uint32_t name, int32_t n);

// orig: (none) — enhancement: fix 20: Blarg's class 1083 breaking far from Ratchet (L6:0x2ffd94): whether its break
// (0x300278) throws its debris: the 1.02 build's argument (L6:0x3011c4 on the PAL disc 2.00: the moby it breaks for, `link`,
// not -1); always with the fix off, but on the PAL disc 2.00 (the 1.02 build: region_core_moved()), whose own code it is
bool fixes102_blarg_far_debris(int32_t link);

// orig: (none) — enhancement: fix 32: Blarg's Snagglebeast's start (L6:0x2f9a28, its state 1): true when the 1.02 build's
// order applies (the pump and target tests only before the countdown starts: L6:0x2fb0b8 on the PAL disc 2.00); false with
// the fix off, but on the PAL disc 2.00 (region_core_moved()), whose own code it is
bool fixes102_snagglebeast_start();

// orig: (none) — enhancement: fix 33: Umbris's Snagglebeast's walk (L7:0x314150 at 0x315a94 and L7:0x313af0 at 0x313b70):
// true when the 1.02 build's bit tests apply (`(flags & 4) != 0` and `(flags & 2) != 0`: L7:0x316e6c / 0x314f50 on the PAL
// 2.00 disc); false with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_umbris_snagglebeast_walk();

// orig: (none) — enhancement: fix 34: Batalia's class 467 (L8:0x2ea398): true when the 1.02 build's group retirement
// applies (L8:0x2eb770 on the PAL 2.00 disc: with 0x13d3b6 set and its link +0x8 negative, retires classes 0x24c..0x256 and
// 0x1d8 of its group +0x21 and itself); false with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own
// code it is
bool fixes102_batalia_gate_group();

// orig: (none) — enhancement: fix 35: Pokitaru's class 1158 (L11:0x30e088): true when the 1.02 build's sewer underwater
// colour reset applies (L11:0x30f4f8 on the PAL 2.00 disc: while 0x1677c8 <= 180, sets 0x161200..0x161206 back to the
// sewer's bytes); false with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_pokitaru_sewer_tint();

// orig: (none) — enhancement: fix 36: Pokitaru's fighter jet flight step (L11:0x312838): true when the 1.02 build's chase
// camera applies (L11:0x313d00 on the PAL 2.00 disc: no square-button toggle of pvar +0x60 or second camera offset); false
// with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_pokitaru_jet_camera();

// orig: (none) — enhancement: fix 37: the Vendor's hologram moby (vendor_update_tr 0x2a57a8, state 0): true when the 1.02
// build's guard applies (L0:0x2a6b80 on the PAL 2.00 disc: only sets holo +0x73 to 0x20 when its class's byte +6 is
// non-zero); false with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_vendor_holo_alpha();

// orig: (none) — enhancement: fix 38: Gemlik Base's turret top (class 36, spawned by L13:0x2b4958): true when the 1.02
// build's value 0.7 (not 0.9) goes to +0x10 of the top's hit record (L13:0x2b5b08 on the PAL 2.00 disc); false with the
// fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_gemlik_turret_aim();
// orig: (none) — enhancement: fix 43: Gemlik Base's breakable tower (class 170, L13:0x2cefc0) and its hit proxy (class
// 1632, spawned by L13:0x30bfb0): true when the 1.02 build's proxy applies (L13:0x2d0298 / 0x30d468 on the PAL 2.00 disc:
// its scale 2.8 of its class's, not 2.7; its pvars +0 = their +0x20, a hit record; its +0x34 | 0x20); false with the fix
// off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_gemlik_tower_proxy();
// orig: (none) — enhancement: fix 44: Gemlik Base's beam robot (class 1262, gml_class1262_update_l13 L13:0x307e98,
// state 1): true when the 1.02 build's reach applies (L13:0x309544 .. 0x3095c4 on the PAL 2.00 disc: the beam's run-up,
// state 2, only while the target is nearer than 15.0, state 7 otherwise); false with the fix off, except on the PAL 2.00
// disc (region_core_moved()), whose own code it is
bool fixes102_gemlik_robot_reach();
// orig: (none) — enhancement: fix 49: Oltanis's gondola (class 685, olt_class685_update_l14 L14:0x2ee8d8, state 1): true
// when the 1.02 build's test applies (L14:0x2f0050 on the PAL 2.00 disc: the ride starts only while the gondola's up row
// +0xe0 and the hero's moby's agree, their dot product positive); false with the fix off, except on the PAL 2.00 disc
// (region_core_moved()), whose own code it is
bool fixes102_oltanis_gondola_upright();
// orig: (none) — enhancement: fix 39: Drek's fleet's pod (class 1448, vo_class1448_update_l17 L17:0x2f1940): true when
// the 1.02 build's wait applies (L17:0x2f3450 on the PAL 2.00 disc): the pod counts its frames at rest (+0xb0, cleared as it
// is set up and as it arrives) and offers its prompt, and takes the triangle, only past frame_scale_i(30) of them; false
// with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_drek_pod_wait();
// orig: (none) — enhancement: fix 45: Veldin's finale's rafts (class 1381, vf_class1381_update_l18 L18:0x2f16f0, state 2):
// true when the 1.02 build's test of the hero's ride applies (L18:0x2f2ae0 on the PAL 2.00 disc): the raft counts the
// frames he stands on one of its platforms (+0x350) and takes him for a rider only past frame_scale_i(30) of them; held
// also on a sinking raft of its class (state 4); its wait before it sinks once he is off is frame_scale_i(5), not 30;
// false with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_finale_raft_ride();
// orig: (none) — enhancement: fix 46: Veldin's finale's water (class 1402, vf_class1402_update_l18 L18:0x2f2310, state 2):
// true when the 1.02 build's carry applies (L18:0x2f3710 on the PAL 2.00 disc, after the water's seek toward its height:
// while it is more than 0.1 from it, Ratchet swimming (+0x208c 0x11 / 0x12) below its surface (+0x2f0) gets its height,
// as fix 21 does on Rilgar); false with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool fixes102_finale_water_carry();

// orig: (none) — enhancement: fix 14 (1.02 Gameplay): the decoy `m` in its state of 0x2c4c08 pushed (an event 0x10000 of
// strength 1000 from Ratchet's place) when Ratchet swims (state 0x31) within 1.5 of it; nothing with the fix off
void gameplay102_decoy_push(uint32_t m);

// orig: (none) — enhancement: fix 25 (1.02 Gameplay): the bolts' flag 0x200 for a killed Gemlik class-63 robot or blob
// thrower with Ratchet at the height 248 (class1885_hit, gml_class63_update_l13; the PAL 2.00 disc's L13:0x2b6638 and
// 0x30e91c): true when the 1.02 build's test applies; false with the fix off, except on the PAL 2.00 disc
// (region_core_moved()), whose own code it is
bool gameplay102_gemlik_bolts_flag();
// orig: (none) — enhancement: fix 48 (1.02 Gameplay): the 1.02 build's thinning of Gemlik Base's enemies under a heavy
// frame load (gml_class63_update_l13's end, the PAL 2.00 disc's L13:0x2b8af8; class1885_update's throw, 2.00 L13:0x30ec28):
// true when it applies; false with the fix off, except on the PAL 2.00 disc (region_core_moved()), whose own code it is
bool gameplay102_gemlik_load_thinning();
// orig: (none) — enhancement: fix 21 (1.02 Gameplay): Rilgar's class 831 (the water) `m`: true when the 1.02 build skips
// its update (a cutscene: game mode 2); false with the fix off, except on the PAL 2.00 disc (whose own code does it)
bool gameplay102_water_held();
// orig: (none) — enhancement: fix 21 (1.02 Gameplay): the water `m` moved toward its height `target`: while more than 0.1
// from it, Ratchet swimming (+0x208c 0x11 / 0x12) below its surface (+0x2f0) gets its height; nothing with the fix off,
// except on the PAL 2.00 disc (whose own code does it)
void gameplay102_water_carry(uint32_t m, float target);

// orig: (none) — enhancement: a level's load (gameplay_load): fix 3's ring cleared, as the 1.02 build's gameplay_load
// does
void fixes102_level_load();

// orig: (none) — enhancement: fix 5: the Thruster-Pack's sound at a jump's frame 5 (the 1.02 build's
// hero_vertical_velocity / hero_horizontal_velocity); nothing with the fix off
void fixes102_thruster_jump_sound();

// orig: (none) — enhancement: fix 10 (1.02 Gameplay): a character's "talk" prompt (Pokitaru's, Gemlik's, Drek's fleet's
// mobys that take the triangle) not offered while the hero is already in the state 0x32 or 0x1d, or in a vehicle mode
// (+0x20a4); false with the fix off, except on the PAL 2.00 disc (whose own build of those classes does it)
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
