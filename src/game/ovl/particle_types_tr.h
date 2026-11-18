// Veldin's particle types updates: part 1 (0x03, 0x09, 0x0a, 0x0e, 0x14, 0x18, 0x1d, 0x1e, 0x1f, 0x24, 0x25, 0x26,
// and helper local_offset_coord 0x25c230) hand-ported in veldin_particles_1.cpp, and the remaining entries of
// particle_types_tr.cpp (regen group veldin_particles).
#ifndef RAC1_GAME_OVL_PARTICLE_TYPES_TR_H
#define RAC1_GAME_OVL_PARTICLE_TYPES_TR_H

#include <cstdint>

#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// Hand-ported in veldin_particles_1.cpp
float local_offset_coord(const Vec4& origin, const Mat4& mat, const Vec4& pt, int32_t axis);  // 0x0025c230
void particle_03_update(uint32_t p);  // 0x002694f8
void particle_09_update(uint32_t p);  // 0x0026a6f0
void particle_0a_update(uint32_t p);  // 0x0026a880
void particle_0e_update(uint32_t p);  // 0x0026ba68
void particle_14_update(uint32_t p);  // 0x0026c860
void particle_18_update(uint32_t p);  // 0x0026d470
void particle_1d_update(uint32_t p);  // 0x0026e3a8
void particle_1e_update(uint32_t p);  // 0x0026e670
void particle_1f_update(uint32_t p);  // 0x0026eb88
void particle_24_update(uint32_t p);  // 0x0026f898
void particle_25_update(uint32_t p);  // 0x0026fa00
void particle_26_update(uint32_t p);  // 0x0026fb00

// Generated in particle_types_tr.cpp (veldin_particles)
TrRet ovl_259430_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x00259430
TrRet ovl_270758_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x00270758 (type 0x2a's spawner)
void particle_43_update(uint32_t a0);  // 0x00274518
void particle_30_update(uint32_t a0);  // 0x00271c88
void particle_32_update(uint32_t a0);  // 0x00271df8
void particle_33_update(uint32_t a0);  // 0x00271ec8
void particle_3d_update(uint32_t a0);  // 0x00273448
void particle_41_update(uint32_t a0);  // 0x00273ee0
void particle_3a_update(uint32_t a0);  // 0x00272bc0
void particle_27_update(uint32_t a0);  // 0x0026fc38
void particle_49_update(uint32_t a0);  // 0x00275320
void particle_07_update(uint32_t a0);  // 0x0026a418
void particle_2a_update(uint32_t a0);  // 0x00270948
void particle_36_update(uint32_t a0);  // 0x00272438
void particle_4b_update(uint32_t a0);  // 0x002756f0
void particle_4c_update(uint32_t a0);  // 0x00275810

}  // namespace rac1

#endif
