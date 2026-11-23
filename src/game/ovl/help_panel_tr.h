// The help message panel: the three callbacks of help_hint_update's request (0x264328: id 0xc, init 0x237190 /
// update 0x2371e0 / draw 0x237200), ported by hand at Veldin's addresses, proved against the original.
#ifndef RAC1_GAME_OVL_HELP_PANEL_TR_H
#define RAC1_GAME_OVL_HELP_PANEL_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

void help_panel_init_tr(uint32_t a0);  // 0x00237190
void help_panel_update_tr(uint32_t a0);  // 0x002371e0
void help_panel_draw_tr(uint32_t a0);  // 0x00237200

}  // namespace rac1

#endif
