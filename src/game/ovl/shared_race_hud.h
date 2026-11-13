// The hoverboard races' HUD (Rilgar, Kalebo III): the trick meter (HUD element 0x10, which the race's hero code requests:
// its update and draw) and the bottom bar of the lap and the place (element 5's draw). One port of the two levels' copies
// (shared_race_hud.cpp). Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by
// oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_RACE_HUD_H
#define RAC1_GAME_OVL_SHARED_RACE_HUD_H

#include <cstdint>

namespace rac1 {

// The trick meter's update: the meter follows the race's boost (the hero's +0x864), and a trick's landing is scored.
void race_meter_update(uint32_t e);  // L5:0x00262b58 L16:0x00235268
// The trick meter's draw: the trick or the combo just landed and its points, the meter, and the boost's message.
void race_meter_draw(uint32_t e);    // L5:0x00262f50 L16:0x00235660
// The bottom bar: the lap ("Lap: 1/3"), the place, and on Kalebo III the bolt the race carries.
void race_bar_draw(uint32_t e);      // L5:0x00266320 L16:0x00238a30

}  // namespace rac1

#endif
