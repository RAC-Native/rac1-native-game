// Gaspar (level 9) own moby classes, class hooks and level-only engine functions: the declarations of the hand-written ports
// (gaspar_structure / _meteors / _breakables / _platform / _controller / _lift / _fuses / _turret / _hazard / _ferry / _lava /
// _carry.cpp) and of the one function still generated, the palette blend 0x26d240 (gaspar_classes_tr_4.cpp: `TrRet`, the
// uniform ABI; the lava pools' draw 0x2c1920 is the shared ocean_draw_one, aridia_ocean.h).
#ifndef RAC1_GAME_OVL_GASPAR_MOBYS_H
#define RAC1_GAME_OVL_GASPAR_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

uint32_t gas_2c2500_l9(uint32_t m, uint32_t at, uint32_t cls);  // L9:0x002c2500  a piece of the structure
uint32_t gas_2c2610_l9(uint32_t at, uint32_t cls, uint32_t out);  // L9:0x002c2610  the nearest moby of a class
void gas_class1182_update_l9(uint32_t a0);  // L9:0x002c26c8
uint32_t gas_2ea778_l9();  // L9:0x002ea778  a meteor
uint32_t gas_2eaa50_l9(uint32_t ctl);  // L9:0x002eaa50  a cloud on the controller's path
void gas_2ead30_l9(uint32_t m);  // L9:0x002ead30  a puff of smoke's update
uint32_t gas_2ea528_l9(uint32_t src, uint32_t vel);  // L9:0x002ea528  a puff of smoke off a meteor
uint32_t gas_2efe48_l9(uint32_t at, uint32_t dir, float lo, float hi);  // L9:0x002efe48  a splash of the lava (class 417)
void gas_2eaea8_l9(uint32_t m);  // L9:0x002eaea8  a meteor's update
void gas_class263_update_l9(uint32_t a0);  // L9:0x002eb970
uint32_t gas_2edb60_l9(uint32_t m, uint32_t cls);  // L9:0x002edb60  the next build of a breakable
uint32_t gas_chunk_l9(uint32_t src, uint32_t pos, uint32_t vel, uint32_t kind, float scale);  // a chunk (class 320 / 321), L9:0x002ef868
void gas_class276_update_l9(uint32_t a0);  // L9:0x002edc30
uint32_t gas_2ee380_l9(uint32_t m, uint32_t pos, float yaw);  // L9:0x002ee380  the remains (class 284)
void gas_class279_update_l9(uint32_t a0);  // L9:0x002ee110
void gas_class313_update_l9(uint32_t a0);  // L9:0x002ef430
void gas_class320_update_l9(uint32_t a0);  // L9:0x002ef9b0
void gas_class417_update_l9(uint32_t a0);  // L9:0x002f0040
void gas_class664_update_l9(uint32_t a0);  // L9:0x002f86a0
void gas_class1000_update_l9(uint32_t a0);  // L9:0x00300888
void gas_class1150_update_l9(uint32_t a0);  // L9:0x003033a0
void gas_303b30_l9(uint32_t m, uint32_t pv, uint32_t hs);  // L9:0x00303b30  class 1172's hits
void gas_class1172_update_l9(uint32_t a0);  // L9:0x00303d10
uint32_t gas_3041d0_l9(uint32_t m, uint32_t at, int32_t joint);  // L9:0x003041d0  the structures' part nearest a point
void gas_class1181_update_l9(uint32_t a0);  // L9:0x00304360
void gas_class1201_update_l9(uint32_t a0);  // L9:0x00304c80
void gas_class1206_update_l9(uint32_t a0);  // L9:0x00305a28
uint32_t gas_308220_l9(uint32_t m, uint32_t cls);  // L9:0x00308220  the next build of a class 1285
void gas_class1285_update_l9(uint32_t a0);  // L9:0x003082d8
void gas_class1293_update_l9(uint32_t a0);  // L9:0x003091b0
void gas_30a238_l9(uint32_t at, float from, float to, float k);  // L9:0x0030a238  an angle eased
int32_t gas_30a298_l9(uint32_t m);  // L9:0x0030a298  the raft's ride (1: arrived)
void gas_class1766_update_l9(uint32_t a0);  // L9:0x00309c08
void gas_mat_scale_l9(uint32_t out, float s);  // L9:0x0021f6f8  a scale matrix
void gas_21baf8_l9();  // L9:0x0021baf8  VU1 program 7's constants for the lava's streams
TrRet gas_26d240_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0026d240
void gas_21e560_l9(int32_t a, int32_t b, float blend);  // L9:0x0021e560  the streams' texture
void gas_21e8c0_l9(uint32_t recs, int32_t n, int32_t a, int32_t b, float blend);  // L9:0x0021e8c0  the lava's streams
void gas_2c1978_l9(uint32_t rec, uint32_t strips, int32_t n);  // L9:0x002c1978  the lava pools' edges
void gas_class317_draw_l9(uint32_t a0);  // L9:0x002ef750
int32_t gas_hook_2e2770_l9(uint32_t m, uint32_t a1, uint32_t a2, float f12);  // L9:0x002e2770  class 193's hook +0xc
uint32_t gas_hook_2e27a0_l9(uint32_t m);  // L9:0x002e27a0  class 193's hook +0x10
// (class 193's hooks +0 / +4 / +8, L9:0x002e2680 / 0x2e26d0 / 0x2e2720: shared_helpers.h's carry_state_hook)

}  // namespace rac1

#endif
