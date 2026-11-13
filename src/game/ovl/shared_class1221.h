// Classes 1221 and 1222 (Kerwan, Umbris, Hoven, Quartu: one update for both), shared: one port of the four levels' copies
// (shared_class1221.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1221_H
#define RAC1_GAME_OVL_SHARED_CLASS1221_H

#include <cstdint>

namespace rac1 {

// A falling, spinning piece that fades over its life and leaves when its life is out.
void class1221_update(uint32_t m);  // L3:0x002de370 L3:0x002de458 L7:0x0031d9f8 L7:0x0031dae0 L12:0x00301d08 L12:0x00301df0 L15:0x002e5e70 L15:0x002e5f58

}  // namespace rac1

#endif
