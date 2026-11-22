// Batalia's part 2: its own draw-list callbacks registered for the indirect calls (the functions: batalia_draws / the classes' own
// files, and the shared ports shared_*.cpp for the draws several levels carry).
#include <game/ovl/batalia_part2.h>

#include <game/ovl/batalia_mobys.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/hud_slot7_tr.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class252.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/shared_sea_draw.h>

namespace rac1 {

// orig: (none) -- registers the draw callbacks the classes' updates queue (the draw lists d880 / da80, appenders L8:0x20bdf0 / 0x20bfa8)
// and HUD slot 7's draw function (requested by hero_camera_7_update L8:0x218a00 with the Metal Detector owned)
void batalia_part2_register() {
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d3878), class252_draw_beam);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d4108), class252_draw_glow);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da0d0), sea_draw);  // class 327's sea (shared_sea_draw.cpp)
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002deee0), bat_class440_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e5188), bat_class452_draw_l8);
    // classes 600 and 1649, the scrolling layers (shared_scroll.cpp; their set-up L8:0x20f250 is Veldin's shadows_vu1_setup, the
    // meshes' offsets uv_offset_pairs in the VU0 as the original's 0x258090)
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f0dc8), class600_draw);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f5e58), bat_class671_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002fb948), class832_draw);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00304bc0), class1135_draw);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00309030), class1649_draw);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030ab50), sandmouse_glow);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00244ab0), hud_slot7_draw_tr);  // (HUD slot 7's draw: Veldin's port, the level's copy is the same code; its start / update are Veldin's too)
}

}  // namespace rac1
