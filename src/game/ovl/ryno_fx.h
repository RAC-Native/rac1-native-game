#ifndef RAC1_GAME_OVL_RYNO_FX_H
#define RAC1_GAME_OVL_RYNO_FX_H
// ryno_fx, ported by hand (ryno_fx.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void ryno_2cd1f0_update(uint32_t m);  // orig: 0x002cd1f0
void ryno_fx_fns_register();  // orig: (none)

}  // namespace rac1
#endif
