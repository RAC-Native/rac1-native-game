// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/custom_arena.cpp.)
// Maktar's arena controller (local experiment, branch feat/custom-levels-experiment; PLANET_PLAN.md P4): RAC2's challenge
// controller (its class 2905, a 12-state machine) written again for RAC1's engine by behaviour, its level code read as the
// reference, nothing of it copied. It replaces step 4c's three-wave hook.
//
// The planet's folder gives it (tools/custom_level/maktar.py, from the player's RAC2 disc, never committed):
//   challenges.txt: "challenge <index> <name text id> <the challenge to win first> <prize> <name>" then its waves,
//     "wave <c0..c9>": c0..c3 how many of RAC2's enemies 2506, 2850, 3131, 3134 (its controller's order, read in its code),
//     c4 / c5 hazards 2563 / 2953, c6 / c7 crates and items, c8 the bosses (1 Chainblade, 256 the B2 Brawler), c9 the
//     gates open (low byte);
//   hud.txt: the HUD's shapes and texts (custom_arena_hud.h);
//   planet.txt: "standin <RAC1 class> <template uid> <pvars size>" (a hidden template the arena copies),
//     "stand_for <RAC2 class> <RAC1 class>", "announce <event> <entries>", the music (custom_planet.h).
//
// What it does: standing at the arena's centre, the prompt; Triangle opens "Select Your Challenge" (Left / Right: the
// challenges, their victories and prize; Cross: start; Triangle: leave). A challenge: the challenge music and the
// announcer, then its waves: each wave's enemies (RAC1 stand-ins of RAC2's four, copies of the templates) queued and handed
// out by the gates (the jointed arms, custom_classes.cpp: lower, drop up to three, rise), as RAC2's controller hands its
// recycled enemies to the arms; the next wave once a wave's enemies are all down. Won: the prize in bolts (RAC2's
// prize x 0.3^victories, at least 500), the victory counted, the planet's music back. The time challenges' limit, and
// the Wrench Expert's rules (the wrench only, no hit), lose it. A death reloads the level: the arena from its start.
// Left out for now (listed in the menu with the reason): the boss challenges and waves (the bosses: P7), the challenges
// needing RAC2 weapons (the Lancer's, the Mini-Nuke's).
// The victories are kept in the save's .pc (custom_planet.h: our item 0x201).
// (testing) RAC1_ARENA_SCRIPT=<frame>:<challenge>[,...]: that challenge started at that level frame, as from the menu;
// RAC1_WAVES_AUTOKILL=<frames>: each enemy killed that many frames after it drops (a headless run through a challenge).
// Nothing happens without a custom planet, or on any other level: the game is the same.
#ifndef RAC1_HOST_CUSTOM_ARENA_H
#define RAC1_HOST_CUSTOM_ARENA_H

#include <cstdint>
#include <vector>

namespace rac1 {

// orig: (none) — once a gameplay frame (gameplay_frame, at its start): the controller stepped, its menu's input read and
// taken from the game
void custom_arena_frame();

// orig: (none) — the HUD's draw (hud_draw, after the elements): the prompt, the menu, the fight's panels, the result, in
// RAC2's layout (custom_arena_hud.h)
void custom_arena_hud();

// orig: (none) — the camera's update (camera_update, the view taken from the active camera): while one of the arena's path
// cameras runs (RAC2's type 19, custom_arena.cpp) the view's position `pos` and rows `rows` made its; false (nothing
// written) otherwise, and always without a custom planet
bool custom_arena_camera(uint32_t pos, uint32_t rows);

// The arena's crowd follows the controller (custom_classes.cpp):
enum class ArenaMood { Waiting, Fighting, Cheering };
// orig: (none)
ArenaMood custom_arena_mood();

// The gates (the jointed arms, custom_classes.cpp) hand the enemies out:
// orig: (none) — an arm set up: its gate's number (0 ..), in the order they set up
int custom_arena_gate_register(uint32_t m);
// orig: (none) — whether gate `gate` (raised, waiting) should lower and drop
bool custom_arena_gate_wanted(int gate);
// orig: (none) — gate `gate` drops now (its moby `m`): its enemies spawned below it
void custom_arena_gate_drop(int gate, uint32_t m);

// The hazards and items (P5): the floor's tiles (RAC2's 2563) turn over while their wave wants them (column c4: how
// many, the first ones to set up), the lifts (RAC2's 3276) bring the wave's crates up (columns c6 / c7, planet.txt's
// "items"); the vents (RAC2's 2953, code only: planet.txt's "vent" lines) are the controller's own (column c5).
// orig: (none) — a tile set up: its number
int custom_arena_tile_register(uint32_t m);
// orig: (none) — whether tile `tile` should be turned over (its hazard side up)
bool custom_arena_tile_on(int tile);
// orig: (none) — the tiles' damage (planet.txt's tile_damage)
int32_t custom_arena_tile_damage();
// orig: (none) — a lift set up: its number
int custom_arena_lift_register(uint32_t m);
// orig: (none) — a crate for lift `lift` (idle, down) to bring up now: the crate spawned on it (at `at`), 0 none
uint32_t custom_arena_lift_crate(int lift, float x, float y, float z);

// The victories in the save's .pc (custom_planet.cpp's items):
// orig: (none) — our item appended to `d` (id, size, bytes); its count
uint32_t custom_arena_pc_items(std::vector<uint8_t>& d);
// orig: (none) — item `id` read: true when it is ours
bool custom_arena_pc_item(uint32_t id, const uint8_t* p, uint32_t size);
// orig: (none) — a new game: no victories
void custom_arena_new_game();

}  // namespace rac1

#endif
