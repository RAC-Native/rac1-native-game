#ifndef RAC1_GAME_OVL_DEV_271450_H
#define RAC1_GAME_OVL_DEV_271450_H
// dev_271450, ported by hand (dev_271450.cpp). Addresses are Veldin's overlay addresses (the shared engine).
#include <cstdint>

namespace rac1 {

void dev_271450_1ff318(uint32_t p0, uint32_t p1, float pf12);  // orig: 0x001ff318
void devastator_271450_update(uint32_t m);  // orig: 0x00271450
void dev_271450_fns_register();  // orig: (none)

}  // namespace rac1
#endif
