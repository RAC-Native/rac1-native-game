// Umbris's part 2: its own draw callbacks and particle type 0x50 registered for the indirect calls (the functions:
// the shared hand-written ports of the copies several levels carry; the seas' draws of classes 460 / 1018: shared_sea_draw.cpp).
#include <game/ovl/umbris_part2.h>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/shared_boss_bar.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/shared_sea_draw.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

// orig: (none) -- registers the draw callbacks (queued by the classes' updates through 0x2211c8 / 0x221290 / 0x2213d0) and the
// particle update of type 0x50 (the table entry 0x140 the particle table's setup L7:0x290bfc writes)
void umbris_part2_register() {
    // the seas of classes 460 and 1018 (the level's two wave records through ocean_draw_one, aridia_ocean.cpp)
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x002f8038u), sea_draw);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x003048b0u), class823_draw);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00304c40u), class832_draw);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030cd20u), nov_water_751_draw_l1);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030d420u), sea_draw_2);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00312420u), umb_class1106_draw_a_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00312dc8u), umb_class1106_draw_b_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x003131b0u), umb_class1106_draw_c_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00317910u), umb_draw_317910_l7);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0031ce40u), class1135_draw);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0029e4a0u), beam_particle_update);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x00262078u), boss_health_bar_draw);
    ovl_register_level_moby_fn(UMBRIS, L7_ADDR(0x0030dcf0u), umb_class1046_draw_l7);
}

}  // namespace rac1
