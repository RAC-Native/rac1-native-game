// The host's overlay (the rac1-native window, F1: src/_devtools/dev_menu.cpp) and a few rows of the Graphics page
// (graphics.cpp) it shows. Not a port. Kept out of graphics.h, which the code generators read (a change there asks for
// ./setup.sh again).
#ifndef RAC1_HOST_GRAPHICS_QUICK_H
#define RAC1_HOST_GRAPHICS_QUICK_H

#include <string>

namespace rac1 {

enum class QuickSetting { RenderScale, Aspect, Hud, Vsync };
// orig: (none) — enhancement: a row's number of values, its value now, a value's text (the page's)
int graphics_quick_count(QuickSetting q);
int graphics_quick_value(QuickSetting q);
std::string graphics_quick_text(QuickSetting q, int value);
// orig: (none) — enhancement: a value asked for by the overlay (the window's thread, while the game thread may run): applied
// by graphics_frame, between the game frames, as the page's own changes are
void graphics_quick_set(QuickSetting q, int value);
// orig: (none) — enhancement: a value the display supports (the overlay lists those only, as the page steps over the rest)
bool graphics_quick_supported(QuickSetting q, int value);
// orig: (none) — enhancement: the overlay shown (the preferences' "overlay", on by default)
bool graphics_overlay_shown();
// orig: (none) — enhancement: the output's size in pixels, given by the host each frame (the render scale's 4K value is
// a multiple of it; the overlay shows each value's resolution)
void graphics_output_size(int w, int h);
void graphics_set_overlay_shown(bool on);

}  // namespace rac1

#endif  // RAC1_HOST_GRAPHICS_QUICK_H
