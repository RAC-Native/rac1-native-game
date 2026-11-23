#ifndef RAC1_GAME_OVL_METAL_OBJ_H
#define RAC1_GAME_OVL_METAL_OBJ_H
// metal_obj, ported by hand (metal_obj.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void metal_2d2a90_update(uint32_t m);  // orig: 0x002d2a90
void metal_obj_fns_register();  // orig: (none)

}  // namespace rac1
#endif
