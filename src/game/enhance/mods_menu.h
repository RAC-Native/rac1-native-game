// The Mods page (main menu > Options > Mods, an enhancement; docs/mods.md): the mods found in the mods folder
// (runtime/kernel/mods.h), one row each, On / Off; the help box under it shows the selected mod's description, author and
// version. A change is kept in the preferences and applies at the next start (the row says so until then). Not a port:
// built from the game's own widgets as the Controls page (controls.h): a copy of the Camera page with its settings widget
// in the tall box, a copy of the Options help label, the rows a window over the mods (more than ten scroll).
//
// While mods are on, a line says so ("Mods on: N") at the bottom of the title screen, the main menu and the pause menu.
// The page and its Options entry exist only when mods were found (never in a vanilla run: the tests, the oracle).
#ifndef RAC1_GAME_ENHANCE_MODS_MENU_H
#define RAC1_GAME_ENHANCE_MODS_MENU_H

#include <cstdint>

namespace rac1 {

// orig: (none) — enhancement: the page (re)built, its parent the Options page `parent`; its address, 0 when no mod was
// found (no Mods entry)
uint32_t mods_page_install(uint32_t parent);
// orig: (none) — enhancement: the page's address (0 when it is not there)
uint32_t mods_page();
// orig: (none) — enhancement: the ids of the page's title and of its Options help text
int32_t mods_title_text();
int32_t mods_help_text();
// orig: (none) — enhancement: a text of the page's, 0 for others
uint32_t mods_text(int32_t id);
// orig: (none) — enhancement: the settings widget (0x27ef00) changed `byte` (cross on a row); true when it was ours
bool mods_setting_changed(uint32_t byte);
// orig: (none) — enhancement: the settings widget `w`, its selected row `e`: left / right step our rows, the window
// scrolled; true when a row was stepped
bool mods_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed);
// orig: (none) — enhancement: the title's frame (front:0x1eb410: "press start" and the main menu) or the pause menu's
// (0x2772c0) drawn: the "Mods on" line at the bottom while mods are on
void mods_notice_draw();

}  // namespace rac1

#endif
