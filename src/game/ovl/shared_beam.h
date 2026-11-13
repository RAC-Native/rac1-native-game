// The electric beams of Blarg, Orxon and Gemlik Base (class 1068's draw and sparks) and the shards of class 1085
// (shared_beam.cpp: one port for the three levels' copies). Phase 1 of stage 4: the levels' generated copies still run;
// these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_BEAM_H
#define RAC1_GAME_OVL_SHARED_BEAM_H

#include <cstdint>

namespace rac1 {

void beam_sparks(uint32_t m);        // L6:0x002ff978 L10:0x002dc990 L13:0x00309bc8
void beam_draw(uint32_t m);          // class 1068's draw (L6:0x002ff680 L10:0x002dc698 L13:0x003098d0)
void beam_shard_update(uint32_t m);  // class 1085 .. 1089 (L6:0x00300b90 L10:0x002dd010 L13:0x00303878)

}  // namespace rac1

#endif
