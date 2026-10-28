// The GamePad's debug menu (testing, the Wii U port; RAC1_WIIU_MENU=1): while it is on, the GamePad shows a touch menu instead of the
// game's picture - the picture's pieces turned on and off live (the world's tfrags / ties / shrubs, the mobys, the 2D, the frames'
// overlap, the world's worker, the merged draws, the resolution) and the host frame's time by stage, measured as one plays. A tap on
// an item flips it. Host code.
#ifndef RAC1_WIIU_DEBUG_MENU_H
#define RAC1_WIIU_DEBUG_MENU_H

#include <gx2/surface.h>

#include <string>
#include <vector>

// on: RAC1_WIIU_MENU=1 (read once)
bool debug_menu_wanted();
// its shader (shaders/debug_text.gsh), font and buffers: after WHBGfxInit; false: not up (the GamePad shows the game as before)
bool debug_menu_init();
void debug_menu_shutdown();
// the touch screen read (once a host frame, before the draw: a new tap flips its item)
void debug_menu_input();
// the menu and `stats` (the timing's lines) drawn into the GamePad's colour buffer (WHBGfxBeginRenderDRC done, the buffer cleared)
void debug_menu_draw(const GX2ColorBuffer* cb, const std::vector<std::string>& stats);

// the switches the menu flips (wiiu_gfx.cpp and the backends read them each frame)
extern bool g_menu_draw_mobys, g_menu_draw_2d, g_menu_overlap;
extern float g_menu_vpscale;

#endif
