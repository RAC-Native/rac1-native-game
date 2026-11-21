// Kalebo III's environment-mapped objects (classes 1943 / 1948 / 1953: shared_envmap.cpp, one port for the level's three
// copies of their code). Phase 1 of stage 4: the generated copies still run; these are proved against them by batch 510.
#ifndef RAC1_GAME_OVL_SHARED_ENVMAP_H
#define RAC1_GAME_OVL_SHARED_ENVMAP_H

#include <cstdint>

namespace rac1 {

// the sphere-map coordinates (u, v pairs into `out`) of `n` points `verts` with the normals `normals` (both packed 12-byte
// vectors), seen from the camera (L16:0x002e8b98 / 0x002e90c8 / 0x002e95f8 / 0x002e9b38 / 0x002e9e40)
void envmap_uvs(int32_t n, uint32_t normals, uint32_t verts, uint32_t out);
void envmap_1948_update(uint32_t m);  // class 1948 (L16:0x002e98e0)
void envmap_1953_update(uint32_t m);  // class 1953 (L16:0x002ea128)
void envmap_1943_draw(uint32_t m);    // class 1943's draw callback (L16:0x002e8d40)
void envmap_1948_draw(uint32_t m);    // class 1948's (L16:0x002e97a0)
void envmap_1953_draw(uint32_t m);    // class 1953's (L16:0x002e9fe8)

}  // namespace rac1

#endif
