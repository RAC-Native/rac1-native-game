#pragma once
// _devtools/dev_unlocks.h — the debug menu's Unlocks tab (openly invented, ADR 0003; not a port).
// The non-weapon items (the pause menu's Gadgets and Items pages: the gadgets, packs, helmets, boots, the Hoverboard, the
// Map-o-matic, the Bolt Grabber, the Persuader, and the goodies of 0x13d388: Zoomerator, Raritanium, CodeBot, Premium /
// Ultra Nanotech), the gold bolts (0x14bec0[level * 4 + k]) and the gold weapons (0x13e520[item]) given and taken back
// through the game's own state: the bytes and the writes the game itself makes (see dev_unlocks.cpp).

#include <cstdint>

namespace rac1::devtools {

// The Unlocks tab's contents (inside the debug menu's tab bar).
void tab_unlocks();

// Testing: RAC1_UNLOCKS=<frame>:<op>[,<frame>:<op>...] applied at those game frames (ops: items+ items- bolts+ bolts-
// gold+ gold- all+ all-, as the tab's "all" / "none" buttons). Call once per game frame; nothing when it is unset.
void unlocks_testing(uint64_t frame);

}  // namespace rac1::devtools
