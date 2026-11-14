// Code every level's overlay carries, reached on Novalis: the particle types 0x06 / 0x08 / 0x38 / 0x39 updates
// and the Vendor's draw callback; and the scenes' level 14 effect (0x23e008) and its draw (0x23db30), the level 14 branch of
// cutscene_level_extras (Oltanis runs its own build: oltanis_*): transliterated at Veldin's addresses, proved against the original.
#ifndef RAC1_GAME_OVL_SHARED_FX_TR_H
#define RAC1_GAME_OVL_SHARED_FX_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

TrRet ovl_25c2b0_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x0025c2b0
void particle_06_update(uint32_t a0);  // 0x00269d20
void vendor_draw_tr(uint32_t a0);  // 0x002a5040
void particle_38_update(uint32_t a0);  // 0x00272838
void particle_39_update(uint32_t a0);  // 0x002728e8
void cutscene_l14_effect_tr(uint32_t a0);  // 0x0023e008
void cutscene_l14_effect_draw_tr(uint32_t a0);  // 0x0023db30

}  // namespace rac1

#endif
