// The Vendor's screens (game mode 5): their start 0x29a6b0 (from the Vendor moby 0x2a5d64, or the hero's PDA), their
// per-frame update 0x29c648 and draw 0x2a02b0 (vendor_screens.cpp, vendor_update.cpp, vendor_draw.cpp; the screens'
// own functions in vendor_screens.h).
#ifndef RAC1_GAME_OVL_VENDOR_TR_H
#define RAC1_GAME_OVL_VENDOR_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

void vendor_screen_start_tr(uint32_t vendor);  // orig: 0x0029a6b0
void vendor_mode_update_tr(uint32_t a0);       // orig: 0x0029c648
void vendor_mode_draw_tr(uint32_t a0);         // orig: 0x002a02b0
// The draw callback the update queues while the screens open and close (the moby the Vendor): the screens' glass.
void vendor_screens_glass_cb(uint32_t vendor);  // orig: 0x0029f990

// The demo scene's stream functions as the generated code calls them (the novalis_gold unit's glue, uniform ABI: v0 and
// f0 returned; f0 is 0): vendor::scene_stream_load and vendor::scene_block_select.
TrRet ovl_2454c8_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // orig: 0x002454c8
TrRet ovl_2455c0_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // orig: 0x002455c0

}  // namespace rac1

#endif
