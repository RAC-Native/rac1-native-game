// End-of-game menu pages (menu state 0x21 and page state 0x23 widget functions; implemented in end_menu.cpp).
#ifndef RAC1_GAME_OVL_END_MENU_TR_H
#define RAC1_GAME_OVL_END_MENU_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

uint32_t end_menu_panel_update_tr(uint32_t a0, uint32_t a1);  // 0x00283530
uint32_t end_menu_panel_draw_tr(uint32_t a0, uint32_t a1);  // 0x002838b0
uint32_t end_menu_panel_focus_in_tr(uint32_t a0, uint32_t a1);  // 0x002833d0
uint32_t end_menu_panel_focus_out_tr(uint32_t a0, uint32_t a1);  // 0x002834a8
uint32_t end_menu_choice_update_tr(uint32_t a0, uint32_t a1);  // 0x0027fed8
uint32_t end_menu_choice_draw_tr(uint32_t a0, uint32_t a1);  // 0x002806e0
uint32_t end_menu_page23_update_tr(uint32_t a0, uint32_t a1);  // 0x00280038
uint32_t end_menu_page23_draw_tr(uint32_t a0, uint32_t a1);  // 0x002803b0
uint32_t end_menu_page23_focus_in_tr(uint32_t a0, uint32_t a1);  // 0x00280020

// orig: (host) a widget function of the end-of-game menu's pages (Veldin's address `veldin`, the level's code address as Veldin's): true when
// it is one of them (its return value in `r`)
bool end_menu_widget_call(uint32_t veldin, uint32_t w, int32_t a1, int32_t& r);

}  // namespace rac1

#endif
