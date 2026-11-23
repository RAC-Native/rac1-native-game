#ifndef RAC1_GAME_OVL_TESLA_H
#define RAC1_GAME_OVL_TESLA_H
// tesla, ported by hand (tesla.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void tesla_2b9310(uint32_t p0);  // orig: 0x002b9310
void tesla_2b94d0(uint32_t p0);  // orig: 0x002b94d0
void tesla_update(uint32_t m);  // orig: 0x002b87e0
void tesla_fns_register();  // orig: (none)

}  // namespace rac1
#endif
