// The "Controls" options page (ROADMAP Milestone 1 item 2): the host's controls
// (src/host/host_input.h: which PC inputs press which PS2 pad control) shown and rebound
// on a page of Options (pause menu and main menu), built from the game's own widgets as the
// Graphics and Advanced pages (graphics.h).
//
// Not a port, and not the game's: the game still reads its DualShock 2 (g_pad_state 0x13c940, from
// the host's 6-byte packet). The page only edits the host's table (and ~/.config/rac1-native/
// controls.txt); the game sees it only through the pad it reads. While a new input is awaited the
// game's pad is held idle (the press that starts it, the new input, its release: none reach the game).
//
// The bindings live on the host side (orig_main.cpp): the page reaches them through ControlsHost,
// which the host sets; without one (the oracle, the tests) the page shows "-" and changes nothing.
#ifndef RAC1_GAME_ENHANCE_CONTROLS_H
#define RAC1_GAME_ENHANCE_CONTROLS_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// orig: (none) — enhancement: the host's side of the page (orig_main.cpp). Rows are host_input's
// remap rows (0 Cross .. 11 Select, 12 the D-pad, 13 / 14 the sticks, 15 / 16 the
// next / previous weapon); device 0 the gamepad, 1 the
// keyboard and mouse.
class ControlsHost {
public:
    virtual ~ControlsHost() = default;
    // a row's inputs on a device as shown (the game font's button glyphs for a PlayStation pad)
    virtual std::string row_value(int row, int device) = 0;
    // the gamepad's name ("" when none is connected)
    virtual std::string pad_name() = 0;
    // a new input awaited for a row (the game's pad idle until it is pressed and let go, or 5 s pass)
    virtual void rebind_begin(int row, int device) = 0;
    // awaited now; *step: for a group on the keyboard, the direction awaited (0 up, 1 down, 2 left, 3 right)
    virtual bool rebind_waiting(int* step) = 0;
    // the last rebinding's end, once: 0 none, 1 done (*swapped: the rows that got the old inputs), 2 cancelled
    virtual int rebind_result(std::vector<int>* swapped) = 0;
    virtual void reset(int device) = 0;
    virtual bool invert_look() = 0;
    virtual void set_invert_look(bool on) = 0;
    virtual float mouse_sensitivity() = 0;
    virtual void set_mouse_sensitivity(float s) = 0;
    // the mouse's mode: Direct (the movement turns the camera and the aim) or Stick (a simulated right stick)
    virtual bool mouse_direct() = 0;
    virtual void set_mouse_direct(bool on) = 0;
    // Direct while aiming: a factor of the mouse's sensitivity
    virtual float aim_sensitivity() = 0;
    virtual void set_aim_sensitivity(float s) = 0;
};
// orig: (none) — enhancement: the host's side set (nullptr: none)
void controls_set_host(ControlsHost* host);

// --- the menu's side (advanced.cpp, graphics.cpp, the settings widget's hooks) ---
// orig: (none) — enhancement: the Controls page (re)built, its parent the Options page `parent`;
// returns the page's address
uint32_t controls_page_install(uint32_t parent);
// orig: (none) — enhancement: the page's address (0 before it is built)
uint32_t controls_page();
// orig: (none) — enhancement: a text of the page's, 0 for others
uint32_t controls_text(int32_t id);
// orig: (none) — enhancement: the ids of the page's title and of its Options help text
int32_t controls_title_text();
int32_t controls_help_text();
// orig: (none) — enhancement: the settings widget (0x27ef00) stepped `byte` (cross): one of our
// rows; true when it was one of ours
bool controls_setting_changed(uint32_t byte);
// orig: (none) — enhancement: the settings widget `w` (its selected row `e`, pad pressed
// `pressed`), once a frame while focused: our page's scrolling, left / right, the rebinding's end;
// true when handled
bool controls_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_CONTROLS_H
