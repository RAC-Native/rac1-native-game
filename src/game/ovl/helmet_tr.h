// Class 1290 (Wrench's "Pilot's Helmet"), the item pickup that gives the Pilot's Helmet (item 7) on Gaspar: Veldin's update
// (0x2e03a0, shared by every level's class table and unported before), transliterated at Veldin's addresses
// (tools/codegen/gen/mkall/veldin_helmet.json).
#ifndef RAC1_GAME_OVL_HELMET_TR_H
#define RAC1_GAME_OVL_HELMET_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

TrRet ovl_2e06a8_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x002e06a8
TrRet ovl_2e0788_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x002e0788
void helmet_update_tr(uint32_t a0);  // 0x002e03a0 (class 1290)
// The sparkles' fields of moby `m`'s pvars (+0x10 .. +0x4c) set up (Veldin's 0x2e06a8, the same code as its 0x2d6bf0).
void helmet_init_fx(uint32_t m);

}  // namespace rac1

#endif
