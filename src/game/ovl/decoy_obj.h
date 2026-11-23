#ifndef RAC1_GAME_OVL_DECOY_OBJ_H
#define RAC1_GAME_OVL_DECOY_OBJ_H
// decoy_obj, ported by hand (decoy_obj.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void decoy_obj_2c3440(uint32_t p0);  // orig: 0x002c3440
void decoy_obj_2c4328(uint32_t p0);  // orig: 0x002c4328
int32_t decoy_obj_2c3af8(uint32_t p0, uint32_t p1, uint32_t p2);  // orig: 0x002c3af8
void decoy_obj_2c40a0(uint32_t p0);  // orig: 0x002c40a0
void decoy_object_update(uint32_t m);  // orig: 0x002c4380
void decoy_obj_fns_register();  // orig: (none)

}  // namespace rac1
#endif
