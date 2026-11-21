// Class 1068 of Blarg and Orxon, a walking robot that throws an electric beam: its update and the helpers of its own (the
// frame's common part, the beam's start and run), shared by the two levels that carry it: one port of the levels' copies
// (shared_class1068.cpp). The beam's draw and sparks and the shards it breaks into are in shared_beam.h.
#ifndef RAC1_GAME_OVL_SHARED_CLASS1068_H
#define RAC1_GAME_OVL_SHARED_CLASS1068_H

#include <cstdint>

namespace rac1 {

void class1068_update(uint32_t m);      // L6:0x002fdbd0 L10:0x002dabe8
void class1068_think(uint32_t m);       // L6:0x002feb40 L10:0x002dbb58 (the frame's common part)
void class1068_beam_run(uint32_t m);    // L6:0x002ff2c8 L10:0x002dc2e0
void class1068_beam_start(uint32_t m);  // L6:0x002ff100 L10:0x002dc118

}  // namespace rac1

#endif
