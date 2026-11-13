// Giant Clank's pad on Quartu and on Veldin's finale (class 1451 / 1899): shared_clank_pad.cpp, one port for both levels'
// copies (with the 1.02 build's change behind 1.02 Gameplay fix 23). Phase 1 of stage 4: the levels' generated copies still
// run; this is proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_CLANK_PAD_H
#define RAC1_GAME_OVL_SHARED_CLANK_PAD_H

#include <cstdint>

namespace rac1 {

void clank_pad_update(uint32_t m);  // class 1451 (L15:0x002ed068), 1899 (L18:0x002fb868)

}  // namespace rac1

#endif
