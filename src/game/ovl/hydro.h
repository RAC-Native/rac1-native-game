#ifndef RAC1_GAME_OVL_HYDRO_H
#define RAC1_GAME_OVL_HYDRO_H
// hydro, ported by hand (hydro.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

int32_t hydro_2e02e0();  // orig: 0x002e02e0
void hydro_1eb448();  // orig: 0x001eb448
void hydro_2eac18(uint32_t p0);  // orig: 0x002eac18
void hydro_2e01b0(uint32_t p0);  // orig: 0x002e01b0
void hydro_2e0340(uint32_t p0);  // orig: 0x002e0340
void hydro_update(uint32_t m);  // orig: 0x002df9a0
void hydro_fns_register();  // orig: (none)

}  // namespace rac1
#endif
