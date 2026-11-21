// Blarg's (Nebula G34's) two mesh classes drawn as quads by their draw callbacks: class 1062 (two meshes) and class 1083
// (four meshes, in two builds). One port of each build, serving its copies (shared_blarg_meshes.cpp).
#ifndef RAC1_GAME_OVL_SHARED_BLARG_MESHES_H
#define RAC1_GAME_OVL_SHARED_BLARG_MESHES_H

#include <cstdint>

namespace rac1 {

// Class 1083's meshes: the first two (64 quads each, all drawn), the last two (96 quads, those facing the camera).
void blarg_class1083_draw_a(uint32_t m);  // L6:0x003003f0
void blarg_class1083_draw_b(uint32_t m);  // L6:0x00300588
void blarg_class1083_draw_c(uint32_t m);  // L6:0x00300720
void blarg_class1083_draw_d(uint32_t m);  // L6:0x00300958
// Class 1062's meshes (58 and 864 quads, those facing the camera, each drawn twice: with a drifting texture and with
// effect texture 0x2a).
void blarg_class1062_draw_b(uint32_t m);  // L6:0x002fd460
void blarg_class1062_draw_c(uint32_t m);  // L6:0x002fd748

}  // namespace rac1

#endif
