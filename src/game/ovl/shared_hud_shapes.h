// Screen-space shapes the space combat's HUD (Pokitaru, Gemlik Base, Drek's fleet) draws straight into the frame's packet
// (the cursor 0x1611c0): a line strip from a table of points, the same turned, a textured quad turned about a corner, a
// ring of four such quads, and a point of the world on the screen (shared_hud_shapes.cpp). One port of the levels'
// copies. Phase 1 of stage 4: the levels' generated copies still run; these are proved against them by oracle batch 504.
#ifndef RAC1_GAME_OVL_SHARED_HUD_SHAPES_H
#define RAC1_GAME_OVL_SHARED_HUD_SHAPES_H

#include <cstdint>

namespace rac1 {

// A line strip of `n` points (halfword pairs at `points`) scaled by `scale` about (x, y), at the depth `z`, in `rgba` (the
// whole register: its upper word goes to the packet too).
void hud_line_strip(uint32_t points, int32_t n, uint32_t z, uint64_t rgba, float x, float y, float scale);  // L11:0x00310ad0 L13:0x002b7b30 L17:0x002e9c28
// The same turned by `angle` (about the screen's normal) before the scale.
void hud_line_strip_turned(uint32_t points, int32_t n, uint32_t z, uint64_t rgba, float x, float y, float scale, float angle);  // L13:0x002b7d50 L17:0x002e9e48
// A textured quad from its corner (x, y) along `h` turned by `angle` and `w` across, its texture (`tex0`) coordinates 0 ..
// (`u`, `v`), at the depth `z`, in the colour (r, g, b, a).
void hud_quad_turned(uint32_t u, uint32_t v, uint64_t tex0, uint32_t z, uint32_t r, uint32_t g, uint32_t b, uint32_t a,
                     float x, float y, float w, float h, float angle);  // L11:0x00310cf8 L13:0x002b7fd0 L17:0x002ea0c8
// A ring of four such quads (the level's effect texture: Pokitaru's 0x42, Gemlik Base's 0x37, Drek's fleet's 0x2e) about
// (x, y), 20 x `size` across, the first at `angle`, each next a quarter turn on.
void hud_ring(uint32_t r, uint32_t g, uint32_t b, uint32_t a, float x, float y, float size, float angle);  // L11:0x00311048 L13:0x002b8320 L17:0x002ea418
// The point `pos` of the world on the screen in whole pixels about its corner: x to `out_x`, y + `add_y` to `out_y`.
void hud_screen_point(uint32_t pos, uint32_t out_x, uint32_t out_y, int32_t add_y);  // L11:0x00311210 L13:0x002b84e8 L17:0x002ea5e0

}  // namespace rac1

#endif
