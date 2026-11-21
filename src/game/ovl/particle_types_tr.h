// Veldin's particle types updates: part 1 (veldin_particles_1.cpp) and part 2 (veldin_particles_2.cpp).
#ifndef RAC1_GAME_OVL_PARTICLE_TYPES_TR_H
#define RAC1_GAME_OVL_PARTICLE_TYPES_TR_H

#include <cstdint>

#include <game/math/vecmath.h>

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

// Hand-ported in veldin_particles_2.cpp
int32_t particle_alpha_dec(uint32_t col_addr, int32_t step);  // 0x00259430
uint32_t particle_2a_spawn(uint32_t pos, uint32_t vel, uint8_t flags, uint32_t owner);  // 0x00270758
void particle_07_update(uint32_t p);  // 0x0026a418
void particle_27_update(uint32_t p);  // 0x0026fc38
void particle_2a_update(uint32_t p);  // 0x00270948
void particle_30_update(uint32_t p);  // 0x00271c88
void particle_32_update(uint32_t p);  // 0x00271df8
void particle_33_update(uint32_t p);  // 0x00271ec8
void particle_36_update(uint32_t p);  // 0x00272438
void particle_3a_update(uint32_t p);  // 0x00272bc0
void particle_3d_update(uint32_t p);  // 0x00273448
void particle_41_update(uint32_t p);  // 0x00273ee0
void particle_43_update(uint32_t p);  // 0x00274518
void particle_49_update(uint32_t p);  // 0x00275320
void particle_4b_update(uint32_t p);  // 0x002756f0
void particle_4c_update(uint32_t p);  // 0x00275810

}  // namespace rac1

#endif
