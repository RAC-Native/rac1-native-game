// Cut content: things the retail game has the data for but never shows, brought back as an option (main menu > Options >
// Advanced > Cut Content, off by default; a global setting, kept in the preferences only). Not a port: each item's
// behaviour is reconstructed (the game's own code for it is a stub), in `// orig: (none)` functions chosen where the
// game dispatches to the original's, so that with the option off the faithful port runs, byte for byte the same.
// The items, and the candidates left out: docs/advanced.md ("Cut Content") and docs/port-inventory.md.
//
// Items (their names for RAC1_CUT_CONTENT):
// - fred: Kerwan's citizens "Fred 1 / 2 / 3" (classes 915, 916, 917; 14 placements on level 3). Their update (L3:0x2dc310)
//   measures the distance and the angle to Ratchet, throws both away and retires the moby on its first frame: with the
//   item on they stay, hold their one sequence (one frame: the models' rest pose, arms out) and turn to face Ratchet
//   when he comes near. They have no collision.
//
// The option is read when a level's mobys first update (a citizen retired then is gone until the level is loaded
// again; one kept stays), never written to a save. The oracle and the headless runs have it off (g_advanced_disabled),
// unless a headless run keeps the settings (RAC1_ADVANCED=1).
#ifndef RAC1_GAME_ENHANCE_CUT_CONTENT_H
#define RAC1_GAME_ENHANCE_CUT_CONTENT_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: the cut content item `item` ("fred") is restored now: RAC1_CUT_CONTENT=<item>,<item>...
// (testing; "none" or empty: none) over the Cut Content row; false while every enhancement is off
bool cut_content_on(const char* item);

// orig: (none) — enhancement: the update registered at L3:0x2dc310 (classes 915 / 916 / 917): the restored citizen
// (cut_content_fred_update_l3) with the item on, or a citizen already kept by it; else the faithful fred_update_l3
void cut_content_fred_dispatch_l3(uint32_t m);
// orig: (none) — cut content restored: a Kerwan citizen alive, its idle sequence looped, turned to face Ratchet near it
void cut_content_fred_update_l3(uint32_t m);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_CUT_CONTENT_H
