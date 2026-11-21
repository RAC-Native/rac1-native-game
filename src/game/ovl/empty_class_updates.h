// Class updates that are only `jr ra`: the gadgets' classes 433 Sonic Summoner, 618 Armor Magnetizer and 1289 O2 Mask
// (class 1289 is spawned by the ship's landing sequence 0x290590), and 1249 Drek (the NPC model of his scenes,
// placed on Veldin's finale, level 18); and the update the end-of-game menu gives the mobys it spawns (0x2830d8: class 1957, the Helpdesk Lady who
// presents its choices).
#ifndef RAC1_GAME_OVL_EMPTY_CLASS_UPDATES_H
#define RAC1_GAME_OVL_EMPTY_CLASS_UPDATES_H

#include <cstdint>

namespace rac1 {

void sonic_summoner_update(uint32_t m);  // orig: 0x002cc358  class 433
void armor_magnetizer_update(uint32_t m);  // orig: 0x002d3830  class 618
void o2_mask_update(uint32_t m);  // orig: 0x002e0398  class 1289
void drek_npc_update(uint32_t m);  // orig: 0x002df998  class 1249 Drek
void end_menu_moby_update(uint32_t m);  // orig: 0x00283900  the end-of-game menu's mobys
// Any other bare `jr ra` reached through a pointer: particle type 0x11's update (0x26c3c8), the levels' empty class updates
// and camera callbacks (their registrations name the addresses).
void empty_update(uint32_t m);
// orig: (none) — registers them for the indirect calls (ovl_call_moby_fn)
void empty_class_updates_register();

}  // namespace rac1
#endif
