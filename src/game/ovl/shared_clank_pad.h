// Giant Clank's pad on Quartu and on Veldin's finale (class 1451 / 1899): shared_clank_pad.cpp, one port for both levels'
// copies (with the 1.02 build's change behind 1.02 Gameplay fix 23, always taken on the PAL disc 2.00). Both levels run it
// (quartu_mobys.cpp, veldin_finale_mobys.cpp); oracle batch 510 proves it against each level's own copy, 240 / 270 / 271 /
// 372 on the levels' states.
#ifndef RAC1_GAME_OVL_SHARED_CLANK_PAD_H
#define RAC1_GAME_OVL_SHARED_CLANK_PAD_H

#include <cstdint>

namespace rac1 {

void clank_pad_update(uint32_t m);  // class 1451 (L15:0x002ed068), 1899 (L18:0x002fb868)

}  // namespace rac1

#endif
