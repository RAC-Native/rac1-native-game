// Class 1302 of Blarg and Orxon, the Gadgebots' entrance: its update (a sign counting the Gadgebots still to come, which
// opens the entrance's mobys when none is left) and its draw callback (a glowing sign: four quads of the level's mesh turned
// toward the camera, their texture scrolling, and a growing ring of light over them), shared by the two levels that carry
// it: one port of the levels' copies (shared_class1302.cpp).
#ifndef RAC1_GAME_OVL_SHARED_CLASS1302_H
#define RAC1_GAME_OVL_SHARED_CLASS1302_H

#include <cstdint>

namespace rac1 {

void class1302_update(uint32_t m);              // L6:0x00307d30 L10:0x002e7cd8
void class1302_open(uint32_t m, uint32_t o);    // L6:0x00307c48 L10:0x002e7bf0 (one of the entrance's mobys `o`)
void class1302_draw(uint32_t m);                // L6:0x00307578 L10:0x002e7520

}  // namespace rac1

#endif
