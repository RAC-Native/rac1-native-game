// The classes Veldin and Novalis share (the Vendor 11, the Infobot 750), transliterated at Veldin's addresses.
#ifndef RAC1_GAME_OVL_SHARED_CLASSES_TR_H
#define RAC1_GAME_OVL_SHARED_CLASSES_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

void vendor_update_tr(uint32_t a0);  // 0x002a57a8
TrRet ovl_266858_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x00266858
TrRet ovl_2d5988_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x002d5988
TrRet ovl_2d6cd0_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x002d6cd0
void infobot_update_tr(uint32_t a0);  // 0x002d5de8
void class167_update_tr(uint32_t a0);  // 0x002b4198 (class 167, the Thruster-Pack's two jets: spawned by hero_update with the pack 3)
void class167_draw_tr(uint32_t a0);    // 0x002b3628 (its draw-list callback, queued by its update)

// orig: (none) -- registers the shared classes' updates
void shared_classes_tr_register();

}  // namespace rac1

#endif
