// The Vendor's "All Ammo" entry (Options > Advanced > Buy All Ammo, off by default): an enhancement, as in Ratchet &
// Clank: Up Your Arsenal's Vendor, that fills every weapon's ammo in one purchase. Not a port: the entry is the
// host's, drawn and handled beside the game's own list (the item list 0x1ca4c0 and its cursor are left as they are).
//
// - The entry sits before the list's first item: Left on the first item selects it, Right goes back to the list.
// - Selected, it shows "All Ammo" and the total price of filling every weapon of the list that is not full (the
//   game's own prices, the PDA's when the screens were opened by the PDA), and the Vendor's sign says so.
// - Cross buys: each weapon of the list in the list's order gets as much ammo as the bolts left pay for (all of it
//   when they are enough), with the game's own steps of an ammo purchase (0x29ba78: the ammo added by 0x233e40, the
//   ammo bought counted, the bolts taken), then once the Vendor's thanks, the list rebuilt, the sign's "purchased"
//   message and the purchase sound. With too few bolts for even one round of any weapon: the game's "not enough
//   bolts" message and its error sound; with every weapon full: the game's "full" sound.
// With the setting off nothing here runs: the game's Vendor is unchanged.
#ifndef RAC1_GAME_ENHANCE_VENDOR_BUY_ALL_H
#define RAC1_GAME_ENHANCE_VENDOR_BUY_ALL_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: game mode 5, after the pad is read and before the Vendor's update (0x29c648): the entry's
// input (selected: left / right / cross taken from the game's view of the pad), and the purchase
void vendor_buy_all_input();

// orig: (none) — enhancement: the Vendor's draw (0x2a02b0), with the HUD (0x23af48): the entry and, selected, its
// panel over the item monitor
void vendor_buy_all_draw();

// orig: (none) — enhancement: the total price of filling every weapon of the Vendor's list (0 when all are full), and
// how many weapons are not full
int32_t vendor_buy_all_price(int32_t* weapons = nullptr);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_VENDOR_BUY_ALL_H
