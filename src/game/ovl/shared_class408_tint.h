// Class 408's tint (Quartu, Drek's fleet): a draw callback that pulses the GS register 1 colour (the screen's fog / tint)
// with a sine of the frame counter; one port of the two levels' copies (shared_class408_tint.cpp). Phase 1 of stage 4: the
// levels' generated copies still run; this is proved against them by oracle batch 507.
#ifndef RAC1_GAME_OVL_SHARED_CLASS408_TINT_H
#define RAC1_GAME_OVL_SHARED_CLASS408_TINT_H

#include <cstdint>

namespace rac1 {

void class408_tint(uint32_t m);  // L15:0x002cbd88 L17:0x002cc740

}  // namespace rac1

#endif
