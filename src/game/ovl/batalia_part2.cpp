// Batalia's part 2: its own draw-list callbacks registered for the indirect calls (the functions: batalia_part2_tr.cpp).
#include <game/ovl/batalia_part2.h>

#include <game/ovl/batalia_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/hud_slot7_tr.h>

namespace rac1 {

// orig: (none) -- registers the draw callbacks the classes' updates queue (the draw lists d880 / da80, appenders L8:0x20bdf0 / 0x20bfa8)
// and HUD slot 7's draw function (requested by hero_camera_7_update L8:0x218a00 with the Metal Detector owned)
void batalia_part2_register() {
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d3878), bat_class252_draw_a_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d4108), bat_class252_draw_b_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da0d0), bat_class327_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002deee0), bat_class440_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e5188), bat_class452_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f0dc8), bat_class600_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f5e58), bat_class671_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002fb948), bat_class832_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00304bc0), bat_class1135_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00309030), bat_class1649_draw_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030ab50), bat_sandmouse_glow_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00244ab0), hud_slot7_draw_tr);  // (HUD slot 7's draw: Veldin's port, the level's copy is the same code; its start / update are Veldin's too)
}

}  // namespace rac1
