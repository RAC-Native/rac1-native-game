#ifndef RAC1_GAME_OVL_VENDOR_FRAME_H
#define RAC1_GAME_OVL_VENDOR_FRAME_H
// The Vendor's screens' frame picture:, ADR 0012.
#include <cstdint>

namespace rac1 {

// The kernel's vsync wait 0x122298 as the vendor's screens call it (vendor_draw.cpp, vendor_screens.cpp, Veldin's addresses;
// `ra` the call's return address there). The draw 0x2a02b0 renders a frame of the game (0x1f85b8), waits a vsync, kicks it
// and stores the frame buffer's picture (0x1f99c0: another wait, then 0x2a0f18) that it puts back as the background of
// every later frame (0x2a0fc8): the first wait is a frame the host draws as a frame of the main loop (EeVsyncFrame::Scene),
// the second a vsync that shows nothing new. Other call sites (the dialog stream's wait 0x29af48): not ported.
void vendor_vsync_wait(uint32_t ra);

// The vendor's screens draw over their stored picture this frame (game mode 5, the draw's mode word +8 = 1): what the host
// draws before the picture is put back is covered by it (the host's 3D renderers, the MRP's mobys).
bool vendor_picture_frame();

}  // namespace rac1

#endif
