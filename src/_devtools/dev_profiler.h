#pragma once
// _devtools/dev_profiler.h — the debug menu's Profiler tab and its overlay (openly invented, ADR 0003; not a port).
// The numbers come from src/profiler/host_profiler.h: recorded only while the tab is shown or the overlay is on.

namespace rac1::devtools {

// The tab's contents (inside the debug menu's window).
void tab_profiler();
// Whether the overlay is on (drawn with the menu closed too).
bool profiler_overlay_wanted();
// The overlay (a small window over the frame; inside an ImGui frame).
void profiler_overlay();

}  // namespace rac1::devtools
