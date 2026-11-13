// The sky-reflecting objects of Hoven and Oltanis (one draw callback each level: hov_cb_2e71b0 / olt_cb_307a28): their meshes
// drawn with two effect textures, the second's coordinates the view's reflection, sphere-mapped by the VU0 in macro mode.
// One port of the two levels' copies (shared_env_objects.cpp). Phase 1 of stage 4: the levels' generated copies still run;
// these are proved against them by oracle batch 517.
#ifndef RAC1_GAME_OVL_SHARED_ENV_OBJECTS_H
#define RAC1_GAME_OVL_SHARED_ENV_OBJECTS_H

#include <cstdint>

namespace rac1 {

// The reflection's (u, v) of `n` points `points` with the normals `normals` (packed 12-byte vectors) into `out`
// (doublewords), from the camera: VU0 macro code (its registers vf1 .. vf31 and Q left as the original leaves them).
void env_objects_uvs(int32_t n, uint32_t points, uint32_t normals, uint32_t out);  // L12:0x002667fc L14:0x0025602c
// The meshes `count` records of 0x20 bytes at `recs` (+0 a bounding sphere, +0x10 the points, +0x14 the texture
// coordinates, +0x18 the normals, +0x1c the point count): each on screen drawn white with the textures `tex0a` / `tex0b`.
void env_objects_draw(uint32_t recs, int32_t count, uint64_t tex0a, uint64_t tex0b);  // L12:0x002bc210 L14:0x002ab3e8
// The draw callbacks: the level's records, count and two effect textures.
void env_objects_callback(uint32_t m);  // L12:0x002e71b0 L14:0x00307a28

}  // namespace rac1

#endif
