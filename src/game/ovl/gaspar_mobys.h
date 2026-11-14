// Gaspar (level 9) own moby classes, class hooks and level-only engine functions: the declarations of the hand-written ports
// (gaspar_structure / _meteors / _breakables / _platform.cpp) and of gaspar_classes_tr_N.cpp
// (generated: `void` = a class update / entry in the entry ABI, `TrRet` = a helper or a `_u` entry in the uniform ABI).
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
TrRet gas_2ef868_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002ef868  (the same, the generated ABI: class 1285 calls it)
void gas_class276_update_l9(uint32_t a0);  // L9:0x002edc30
uint32_t gas_2ee380_l9(uint32_t m, uint32_t pos, float yaw);  // L9:0x002ee380  the remains (class 284)
void gas_class279_update_l9(uint32_t a0);  // L9:0x002ee110
void gas_class313_update_l9(uint32_t a0);  // L9:0x002ef430
void gas_class320_update_l9(uint32_t a0);  // L9:0x002ef9b0
void gas_class417_update_l9(uint32_t a0);  // L9:0x002f0040
void gas_class664_update_l9(uint32_t a0);  // L9:0x002f86a0
void gas_class1000_update_l9(uint32_t a0);  // L9:0x00300888
void gas_class1150_update_l9(uint32_t a0);  // L9:0x003033a0
TrRet gas_303b30_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x00303b30
void gas_class1172_update_l9(uint32_t a0);  // L9:0x00303d10
TrRet gas_3041d0_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x003041d0
void gas_class1181_update_l9(uint32_t a0);  // L9:0x00304360
void gas_class1201_update_l9(uint32_t a0);  // L9:0x00304c80
void gas_class1206_update_l9(uint32_t a0);  // L9:0x00305a28
TrRet gas_308220_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x00308220
void gas_class1285_update_l9(uint32_t a0);  // L9:0x003082d8
void gas_class1293_update_l9(uint32_t a0);  // L9:0x003091b0
TrRet gas_30a238_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0030a238
TrRet gas_30a298_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0030a298
void gas_class1766_update_l9(uint32_t a0);  // L9:0x00309c08
TrRet gas_21f6f8_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0021f6f8
TrRet gas_21baf8_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0021baf8
TrRet gas_26d240_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0026d240
TrRet gas_21e560_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0021e560
TrRet gas_21e8c0_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x0021e8c0
TrRet gas_2c1220_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002c1220
TrRet gas_2c1920_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002c1920
TrRet gas_2c1978_l9(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002c1978
void gas_class317_draw_l9(uint32_t a0);  // L9:0x002ef750
TrRet gas_hook_2e2680_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002e2680
TrRet gas_hook_2e26d0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002e26d0
TrRet gas_hook_2e2720_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002e2720
TrRet gas_hook_2e2770_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002e2770
TrRet gas_hook_2e27a0_u(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L9:0x002e27a0

}  // namespace rac1

#endif
