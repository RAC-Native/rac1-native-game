// Class updates that are only `jr ra` (see empty_class_updates.h).
#include <game/ovl/empty_class_updates.h>

#include <game/ovl/ovl.h>

namespace rac1 {

// orig: 0x002cc358
void sonic_summoner_update(uint32_t) {}

// orig: 0x002d3830
void armor_magnetizer_update(uint32_t) {}

// orig: 0x002e0398
void o2_mask_update(uint32_t) {}

// orig: 0x002df998
void drek_npc_update(uint32_t) {}

// orig: 0x00283900
void end_menu_moby_update(uint32_t) {}

// The other bare `jr ra` functions the game reaches through a pointer (a particle type's update, the levels' class updates
// and camera callbacks): one function for all of them, registered in their places (veldin_extras.cpp, the levels' files).
// orig: 0x0026c3c8 L3:0x002e2a40
void empty_update(uint32_t) {}

// orig: (none) — registers the empty updates for the indirect calls (ovl_call_moby_fn)
void empty_class_updates_register() {
    ovl_register_moby_fn(0x002cc358, sonic_summoner_update);
    ovl_register_moby_fn(0x002d3830, armor_magnetizer_update);
    ovl_register_moby_fn(0x002e0398, o2_mask_update);
    ovl_register_moby_fn(0x002df998, drek_npc_update);
    ovl_register_moby_fn(0x00283900, end_menu_moby_update);
}

}  // namespace rac1
