#pragma once
// _devtools/dev_menu.h — openly-invented debug menu for rac1-native (ADR 0003; not a port).
// Only with --debug-tools or RAC1_DEBUG_TOOLS=1. R3 on the pad or F1 opens it: rendering categories and shading, display (the
// window's settings are in Options > Graphics), the game's speed / pause / single step, its language and a state snapshot,
// the player (health, bolts, cheats, moving him by hand, teleport, saved positions, checkpoint), the items (give,
// equip, ammo), the mobys (freeze, retire, list, go to), a free camera, audio, and the engine
// (the frame control block, unported calls).

struct GLFWwindow;

namespace rac1::devtools {

struct DevSettings {
    bool open = false;
    bool block_game_input = true;  // the pad goes to the menu while it is open
    bool pause_when_open = false;
    bool paused = false;
    bool step_once = false;
    float speed = 1.0f;     // game frames per 1/60 s
    // (the frame cap and v-sync are in Options > Graphics, src/game/enhance/graphics.h)
    bool pipeline = true;   // a frame drawn after the next game frame's update: its world built meanwhile (one frame of latency)
    bool game_thread = true;  // with the pipelined frames: the game's update on its own thread while that frame is drawn (runtime/ee/game_thread.h)
    bool god_mode = false;
    bool infinite_ammo = false;
    bool infinite_bolts = false;
    bool show_stats = false;
};

DevSettings& dev_settings();

// The game language saved by the debug menu (devmenu_display.txt), written to the game's
// language 0x15ed88 before the level loads: the host stands in for the console setting the
// overlay's main (0x23d598, not ported) reads at boot. Call before the level's load.
void dev_language_boot();

// Whether to start with the game's own boot (the intro movie, the title, the main menu): the debug menu's saved choice
// (devmenu_display.txt "boot"), on by default.
bool dev_boot_wanted();

// Loads its saved settings (devmenu_display.txt: pipelining, language, boot; the window's are Options > Graphics). with_menu:
// the debug tools (--debug-tools / RAC1_DEBUG_TOOLS=1): the menu and its hotkeys; without, poll / draw only show the
// host's overlay (the rac1-native window, F1) and dev_menu_owns_input is false.
void dev_menu_init(GLFWwindow* window, bool with_menu);
void dev_menu_shutdown();
// Polls R3 / F1 (edge): opens or closes the menu; F2 / L3 toggles moving the hero by hand; moves
// the hero or the free camera, takes a state snapshot.
// Call once per host frame before the game frames.
void dev_menu_poll(GLFWwindow* window);
// Whether the game should get no pad input this frame (the menu or the free camera has it).
bool dev_menu_owns_input();
// Whether the cursor is over the host's overlay (the rac1-native window, F1): its clicks are not the game's, nor do they
// capture the cursor.
bool dev_overlay_wants_mouse();
// The cheats that hold every game frame (after the game's update).
void dev_menu_game_frame();
// Kills Ratchet through the game's own death state (done at the next game frame's end). Also RAC1_DIE_AT=<game frame> (testing).
void dev_kill_hero();
void dev_kill_apply();  // the pending kill, between game frames (dev_menu_game_frame calls it; the headless loop too)
// Hoverboard anywhere: puts Ratchet on the race's hoverboard where he stands (on: the board's own mount) or takes him off
// (off: the game's hero_set_state(0, 1)); only on Rilgar (5) and Kalebo III (16), the levels with the board. Done at the next
// game frame's end. Also RAC1_HOVERBOARD_AT=<on frame>[,<off frame>,<on frame>...] (testing).
void dev_hoverboard_set(bool on);
bool dev_hoverboard_on();
void dev_hoverboard_apply();  // the pending change, between game frames (dev_menu_game_frame calls it; the headless loop too)
// Draws the menu (and the stats overlay) over the frame. update_ms: the game's update time.
void dev_menu_draw(double update_ms, double frame_ms);
// Whether dev_menu_draw draws anything now (the menu, the stats or the profiler's overlay): the frame capture's choice (orig_main.cpp)
bool dev_menu_drawing();

}  // namespace rac1::devtools
