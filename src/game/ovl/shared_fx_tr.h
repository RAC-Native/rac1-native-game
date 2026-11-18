// Shared effects carried by every level's overlay (hand-ported at Veldin's addresses in veldin_particle_fx.cpp and
// veldin_vendor_draw.cpp): particle types 0x06, 0x38, and 0x39, helper vec_damp_along (0x25c2b0), and the Vendor's
// world draw callback (0x2a5040).
#ifndef RAC1_GAME_OVL_SHARED_FX_TR_H
#define RAC1_GAME_OVL_SHARED_FX_TR_H

#include <game/math/vecmath.h>

#include <cstdint>

namespace rac1 {

void vec_damp_along(Vec4& out, const Vec4& v, const Vec4& dir, float k);  // 0x0025c2b0
void particle_06_update(uint32_t p);                                      // 0x00269d20
void vendor_draw_tr(uint32_t m);                                          // 0x002a5040
void particle_38_update(uint32_t p);                                      // 0x00272838
void particle_39_update(uint32_t p);                                      // 0x002728e8

}  // namespace rac1

#endif
