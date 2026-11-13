// Class 1443 (class 1890 on Veldin), a target of spinning rings: it spins and hums until every moby of its group has been
// hit, then goes quiet (the group's nearest member sounds) until they recover; and its draw callback, the rings as quads; one
// port of the two levels' copies (shared_class1443.cpp). Phase 1 of stage 4: the levels' generated copies still run; this is
// proved against them by oracle batch 505.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1443_H
#define RAC1_GAME_OVL_SHARED_CLASS1443_H

#include <cstdint>

namespace rac1 {

void class1443_update(uint32_t m);  // L16:0x002e5708 L18:0x002fae48
void class1443_draw(uint32_t m);    // L16:0x002e58d8 L18:0x002fb018
// 1 when every moby of the group `group` has its +0xbc set (0 for a group without a list)
int32_t class1443_group_all_hit(int32_t group);  // L16:0x002e5cd0 L18:0x002fb410
// the sound 0 of the group's member nearest the hero (members of class 0x5a3 left out)
void class1443_group_sound(int32_t group);  // L16:0x002e5d30 L18:0x002fb470

}  // namespace rac1

#endif
