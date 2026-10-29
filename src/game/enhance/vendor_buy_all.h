// The Vendor's "All Ammo" entry (Options > Advanced > Buy All Ammo, off by default): an enhancement, as in Ratchet &
// Clank: Up Your Arsenal's Vendor, that fills every weapon's ammo in one purchase. Not a port.
//
// The entry is an entry of the game's own item list (0x1ca4c0: the list the Vendor's and the PDA's screens walk, scroll,
// draw and wrap): item_list_build (0x29a400) puts it first. The game's code then moves the cursor onto it, scrolls the
// strip or the wheel and draws its tile as for any other entry. It is an ammo entry of one of the list's weapons (the
// weapon not full with the cheapest round, else the first), marked by its +8 word (0 in the game's entries: the game
// starts a purchase only when it is 0, so Cross on the entry starts none). The host steps in only where the game acts on
// the selected entry, at the calls of the vendor's transliteration (tools/codegen/gen/mkall/veldin_vendor.glue):
// - its tile (0x29dea0): a 2 x 2 of the list's own weapon tiles, through the game's hud_frame / hud_sprite;
// - the item monitor (0x29e198): "All Ammo" and the total price; the sign's text when the cursor arrives on it;
// - the screens' opening (0x29a6b0): the cursor on the entry instead of the middle of the list;
// - Cross (vendor_buy_all_input): every weapon of the list filled, with the game's own steps of an ammo purchase.
// The rest is the game's own for the entry's weapon: the hologram on the stand, the monitor's picture and the ammo
// counter at the top left show that weapon.
// With the setting off nothing here runs: every hook calls the game's function as the transliteration did.
#ifndef RAC1_GAME_ENHANCE_VENDOR_BUY_ALL_H
#define RAC1_GAME_ENHANCE_VENDOR_BUY_ALL_H

#include <cstdint>
#include <vector>

namespace rac1 {

// orig: (none) — enhancement: the end of item_list_build (0x29a400): the entry put first in the list
void vendor_buy_all_list_built();

// orig: (none) — enhancement: game mode 5, after the pad is read and before the Vendor's update (0x29c648): Cross on the
// entry buys all
void vendor_buy_all_input();

// orig: (none) — enhancement: the total price of filling every weapon of the Vendor's list (0 when all are full), and
// how many weapons are not full; `pda`: the PDA's prices (-1: by who opened the screens)
int32_t vendor_buy_all_price(int32_t* weapons = nullptr, int pda = -1);

// The vendor transliteration's calls (tools/codegen/gen/mkall/veldin_vendor.glue), `ra` the call site's return address.
// orig: (none) — enhancement: the text lookup 0x203ad8
uint32_t vendor_buy_all_text(uint32_t ra, int32_t id);
// orig: (none) — enhancement: sprintf 0x116248
int32_t vendor_buy_all_sprintf(uint32_t ra, uint32_t dst, uint32_t fmt, const std::vector<uint64_t>& args);
// orig: (none) — enhancement: hud_frame 0x23b208 (`s0`, `s1`: the strip's loop registers of 0x29dea0)
int32_t vendor_buy_all_hud_frame(uint32_t ra, uint32_t key, int32_t i, uint32_t s0, uint32_t s1);
// orig: (none) — enhancement: hud_sprite 0x23b4d8
void vendor_buy_all_hud_sprite(uint32_t ra, int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a);
// orig: (none) — enhancement: camera_reset 0x23aef8
void vendor_buy_all_camera_reset(uint32_t ra);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_VENDOR_BUY_ALL_H
