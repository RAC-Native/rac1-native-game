// Particle types 0x43's (Veldin's 0x274518), 0x30's (0x271c88) and 0x32's (0x271df8) updates, 0x33's (0x271ec8), 0x3d's
// (0x273448), 0x41's (0x273ee0) and 0x3a's (0x272bc0) (met on Umbris), 0x31's (0x271d20: the bullets of Kerwan's helicopter, issue
// #26), 0x0a's (0x26a880), 0x0e's (0x26ba68), 0x27's (0x26fc38), 0x49's (0x275320), 0x1f's (0x26eb88: Giant Clank's streaks, #51) and
// 0x50's (0x275eb0: Drek's fleet's laser beams, #53) (spawned by ported level code: see veldin_extras.cpp), and the 17 types left
// (0x03 .. 0x4c: no code of any level spawns them, but each level's table lists them; 0x2a's spawner 0x270758 is its own update's),
// transliterated: the declarations of particle_types_tr.cpp (regen group veldin_particles).
#ifndef RAC1_GAME_OVL_PARTICLE_TYPES_TR_H
#define RAC1_GAME_OVL_PARTICLE_TYPES_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

TrRet ovl_259430_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x00259430
TrRet ovl_2743a8_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x002743a8
TrRet ovl_270758_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // 0x00270758 (type 0x2a's spawner)
void particle_43_update(uint32_t a0);  // 0x00274518
void particle_30_update(uint32_t a0);  // 0x00271c88
void particle_32_update(uint32_t a0);  // 0x00271df8
void particle_33_update(uint32_t a0);  // 0x00271ec8
void particle_3d_update(uint32_t a0);  // 0x00273448
void particle_41_update(uint32_t a0);  // 0x00273ee0
void particle_3a_update(uint32_t a0);  // 0x00272bc0
void particle_31_update(uint32_t a0);  // 0x00271d20
void particle_0a_update(uint32_t a0);  // 0x0026a880
void particle_0e_update(uint32_t a0);  // 0x0026ba68
void particle_27_update(uint32_t a0);  // 0x0026fc38
void particle_49_update(uint32_t a0);  // 0x00275320
void particle_1f_update(uint32_t a0);  // 0x0026eb88
void particle_50_update(uint32_t a0);  // 0x00275eb0
void particle_03_update(uint32_t a0);  // 0x002694f8
void particle_07_update(uint32_t a0);  // 0x0026a418
void particle_09_update(uint32_t a0);  // 0x0026a6f0
void particle_14_update(uint32_t a0);  // 0x0026c860
void particle_18_update(uint32_t a0);  // 0x0026d470
void particle_1d_update(uint32_t a0);  // 0x0026e3a8
void particle_1e_update(uint32_t a0);  // 0x0026e670
void particle_21_update(uint32_t a0);  // 0x0026f048
void particle_24_update(uint32_t a0);  // 0x0026f898
void particle_25_update(uint32_t a0);  // 0x0026fa00
void particle_26_update(uint32_t a0);  // 0x0026fb00
void particle_2a_update(uint32_t a0);  // 0x00270948
void particle_36_update(uint32_t a0);  // 0x00272438
void particle_3f_update(uint32_t a0);  // 0x002738b0
void particle_4b_update(uint32_t a0);  // 0x002756f0
void particle_4c_update(uint32_t a0);  // 0x00275810

}  // namespace rac1

#endif
