// Novalis's scrolling surfaces (waterfalls / energy strips: classes 676, 678, 761, 1225) and its water (class 751), drawn by
// Novalis's code through VU1 program 7 (the shadow program the base ELF's 0x10e810 uploads), ported by hand in
// novalis_water.cpp. The water's code is the same on Rilgar, Umbris, Pokitaru, Hoven and Gemlik: its ports serve them too.
#ifndef RAC1_GAME_OVL_NOVALIS_WATER_H
#define RAC1_GAME_OVL_NOVALIS_WATER_H

#include <cstdint>

namespace rac1 {

// The draw callbacks the classes' updates queue (sub_001f93e0): VU1 program 7 set up (shadows_vu1_setup), then their strips
// (the records at 0x1e2fc0 x 8, 0x1e3380 x 2, 0x1fbc80 x 4, 0x202d80 x 3); class 1225's then sets its strips' wave height
// from the frame counter. Class 751's draws the 21 water tiles at 0x1e34c0 (the same callback on Rilgar, Umbris, Pokitaru, Hoven
// and Gemlik draws each level's own tiles).
void nov_strips_676_draw_l1(uint32_t m);    // orig: L1:0x002f60f8
void nov_strips_678_draw_l1(uint32_t m);    // orig: L1:0x002f6150
void nov_strips_761_draw_l1(uint32_t m);    // orig: L1:0x002feb28
void nov_strips_1225_draw_l1(uint32_t m);   // orig: L1:0x00309bf8
void nov_water_751_draw_l1(uint32_t m);     // orig: L1:0x002fd0c0 (L5:0x0030d678 L7:0x0030cd20 L11:0x0030e708 L12:0x002bf748 L13:0x0030a460)
// The strips' VU1 set-up: shadows_vu1_setup (0x21e340 = Veldin's 0x1fc740)
void nov_strips_vu7_setup_l1();             // orig: L1:0x002b96c0
// The water's VU1 set-up (0x1fc740's twin without the Z bias): program 7 uploaded when not loaded, the two view matrices x 1024
// about the camera, the fans' tag, vf29 / vf30 / the fog clamp, MSCALF 0, the fixed GS packet; the GS state cache reset
void nov_water_vu7_setup_l1();              // orig: L1:0x002b8f98 (L5:0x002cd960 L7:0x002cbed0 L11:0x002c8108 L12:0x002bba98 L13:0x002af8b8)

// Strip vertices, water height/normal/lighting grid, UV scroll, and VU1 program 7 packet builders
// (prim_list2 and the water's functions below are shared: the other levels' copies are the same code, every address
// the engine's; their addresses on each level are listed after the declarations)
void nov_prim_list2_l1(int32_t n, uint32_t xyz, uint32_t rgba, uint32_t st, uint32_t st2, uint32_t clip);  // orig: L1:0x0021fa98 (L9:0x0021d250 L12:0x00220a58 L14:0x00211128)
void nov_strip_verts_l1(uint32_t rec);                       // orig: L1:0x00262618
void nov_strips_draw_l1(int32_t n, uint32_t recs);           // orig: L1:0x002b96e0
void nov_water_grid_in_l1(uint32_t grid, uint32_t tile);     // orig: L1:0x00261c10 (L5:0x00276bc0 L7:0x00275380 L11:0x00272fa8 L12:0x00265de8 L13:0x00258e78)
// The water grid's step (the tiles' records `tiles`, 0x1190 bytes: nov_water_grid_in_l1 / _wave / _step / _blend / _update,
// novalis_water.cpp): one wave step of a tile into its block `buf`; the step of `n` tiles; a block blended between two by
// `t`; the water's frame (a step, or the blend by the phase)
void nov_water_grid_wave_l1(uint32_t grid, int32_t tile, int32_t buf, uint32_t tiles);  // L1:0x00262038 L5:0x00276fe8 L7:0x002757a8 L11:0x002733d0 L12:0x00266210 L13:0x002592a0
void nov_water_grid_step_l1(uint32_t tiles, int32_t n);       // L1:0x002b7f30 L5:0x002cc910 L7:0x002cae68 L11:0x002c70b8 L12:0x002baa48 L13:0x002ae850
void nov_water_grid_blend_l1(uint32_t out, uint32_t a, uint32_t b, float t);  // L1:0x00261d78 L5:0x00276d28 L7:0x002754e8 L11:0x00273110 L12:0x00265f50 L13:0x00258fe0
void nov_water_grid_update_l1(uint32_t tiles, int32_t n);     // L1:0x002b7fe0 L5:0x002cc9c0 L7:0x002caf18 L11:0x002c7168 L12:0x002baaf8 L13:0x002ae900
void nov_water_grid_light_l1(uint32_t tile);                 // orig: L1:0x00261df8 (L5:0x00276da8 L7:0x00275568 L11:0x00273190 L12:0x00265fd0 L13:0x00259060)
void nov_water_grid_xyz_l1(uint32_t tile);                   // orig: L1:0x002b8c08 (L5:0x002cd5d0 L7:0x002cbb40 L11:0x002c7d78 L12:0x002bb708 L13:0x002af528)
void nov_water_uv_scroll_l1(uint32_t tile);                  // orig: L1:0x002b80d0 (L5:0x002ccab0 L7:0x002cb008 L11:0x002c7258 L12:0x002babe8 L13:0x002ae9f0)
void nov_water_cell_packet_l1(uint32_t mscal, int32_t cell); // orig: L1:0x00262388 (L5:0x00277338 L7:0x00275af8 L11:0x00273720 L12:0x00266560 L13:0x002595f0)
void nov_water_cell_l1(uint32_t tile, int32_t cell, int32_t clip);  // orig: L1:0x002b8cb0 (L5:0x002cd678 L7:0x002cbbe8 L11:0x002c7e20 L12:0x002bb7b0 L13:0x002af5d0)
void nov_water_draw_l1(uint32_t tiles, int32_t n);           // orig: L1:0x002b91c8 (L5:0x002cdb90 L7:0x002cc100 L11:0x002c8338 L12:0x002bbcc8 L13:0x002afae8)

// orig: (none) — registers the draw callbacks for the indirect calls (ovl_call_moby_fn)
void novalis_water_fns_register();

}  // namespace rac1

#endif
