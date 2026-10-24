#pragma once
// _devtools/dev_unlocks.h — the debug menu's Unlocks tab (openly invented, ADR 0003; not a port).
// The non-weapon items (the pause menu's Gadgets and Items pages: the gadgets, packs, helmets, boots, the Hoverboard, the
// Map-o-matic, the Bolt Grabber, the Persuader, and the goodies of 0x13d388: Zoomerator, Raritanium, CodeBot, Premium /
// Ultra Nanotech), the gold bolts (0x14bec0[level * 4 + k]) and the gold weapons (0x13e520[item]) given and taken back
// through the game's own state: the bytes and the writes the game itself makes (see dev_unlocks.cpp). Also the weapons and
// their ammo, the skill points (0x13d408), the cheats (unlocked 0x15edc0, on 0x15edb0), the challenge mode count 0x15ee20
// (the pause menu's Goodies) and the planets on the ship's map (the game's level_mark_visited).

#include <cstdint>

namespace rac1::devtools {

// The Unlocks tab's contents (inside the debug menu's tab bar).
void tab_unlocks();

// Testing: RAC1_UNLOCKS=<frame>:<op>[,<frame>:<op>...] applied at those game frames (ops, as the tab's buttons: items+-
// weapons+- bolts+- gold+- skills+- cheats+- (unlocked) cheatson+- (switched on) challenge+- planets+ ammo+ all+-, all+ the
// "Unlock everything"). Call once per game frame; nothing when it is unset.
void unlocks_testing(uint64_t frame);

}  // namespace rac1::devtools
