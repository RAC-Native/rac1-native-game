// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/quick_rings.cpp.)
// Three quick select rings (an enhancement, Advanced > Quick Select Rings: 1 / 3, 1 by default; New Game > Modern turns it
// to 3), as the later games of the series have them. Ring 1 is the game's own (0x141ea0[8], in the game's save as before);
// rings 2 and 3 are ours, eight items each, kept in save<N>.pc (advanced.cpp, item 0x400) next to the game's save.
// - In game, while the quick select is open (Triangle held): L1 / L2 show the ring on the left, R1 / R2 the one on the right
//   (they wrap: ring 1's left is ring 3); those presses never reach the game (no crouch, no first-person view), until the
//   button is let go. The ring shown when the quick select closes is the one it opens on next time. Above the ring: the
//   L1 / R1 arrows (the pause page's frame 0xe99e) and three dots, the ring shown lit. The PC's next / previous weapon
//   moves the selection inside the ring shown (and, closed, equips from it).
// - Pause menu > Quick Select: L1 / R1 turn the selected place as before, L2 / R2 change the ring being edited (its number
//   above the wheel). The same item may be in more than one ring; placing it takes it only from its old place in that ring.
// - A new item that finds ring 1 full goes to the first free place of ring 2, then ring 3 (the game's item_give 0x260860).
// - A plugin's weapon_allow (mod_code.h) blanks the items it disallows in every ring.
// At 1 (and while every enhancement is off: headless runs, the reference checks) nothing here changes what the game does.
#ifndef RAC1_HOST_QUICK_RINGS_H
#define RAC1_HOST_QUICK_RINGS_H

#include <cstdint>
#include <vector>

namespace rac1 {

// orig: (none) — enhancement: the rings there are: 3, or 1 (the game's alone; the option at 1, or every enhancement off).
// advanced.cpp's (the option is a row of the pause menu's Advanced page)
int quick_rings_count();

// orig: (none) — enhancement: the ring the quick select shows / opens on (0 ring 1, the game's; 0 while there is one ring)
int quick_rings_shown();

// orig: (none) — enhancement: ring `ring`'s item at place `place` (0..7; ring 0 is the game's 0x141ea0)
uint32_t quick_rings_item(int ring, int place);

// orig: (none) — enhancement: the item at place `place` of the ring shown (0x141ea0[place] while ring 1 is shown)
uint32_t quick_rings_shown_item(int32_t place);

// orig: (none) — enhancement: the pad state's build (pad_build, before the game's remaps): with three rings and the quick
// select open, the shoulder buttons' presses noted (the ring switch) and taken from the game until they are let go; the
// held buttons `held` as the game must see them
uint32_t quick_rings_pad(uint32_t pad, uint32_t held);

// orig: (none) — enhancement: the quick select's start (quick_select_start 0x237ae8, its slots made from ring 1): the ring
// last used put in them instead
void quick_rings_start(uint32_t e);

// orig: (none) — enhancement: the quick select's frame (quick_select_update 0x237ba0, first): a ring switch asked for
void quick_rings_update(uint32_t e);

// orig: (none) — enhancement: the quick select's draw (quick_select_draw 0x2382a0): the arrows and the ring's dots above the
// box at (x, y), alpha `alpha` (0..128)
void quick_rings_draw(uint32_t e, int32_t x, int32_t y, int32_t alpha);

// orig: (none) — enhancement: an icon's alpha `a` of the quick select, faded in after a ring switch
int32_t quick_rings_icon_alpha(int32_t a);

// orig: (none) — enhancement: Pause menu > Quick Select (pause_menu_quick.cpp, its 8 items at w+0x30): the ring edited's
// item at `place` when the page is focused in (0x141ea0's for ring 1)
uint32_t quick_rings_menu_item(uint32_t w, int place);
// orig: (none) — enhancement: the page focused out: its items written to the ring edited; false for ring 1 (the game's
// own write to 0x141ea0)
bool quick_rings_menu_focus_out(uint32_t w);
// orig: (none) — enhancement: the page's frame: L2 / R2 change the ring edited
void quick_rings_menu_update(uint32_t w);
// orig: (none) — enhancement: the page's draw: the L2 / R2 arrows and the ring's number above the wheel
void quick_rings_menu_draw(uint32_t w);

// orig: (none) — enhancement: item_give (0x260860) found ring 1 full for item `id`: put in ring 2's, else ring 3's first
// free place (with three rings)
void quick_rings_give(uint32_t id);

// orig: (none) — enhancement: save<N>.pc's item (advanced.cpp): rings 2 and 3 and the ring last used, appended to `d` when
// there is something to keep (the count of items added); an item read (true when it is ours); the rings emptied before a
// save's items are read (a save without them: none); a new game's (empty)
uint32_t quick_rings_pc_items(std::vector<uint8_t>& d);
bool quick_rings_pc_item(uint32_t id, const uint8_t* p, uint32_t size);
void quick_rings_pc_reset();
void quick_rings_new_game();
// orig: (none) — enhancement: the challenge mode's new game (challenge_game_image 0x284950): the rings kept over the default
// save read (before / after new_game_state), the items no longer owned dropped as the game drops ring 1's
void quick_rings_challenge_keep();
void quick_rings_challenge_restore();

// orig: (none) — enhancement: our texts (0x7a00..0x7a02: "Ring 1".."Ring 3"); 0 for another id
uint32_t quick_rings_text(int32_t id);

}  // namespace rac1

#endif
