// The scrolling textures of Batalia's, Pokitaru's and Hoven's meshes (water, lava, energy: shared_scroll.cpp, one port for
// every copy; the copies differ only in the level tables they work on, numbered as below). Phase 1 of stage 4: the levels'
// generated copies still run; these are proved against them by oracle batch 510.
#ifndef RAC1_GAME_OVL_SHARED_SCROLL_H
#define RAC1_GAME_OVL_SHARED_SCROLL_H

#include <cstdint>

namespace rac1 {

// `n` texture coordinates (s, t pairs) from `src` to `dst` moved by the offset at `offset` (L8:0x00258090 L11:0x002739c0 L12:0x002668d4)
void uv_offset_pairs(uint32_t dst, uint32_t src, int32_t n, uint32_t offset);
// One set of meshes drawn with their texture moved by the set's offset `index` (sets: 0 L8:0x002f0c18, 1 L8:0x002f0cf0,
// 2 L8:0x00308e90, 3 L11:0x0031d9a0, 4 L11:0x0031da78, 5 L11:0x0031e780, 6 L11:0x0031e858, 7 L12:0x0030bd90).
void uv_scroll_meshes(int32_t set, uint32_t index);
// The offset `index` moved by its speed this frame, kept in -1 .. 1 (sets: 0 L8:0x002f1378, 1 L8:0x002f1460, 2 L8:0x003095e0,
// 3 L8:0x003096c8, 4 L11:0x0031e120, 5 L11:0x0031e208, 6 L11:0x0031ef80, 7 L11:0x0031f068, 8 L8:0x002f12a0, 9 L8:0x00309508,
// 10 L11:0x0031e048, 11 L11:0x0031eea8).
void uv_scroll_step(int32_t set, uint32_t index);
// the meshes' colours (`meshes` tables at `colours`, `counts` each) scaled by r, g, b, a; `full`: the colour 255 for r, g, b
// (L8:0x002f0f68 / 0x003091d0, L11:0x0031dd10 / 0x0031eb70, L12:0x0030bba0)
void mesh_colours_scale(uint32_t colours, int32_t meshes, uint32_t counts, uint32_t full, float r, float g, float b, float a);
// a wave: the phases at `phase` moved by the speeds `su` / `sv` (kept in -1 .. 1), `out` the sines of the phases (times pi)
// times `amp` (L8:0x002f1158 / 0x003093c0, L11:0x0031df00 / 0x0031ed60)
void wave_step(uint32_t out, uint32_t phase, float su, float sv, float amp);

}  // namespace rac1

#endif
