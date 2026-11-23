#ifndef RAC1_GAME_OVL_MORPH_FX_H
#define RAC1_GAME_OVL_MORPH_FX_H
// morph_fx, ported by hand (morph_fx.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void morph_275aa0_update(uint32_t m);  // orig: 0x00275aa0
void morph_fx_fns_register();  // orig: (none)

}  // namespace rac1
#endif
