// Cut content: things the retail game has the data for but never shows, brought back as an option (main menu > Options >
// Advanced > Cut Content, off by default; a global setting, kept in the preferences only). The game's own code for
// each item is a stub: its behaviour is reconstructed (`// orig: (none)`), or ported from an earlier build that still has
// it (`// orig: beta 2002-08 ...`), in functions chosen where the game dispatches to the original's, so that with the
// option off the faithful port runs, byte for byte the same.
// The items, and the candidates left out: docs/advanced.md ("Cut Content") and docs/port-inventory.md.
//
// Items (their names for RAC1_CUT_CONTENT):
// - fred: Kerwan's citizens "Fred 1 / 2 / 3" (classes 915, 916, 917; 14 placements on level 3). Their update (L3:0x2dc310)
//   measures the distance and the angle to Ratchet, throws both away and retires the moby on its first frame: with the
//   item on they stay, play an idle (Nebula G34's citizen's, class 1105, the same skeleton: their own sequence is one
//   frame, the rest pose with the arms out) and turn to face Ratchet when he comes near. They have no collision.
// - ships: Eudora's Transport Ships (class 484, 3 placements on level 4). Their update (L4:0x2ce060) is moby_retire; the
//   beta (2002-08) and prototype (2002-06) builds keep the real one, ported here: hidden until Ratchet enters its box,
//   the ship flies its path with the enemies it carries (Circular Saw Enemies 340, Blarg Troopers 427), drops them one
//   by one at its drop point and leaves; two of the three start a short scripted camera on their arrival.
//
// The option is read when a level's mobys first update (a citizen retired then is gone until the level is loaded
// again; one kept stays), never written to a save. The oracle and the headless runs have it off (g_advanced_disabled),
// unless a headless run keeps the settings (RAC1_ADVANCED=1).
#ifndef RAC1_GAME_ENHANCE_CUT_CONTENT_H
#define RAC1_GAME_ENHANCE_CUT_CONTENT_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: the cut content item `item` ("fred", "ships") is restored now: RAC1_CUT_CONTENT=<item>,<item>...
// (testing; "none" or empty: none) over the Cut Content row; false while every enhancement is off
bool cut_content_on(const char* item);

// orig: (none) — enhancement: the update registered at L3:0x2dc310 (classes 915 / 916 / 917): the restored citizen
// (cut_content_fred_update_l3) with the item on, or a citizen already kept by it; else the faithful fred_update_l3
void cut_content_fred_dispatch_l3(uint32_t m);
// orig: (none) — cut content restored: a Kerwan citizen alive, its idle sequence looped, turned to face Ratchet near it
void cut_content_fred_update_l3(uint32_t m);

// orig: (none) — enhancement: the update registered at L4:0x2ce060 (class 484): the restored ship
// (cut_content_ships_update_l4) with the item on, or a ship already kept by it; else the faithful eud_class484_update_l4
void cut_content_ships_dispatch_l4(uint32_t m);
// orig: beta 2002-08 L4:0x002a18e8 — cut content restored: a Transport Ship's update, ported from the beta build
void cut_content_ships_update_l4(uint32_t m);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_CUT_CONTENT_H
