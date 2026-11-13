// Aridia's ocean (class 854's draw callback), background ship glows (classes 1212, 1213), and Rocket Launcher rocket
// (class 1208), ported by hand in aridia_ocean.cpp. The ocean draws through VU1 program 7 (the shadow program), its
// prim lists rendered by the MRP (mrp_fx.h).
#ifndef RAC1_GAME_OVL_ARIDIA_OCEAN_H
#define RAC1_GAME_OVL_ARIDIA_OCEAN_H

#include <cstdint>

namespace rac1 {

// Aridia ocean, ship glows, and class 1208 update (aridia_ocean.cpp)
void ari_ocean_draw_tr_l2(uint32_t m);  // orig: L2:0x002ea048
void ari_ocean_setup_l2(uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t h0, uint32_t h1, uint32_t tex, float f12,
                        float f13, float f14, float f15);  // orig: L2:0x002a4818
void ari_ocean_vu7_setup_l2(uint32_t rec);                 // orig: L2:0x002a40f0
void ari_spr_dma_to_l2(uint32_t madr, uint32_t sadr, uint32_t qwc);  // orig: L2:0x002101b8
void ari_ocean_end_l2();                                   // orig: L2:0x002a4880
void ari_ocean_end_body_l2(uint32_t buf);                  // orig: L2:0x002a46e0
void ari_ocean_record_l2(uint32_t rec);                    // orig: L2:0x002a48a0
void ari_ocean_cells_clip_l2(uint32_t rec);                // orig: L2:0x0024f748
void ari_ocean_cells_draw_l2(uint32_t rec);                // orig: L2:0x0024fa80
void ari_glow_quad_l2(uint32_t pos, uint32_t rgba, float size, float fade);  // orig: L2:0x00264b50
// (the two ship glows are shared: the other levels' copies are the same code, listed after the declarations)
void ari_bg_ship1_glow_draw_l2(uint32_t m);                // orig: L2:0x002ec308 (L9:0x00305c10)
void ari_bg_ship2_glow_draw_l2(uint32_t m);                // orig: L2:0x002ecc68 (L9:0x00306570 L10:0x002eb958)
void ari_class1208_update_l2(uint32_t m);                  // orig: L2:0x002ebf20 (the Rocket Launcher's rocket)

// orig: (host) the ocean's draw callback as the engine calls it: L2:0x2ea048 inside the MRP's prim-list scope (mrp_fx.h)
void ari_ocean_draw_l2(uint32_t m);

// orig: (none) -- registers the ocean's / glows' functions for the indirect calls (ovl_call_moby_fn)
void aridia_ocean_fns_register();
void ari_ocean_tex_upload_l2(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, float pf12);  // orig: L2:0x0024fca0

}  // namespace rac1

#endif
