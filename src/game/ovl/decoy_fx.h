#ifndef RAC1_GAME_OVL_DECOY_FX_H
#define RAC1_GAME_OVL_DECOY_FX_H
// decoy_fx, ported by hand (decoy_fx.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void decoy_2c31c0_update(uint32_t m);  // orig: 0x002c31c0
void decoy_fx_fns_register();  // orig: (none)

}  // namespace rac1
#endif
