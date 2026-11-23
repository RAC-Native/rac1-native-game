// Breakable props of Blarg and Umbris (classes 1655, 1660 and 1664 of Blarg, 1650 and 1663 of Umbris): hit, they break into
// debris and leave their broken piece (shared_breakables.cpp: one port of the five copies, the same code bound to each
// class's debris and piece). Every level with a copy runs these ports; they are proved against each level's own copy by
// oracle batch 519.
#ifndef RAC1_GAME_OVL_SHARED_BREAKABLES_H
#define RAC1_GAME_OVL_SHARED_BREAKABLES_H

#include <cstdint>

namespace rac1 {

// Three debris classes and the broken piece's class: L6:0x00309860 (class 1655), L7:0x0031f3d8 (1650), L7:0x0031f5b0 (1663)
void class1655_update(uint32_t m);
void class1650_update(uint32_t m);
void class1663_update(uint32_t m);
// One debris class (two of it) and the broken piece's class: L6:0x00309e08 (class 1660), L6:0x00309fa8 (1664)
void class1660_update(uint32_t m);
void class1664_update(uint32_t m);

}  // namespace rac1

#endif
