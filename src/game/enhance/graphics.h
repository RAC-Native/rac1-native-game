// The "Display" and "Graphics" options pages and New Game's two windows, the mode (Original / Modern) and the graphics
// preset (Current / Original / Low / Medium / High) (ROADMAP Milestone 1 items 3 and 4). Not a port: settings of the PC version's renderer, shown as two
// pages of Options (pause menu and main menu) built from the game's own widgets, as the Advanced page
// (advanced.h). Render side only: none of these settings
// changes the game's state (the view is widened, scaled and drawn by the host; proved by RAC1_RAMHASH
// runs).
//
// The machine's settings: kept as the player's preferences (~/.config/rac1-native/graphics_options.txt) only, set from the
// main menu and the pause menu, never taken from a save nor changed by loading one (the saves' save<N>.pc held the picture
// settings until 2026-11-02, items 32..56: read and ignored, advanced.cpp).
#ifndef RAC1_GAME_ENHANCE_GRAPHICS_H
#define RAC1_GAME_ENHANCE_GRAPHICS_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// --- the host's side (orig_main.cpp) ---
// The window as the settings ask for it
struct GraphicsDisplay {
    bool vulkan = false;  // the Graphics page's Renderer row (true: Vulkan, false: OpenGL, the default); read once, at the window's making
    bool fullscreen = false;
    int window_w = 960, window_h = 720;  // the size in window mode
    bool vsync = true;
    int fps_cap = 60;  // 0: unlimited
    int monitor = -1;  // the Graphics page's Monitor row, an index in the host's list (-1: the primary monitor)
};
// orig: (none) — enhancement: the host's monitors as GLFW names them, in its order, given once after GLFW is up
// (the Monitor row's values; the list is not fixed, and its order is not stable between sessions)
void graphics_set_monitors(std::vector<std::string> names);
// orig: (none) — enhancement: the host's renderer when it falls back to OpenGL (no Vulkan driver)
void graphics_set_vulkan(bool vulkan);
// orig: (none) — enhancement: the preferences read and applied to the renderer (view mode, aspect,
// field of view, level of detail), once; nothing read while every enhancement is off (headless
// runs, the oracle): the defaults
void graphics_load();
// orig: (none) — enhancement: the window settings now, and a counter bumped each time the page
// changes one of them (the host applies them when it changes)
const GraphicsDisplay& graphics_display();
uint32_t graphics_display_serial();
// orig: (none) — enhancement: the host's window as it is (its size in window mode, fullscreen):
// the page's "current" size, kept in the preferences
void graphics_host_window(int w, int h, bool fullscreen);
// orig: (none) — enhancement: the 3D's render scale (0.5 .. 2: the MRP's picture drawn at that
// fraction of the output's resolution, then scaled to it; the 2D at the output's)
float graphics_render_scale();
// orig: (none) — enhancement: testing / env overrides (RAC1_RENDER_SCALE)
void graphics_set_render_scale_percent(int pct);

// --- the global switch: Graphics Original / Enhanced ---
// orig: (none) — enhancement: true when the renderer's additions (the modern lighting and every row of the Graphics page under
// the switch: shadows, lights, occlusion, sun rays, surface detail, grass, ambient particles) are drawn now; false: the
// original's picture whatever those rows hold. The one gate: the settings (and the debug menu's live switch) set the renderer's
// switches through it
bool graphics_enhanced();
// orig: (none) — enhancement: the debug menu's live switch (this run only, the preferences kept): Original stops every addition
// (their live values kept aside), Enhanced brings them back
void graphics_set_enhanced_live(bool on);
// orig: (none) — enhancement: the Graphics page's presets (0 Low, 1 Medium, 2 High; 3 Custom: none of them): the one the
// renderer's live switches match, and one set live (the debug menu; this run only)
int graphics_live_preset();
void graphics_set_live_preset(int preset);

// --- the menu's side (advanced.cpp, the ported widgets' enhancement hooks) ---
// orig: (none) — enhancement: the Display and Graphics pages (re)built, their parent the Options page `parent`
// (the pause menu's or the main menu's); returns the Graphics page's address (graphics_display_page: the Display page's)
uint32_t graphics_page_install(uint32_t parent);
uint32_t graphics_display_page();
// orig: (none) — enhancement: the menu leaves page `cur` for its parent `to` (back): when `cur` is one of
// our pages (Graphics, Advanced, the New Game popup), the 14 menu mobys `mobys` (their addresses) put on
// `to`'s boxes at once. Our pages borrow their boxes' animations from other pages (Camera, Options...),
// whose first frames are those pages' parents' boxes, not ours' parent's (the main menu's Options, the
// root page): played back as the game plays a page's own, they ended on the wrong boxes
void graphics_menu_back(uint32_t cur, uint32_t to, uint32_t mobys);
// orig: (none) — enhancement: the menu goes forward from page `cur` to `to`: from New Game's first window to its second,
// the boxes held as they are (both windows are the same boxes; played from their first frames they jumped)
void graphics_menu_forward(uint32_t cur, uint32_t to, uint32_t mobys);
// orig: (none) — enhancement: a text of ours (the Graphics page's, the mode popup's), 0 for others
uint32_t graphics_text(int32_t id);
// orig: (none) — enhancement: the ids of the pages' titles and of their Options help texts (Graphics, Display)
int32_t graphics_title_text();
int32_t graphics_help_text();
int32_t graphics_display_title_text();
int32_t graphics_display_help_text();
// orig: (none) — enhancement: the settings widget (0x27ef00) changed `byte`: a Graphics row
// stepped (+1); true when it was one of ours
bool graphics_setting_changed(uint32_t byte);
// orig: (none) — enhancement: the settings widget `w`'s selected row `e` (left / right, pad
// pressed `pressed`): our pages' rows stepped back / forward; true when handled
bool graphics_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed);
// orig: (none) — enhancement: the Texture Detail row applied to the MRP's mip switches (the host calls it
// once it has read its testing variables; the row applies it live afterwards)
void graphics_mips_apply();
// orig: (none) — enhancement: once a game frame: the preferences written when changed
void graphics_frame();
// orig: (none) — enhancement: the debug menu's Enhancements tab: the live switch (Original / Enhanced) and the renderer's live values
// (lighting, shadows, lights, occlusion, rays, materials, particles, grass) taken as the player's preferences ("Save as my
// settings"), written at the next frame
void graphics_take_live();

// --- the main menu (the boot module) ---
// orig: (none) — enhancement: the main menu's start (front:0x218f98): the Options pages of ours
// (Display, Graphics, Advanced) and New Game's two windows built, New Game pointed at the first
void graphics_main_menu_install();
// orig: (none) — enhancement: the main menu's Options list `w` just got its entries (front:0x21a1b0):
// Display, Graphics, Controls and Advanced added
void graphics_main_options_entries(uint32_t w);
// orig: (none) — enhancement: the list `w` runs its entry's action 4 (New Game): when it is New Game's second window's,
// the mode chosen in the first (advanced_apply_mode) and the preset of its selected entry applied
void graphics_new_game_choice(uint32_t w);
// The modes (the first window's entries; 0 none known). 3 was Experimental (Modern plus Graphics Enhanced, High) until
// 2026-11-02: an older save's 3 is read as Modern
enum : uint32_t { GAME_MODE_NONE = 0, GAME_MODE_ORIGINAL = 1, GAME_MODE_MODERN = 2 };
// orig: (none) — enhancement: the Advanced page's gameplay rows set for a mode (advanced.cpp)
void advanced_apply_mode(uint32_t mode);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_GRAPHICS_H
