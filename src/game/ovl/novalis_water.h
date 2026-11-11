// Novalis's scrolling surfaces (waterfalls / energy strips: classes 676, 678, 761, 1225) and its water (class 751), drawn by
// Novalis-only code through VU1 program 7 (the shadow program the base ELF's 0x10e810 uploads), ported by hand in
// novalis_water.cpp.
#ifndef RAC1_GAME_OVL_NOVALIS_WATER_H
#define RAC1_GAME_OVL_NOVALIS_WATER_H

#include <cstdint>

namespace rac1 {

// The draw callbacks the classes' updates queue (sub_001f93e0): VU1 program 7 set up (shadows_vu1_setup), then their strips
// (the records at 0x1e2fc0 x 8, 0x1e3380 x 2, 0x1fbc80 x 4, 0x202d80 x 3); class 1225's then sets its strips' wave height
// from the frame counter. Class 751's draws the 21 water tiles at 0x1e34c0.
void nov_strips_676_draw_l1(uint32_t m);    // orig: L1:0x002f60f8
void nov_strips_678_draw_l1(uint32_t m);    // orig: L1:0x002f6150
void nov_strips_761_draw_l1(uint32_t m);    // orig: L1:0x002feb28
void nov_strips_1225_draw_l1(uint32_t m);   // orig: L1:0x00309bf8
void nov_water_751_draw_l1(uint32_t m);     // orig: L1:0x002fd0c0
// The strips' VU1 set-up: shadows_vu1_setup (0x21e340 = Veldin's 0x1fc740)
void nov_strips_vu7_setup_l1();             // orig: L1:0x002b96c0
// The water's VU1 set-up (0x1fc740's twin without the Z bias): program 7 uploaded when not loaded, the two view matrices x 1024
// about the camera, the fans' tag, vf29 / vf30 / the fog clamp, MSCALF 0, the fixed GS packet; the GS state cache reset
void nov_water_vu7_setup_l1();              // orig: L1:0x002b8f98

// Strip vertices, water height/normal/lighting grid, UV scroll, and VU1 program 7 packet builders
void nov_prim_list2_l1(int32_t n, uint32_t xyz, uint32_t rgba, uint32_t st, uint32_t st2, uint32_t clip);  // orig: L1:0x0021fa98
void nov_strip_verts_l1(uint32_t rec);                       // orig: L1:0x00262618
void nov_strips_draw_l1(int32_t n, uint32_t recs);           // orig: L1:0x002b96e0
void nov_water_grid_in_l1(uint32_t grid, uint32_t tile);     // orig: L1:0x00261c10
void nov_water_grid_light_l1(uint32_t tile);                 // orig: L1:0x00261df8
void nov_water_grid_xyz_l1(uint32_t tile);                   // orig: L1:0x002b8c08
void nov_water_uv_scroll_l1(uint32_t tile);                  // orig: L1:0x002b80d0
void nov_water_cell_packet_l1(uint32_t mscal, int32_t cell); // orig: L1:0x00262388
void nov_water_cell_l1(uint32_t tile, int32_t cell, int32_t clip);  // orig: L1:0x002b8cb0
void nov_water_draw_l1(uint32_t tiles, int32_t n);           // orig: L1:0x002b91c8

// orig: (none) — registers the draw callbacks for the indirect calls (ovl_call_moby_fn)
void novalis_water_fns_register();

}  // namespace rac1

#endif
